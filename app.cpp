#include "app.hpp"
#include "about/about.hpp"
#include "scorecard/scorecard.hpp"
#include "settings/settings.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include <storage/storage.h>
#include <furi_hal_rtc.h>
#include <datetime/datetime.h>
#include <furi/core/string.h>

namespace
{
    constexpr uint8_t StateVersion = 2;
    constexpr const char *StateFileName = "state.bin";
    constexpr const char *HistoryFileName = "rounds.csv";

    struct PersistentStateV1
    {
        uint8_t version = 0;
        uint8_t playerCount = 1;
        uint8_t holeCount = 9;
        uint8_t reserved = 0;
        std::array<std::array<char, GolfScoreApp::MaxNameLength>, GolfScoreApp::MaxPlayers> playerNames{};
        std::array<std::array<uint8_t, GolfScoreApp::MaxHoles>, GolfScoreApp::MaxPlayers> strokes{};
        std::array<uint8_t, GolfScoreApp::MaxHoles> par{};
    };

    void sanitize_csv_field(const char *input, char *output, size_t size)
    {
        if (!output || size == 0)
        {
            return;
        }

        if (!input)
        {
            output[0] = '\0';
            return;
        }

        size_t i = 0;
        while (input[i] != '\0' && i < size - 1)
        {
            char ch = input[i];
            if (ch == ',' || ch == '\n' || ch == '\r')
            {
                ch = ' ';
            }
            output[i] = ch;
            ++i;
        }
        output[i] = '\0';
    }
}

GolfScoreApp::GolfScoreApp()
{
    gui = static_cast<Gui *>(furi_record_open(RECORD_GUI));

    if (!easy_flipper_set_view_dispatcher(&viewDispatcher, gui, this))
    {
        FURI_LOG_E(TAG, "Failed to allocate view dispatcher");
        return;
    }

    if (!easy_flipper_set_submenu(&submenu, GolfScoreViewSubmenu, VERSION_TAG, callbackExitApp, &viewDispatcher))
    {
        FURI_LOG_E(TAG, "Failed to allocate submenu");
        return;
    }

    submenu_add_item(submenu, "Scorecard", GolfScoreMenuRun, submenuChoicesCallback, this);
    submenu_add_item(submenu, "Round Setup", GolfScoreMenuSettings, submenuChoicesCallback, this);
    submenu_add_item(submenu, "About", GolfScoreMenuAbout, submenuChoicesCallback, this);

    createAppDataPath();
    applyDefaults();
    loadState();

    view_dispatcher_switch_to_view(viewDispatcher, GolfScoreViewSubmenu);
}

GolfScoreApp::~GolfScoreApp()
{
    if (timer)
    {
        furi_timer_stop(timer);
        furi_timer_free(timer);
        timer = nullptr;
    }

    if (gui && viewPort)
    {
        gui_remove_view_port(gui, viewPort);
        view_port_free(viewPort);
        viewPort = nullptr;
    }

    if (scorecard)
    {
        scorecard.reset();
    }

    if (settings)
    {
        settings.reset();
    }

    if (about)
    {
        about.reset();
    }

    if (submenu)
    {
        view_dispatcher_remove_view(viewDispatcher, GolfScoreViewSubmenu);
        submenu_free(submenu);
    }

    if (viewDispatcher)
    {
        view_dispatcher_free(viewDispatcher);
    }

    if (gui)
    {
        furi_record_close(RECORD_GUI);
    }
}

uint32_t GolfScoreApp::callbackExitApp(void *context)
{
    UNUSED(context);
    return VIEW_NONE;
}

void GolfScoreApp::callbackSubmenuChoices(uint32_t index)
{
    switch (index)
    {
    case GolfScoreMenuRun:
        if (!scorecard)
        {
            scorecard = std::make_unique<GolfScoreScorecard>(this);
        }

        viewPort = view_port_alloc();
        view_port_draw_callback_set(viewPort, viewPortDraw, this);
        view_port_input_callback_set(viewPort, viewPortInput, this);
        gui_add_view_port(gui, viewPort, GuiLayerFullscreen);

        if (timer)
        {
            furi_timer_stop(timer);
            furi_timer_free(timer);
            timer = nullptr;
        }

        timer = furi_timer_alloc(timerCallback, FuriTimerTypePeriodic, this);
        if (timer)
        {
            furi_timer_start(timer, 100);
        }
        break;
    case GolfScoreMenuSettings:
        if (!settings)
        {
            settings = std::make_unique<GolfScoreSettings>(&viewDispatcher, this);
        }
        view_dispatcher_switch_to_view(viewDispatcher, GolfScoreViewSettings);
        break;
    case GolfScoreMenuAbout:
        if (!about)
        {
            about = std::make_unique<GolfScoreAbout>(&viewDispatcher);
        }
        view_dispatcher_switch_to_view(viewDispatcher, GolfScoreViewAbout);
        break;
    default:
        break;
    }
}

void GolfScoreApp::createAppDataPath()
{
    Storage *storage = static_cast<Storage *>(furi_record_open(RECORD_STORAGE));
    if (!storage)
    {
        return;
    }

    char directory_path[256];
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/%s", APP_ID);
    storage_common_mkdir(storage, directory_path);
    snprintf(directory_path, sizeof(directory_path), STORAGE_EXT_PATH_PREFIX "/apps_data/%s/data", APP_ID);
    storage_common_mkdir(storage, directory_path);
    furi_record_close(RECORD_STORAGE);
}

void GolfScoreApp::applyDefaults()
{
    state.version = StateVersion;
    state.playerCount = 1;
    state.holeCount = 9;
    state.reserved = 0;

    for (auto &name : state.playerNames)
    {
        name.fill('\0');
    }

    for (auto &scores : state.strokes)
    {
        scores.fill(0);
    }

    state.par.fill(GolfScoreDefaultPar);

    for (auto &course : state.courses)
    {
        course.holeCount = 0;
        course.par.fill(0);
        course.name.fill('\0');
    }

    state.activeCourse = InvalidCourseIndex;

    for (uint8_t i = 0; i < MaxPlayers; ++i)
    {
        ensureName(i);
    }
}

void GolfScoreApp::loadState()
{
    PersistentState persisted{};
    if (!readStateFromFile(persisted))
    {
        saveState();
        return;
    }

    if (persisted.version != StateVersion)
    {
        saveState();
        return;
    }

    state = persisted;

    if (state.playerCount < 1 || state.playerCount > MaxPlayers)
    {
        state.playerCount = 1;
    }

    if (state.holeCount < 1 || state.holeCount > MaxHoles)
    {
        state.holeCount = 9;
    }

    for (size_t hole = 0; hole < MaxHoles; ++hole)
    {
        uint8_t par = state.par[hole];
        if (par < GolfScoreMinPar || par > GolfScoreMaxPar)
        {
            state.par[hole] = GolfScoreDefaultPar;
        }
    }

    for (size_t courseIndex = 0; courseIndex < state.courses.size(); ++courseIndex)
    {
        auto &course = state.courses[courseIndex];
        if (course.holeCount < 1 || course.holeCount > MaxHoles)
        {
            course.holeCount = 0;
        }

        for (size_t hole = 0; hole < MaxHoles; ++hole)
        {
            uint8_t par = course.par[hole];
            if (par < GolfScoreMinPar || par > GolfScoreMaxPar)
            {
                course.par[hole] = GolfScoreDefaultPar;
            }
        }

        if (course.holeCount == 0)
        {
            course.par.fill(0);
            course.name.fill('\0');
        }
        else if (course.name[0] == '\0')
        {
            snprintf(course.name.data(), course.name.size(), "Course %u", static_cast<unsigned>(courseIndex + 1));
        }
    }

    if (state.activeCourse >= GolfScoreMaxCourses || !courseSlotInUse(state.activeCourse))
    {
        state.activeCourse = InvalidCourseIndex;
    }

    for (uint8_t i = 0; i < MaxPlayers; ++i)
    {
        ensureName(i);
    }
}

void GolfScoreApp::saveState() const
{
    PersistentState copy = state;
    copy.version = StateVersion;
    copy.reserved = 0;
    writeStateToFile(copy);
}

bool GolfScoreApp::writeStateToFile(const PersistentState &data) const
{
    Storage *storage = static_cast<Storage *>(furi_record_open(RECORD_STORAGE));
    if (!storage)
    {
        return false;
    }

    File *file = storage_file_alloc(storage);
    if (!file)
    {
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    char path[256];
    snprintf(path, sizeof(path), STORAGE_EXT_PATH_PREFIX "/apps_data/%s/data/%s", APP_ID, StateFileName);

    bool result = false;
    if (storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS))
    {
        result = storage_file_write(file, &data, sizeof(PersistentState)) == sizeof(PersistentState);
        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

bool GolfScoreApp::readStateFromFile(PersistentState &data) const
{
    Storage *storage = static_cast<Storage *>(furi_record_open(RECORD_STORAGE));
    if (!storage)
    {
        return false;
    }

    File *file = storage_file_alloc(storage);
    if (!file)
    {
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    char path[256];
    snprintf(path, sizeof(path), STORAGE_EXT_PATH_PREFIX "/apps_data/%s/data/%s", APP_ID, StateFileName);

    bool result = false;
    if (storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING))
    {
        if (storage_file_read(file, &data, sizeof(PersistentState)) == sizeof(PersistentState))
        {
            result = true;
        }
        else
        {
            storage_file_seek(file, 0, true);
            PersistentStateV1 legacy{};
            if (storage_file_read(file, &legacy, sizeof(PersistentStateV1)) == sizeof(PersistentStateV1))
            {
                data = PersistentState{};
                data.version = StateVersion;
                data.playerCount = legacy.playerCount;
                data.holeCount = legacy.holeCount;
                data.reserved = 0;
                data.playerNames = legacy.playerNames;
                data.strokes = legacy.strokes;
                data.par = legacy.par;
                for (auto &course : data.courses)
                {
                    course.holeCount = 0;
                    course.par.fill(0);
                    course.name.fill('\0');
                }
                data.activeCourse = InvalidCourseIndex;
                result = true;
            }
        }
        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

void GolfScoreApp::ensureName(uint8_t index)
{
    if (index >= MaxPlayers)
    {
        return;
    }

    auto &name = state.playerNames[index];
    if (name[0] == '\0')
    {
        snprintf(name.data(), name.size(), "Player %u", static_cast<unsigned>(index + 1));
    }
}

void GolfScoreApp::runDispatcher()
{
    view_dispatcher_run(viewDispatcher);
}

void GolfScoreApp::submenuChoicesCallback(void *context, uint32_t index)
{
    auto *app = static_cast<GolfScoreApp *>(context);
    if (app)
    {
        app->callbackSubmenuChoices(index);
    }
}

void GolfScoreApp::timerCallback(void *context)
{
    auto *app = static_cast<GolfScoreApp *>(context);
    if (!app)
    {
        return;
    }

    auto scorecard_ptr = app->scorecard.get();
    if (scorecard_ptr)
    {
        if (scorecard_ptr->isActive())
        {
            if (app->viewPort)
            {
                view_port_update(app->viewPort);
            }
        }
        else
        {
            if (app->timer)
            {
                furi_timer_stop(app->timer);
            }

            if (app->gui && app->viewPort)
            {
                gui_remove_view_port(app->gui, app->viewPort);
                view_port_free(app->viewPort);
                app->viewPort = nullptr;
            }

            view_dispatcher_switch_to_view(app->viewDispatcher, GolfScoreViewSubmenu);
            app->scorecard.reset();
        }
    }
}

void GolfScoreApp::viewPortDraw(Canvas *canvas, void *context)
{
    auto *app = static_cast<GolfScoreApp *>(context);
    if (!app)
    {
        return;
    }

    auto scorecard_ptr = app->scorecard.get();
    if (scorecard_ptr && scorecard_ptr->isActive())
    {
        scorecard_ptr->updateDraw(canvas);
    }
}

void GolfScoreApp::viewPortInput(InputEvent *event, void *context)
{
    auto *app = static_cast<GolfScoreApp *>(context);
    if (!app)
    {
        return;
    }

    auto scorecard_ptr = app->scorecard.get();
    if (scorecard_ptr && scorecard_ptr->isActive())
    {
        scorecard_ptr->updateInput(event);
    }
}

const char *GolfScoreApp::getPlayerName(uint8_t index) const
{
    if (index >= MaxPlayers)
    {
        static const char *empty = "";
        return empty;
    }
    return state.playerNames[index].data();
}

uint8_t GolfScoreApp::getScore(uint8_t player, uint8_t hole) const
{
    if (player >= MaxPlayers || hole >= MaxHoles || hole >= state.holeCount)
    {
        return 0;
    }
    return state.strokes[player][hole];
}

uint16_t GolfScoreApp::getTotalScore(uint8_t player) const
{
    if (player >= MaxPlayers)
    {
        return 0;
    }

    uint16_t total = 0;
    for (size_t hole = 0; hole < state.holeCount; ++hole)
    {
        total += state.strokes[player][hole];
    }
    return total;
}

int16_t GolfScoreApp::getRelativeToPar(uint8_t player) const
{
    if (player >= MaxPlayers)
    {
        return 0;
    }

    int16_t total = 0;
    int16_t par_total = 0;
    for (size_t hole = 0; hole < state.holeCount; ++hole)
    {
        uint8_t strokes = state.strokes[player][hole];
        if (strokes > 0)
        {
            total += strokes;
            par_total += state.par[hole];
        }
    }

    if (par_total == 0)
    {
        return 0;
    }

    return total - par_total;
}

uint8_t GolfScoreApp::getPar(uint8_t hole) const
{
    if (hole >= MaxHoles)
    {
        return 0;
    }
    uint8_t par = state.par[hole];
    if (par < GolfScoreMinPar || par > GolfScoreMaxPar)
    {
        return GolfScoreDefaultPar;
    }
    return par;
}

uint16_t GolfScoreApp::getCoursePar() const
{
    uint16_t total = 0;
    for (size_t hole = 0; hole < state.holeCount; ++hole)
    {
        total += state.par[hole];
    }
    return total;
}

uint8_t GolfScoreApp::getPlayedHoleCount(uint8_t player) const
{
    if (player >= MaxPlayers)
    {
        return 0;
    }

    uint8_t count = 0;
    for (size_t hole = 0; hole < state.holeCount; ++hole)
    {
        if (state.strokes[player][hole] > 0)
        {
            ++count;
        }
    }
    return count;
}

void GolfScoreApp::adjustScore(uint8_t player, uint8_t hole, int8_t delta)
{
    if (player >= state.playerCount || hole >= state.holeCount)
    {
        return;
    }

    int value = static_cast<int>(state.strokes[player][hole]);
    value = std::clamp(value + delta, 0, 99);
    state.strokes[player][hole] = static_cast<uint8_t>(value);
    saveState();
    requestCanvasRefresh();
}

void GolfScoreApp::resetScores()
{
    for (auto &scores : state.strokes)
    {
        scores.fill(0);
    }
    saveState();
    requestCanvasRefresh();
}

void GolfScoreApp::cyclePlayerCount()
{
    uint8_t next = state.playerCount >= MaxPlayers ? 1 : state.playerCount + 1;
    setPlayerCount(next);
}

void GolfScoreApp::setPlayerCount(uint8_t count)
{
    if (count < 1)
    {
        count = 1;
    }
    if (count > MaxPlayers)
    {
        count = MaxPlayers;
    }

    if (state.playerCount == count)
    {
        return;
    }

    state.playerCount = count;
    saveState();
    requestCanvasRefresh();
}

void GolfScoreApp::toggleHoleCount()
{
    uint8_t next = state.holeCount == 18 ? 9 : 18;
    setHoleCount(next);
}

void GolfScoreApp::setHoleCount(uint8_t count)
{
    if (count < 1)
    {
        count = 1;
    }
    if (count > MaxHoles)
    {
        count = MaxHoles;
    }

    if (state.holeCount == count)
    {
        return;
    }

    state.holeCount = count;
    saveState();
    requestCanvasRefresh();
}

void GolfScoreApp::setPlayerName(uint8_t index, const char *name)
{
    if (index >= MaxPlayers)
    {
        return;
    }

    auto &target = state.playerNames[index];
    target.fill('\0');

    if (name && name[0] != '\0')
    {
        snprintf(target.data(), target.size(), "%s", name);
    }

    ensureName(index);
    saveState();
    requestCanvasRefresh();
}

void GolfScoreApp::setPar(uint8_t hole, uint8_t value)
{
    if (hole >= MaxHoles)
    {
        return;
    }

    int clamped = std::clamp<int>(static_cast<int>(value), static_cast<int>(GolfScoreMinPar), static_cast<int>(GolfScoreMaxPar));
    uint8_t par_value = static_cast<uint8_t>(clamped);

    if (state.par[hole] == par_value)
    {
        return;
    }

    state.par[hole] = par_value;
    saveState();
    requestCanvasRefresh();
}

bool GolfScoreApp::saveCoursePreset(uint8_t index, const char *name)
{
    if (index >= GolfScoreMaxCourses)
    {
        return false;
    }

    auto &preset = state.courses[index];
    preset.holeCount = std::clamp<uint8_t>(state.holeCount, 1, MaxHoles);
    preset.par = state.par;
    preset.name.fill('\0');

    if (name && name[0] != '\0')
    {
        snprintf(preset.name.data(), preset.name.size(), "%s", name);
    }
    else
    {
        snprintf(preset.name.data(), preset.name.size(), "Course %u", static_cast<unsigned>(index + 1));
    }

    state.activeCourse = index;
    saveState();
    return true;
}

bool GolfScoreApp::deleteCoursePreset(uint8_t index)
{
    if (index >= GolfScoreMaxCourses)
    {
        return false;
    }

    auto &preset = state.courses[index];
    preset.holeCount = 0;
    preset.par.fill(0);
    preset.name.fill('\0');

    if (state.activeCourse == index)
    {
        state.activeCourse = InvalidCourseIndex;
    }

    saveState();
    return true;
}

void GolfScoreApp::applyCoursePreset(uint8_t index)
{
    if (!courseSlotInUse(index))
    {
        return;
    }

    const auto &preset = state.courses[index];
    state.holeCount = std::clamp<uint8_t>(preset.holeCount, 1, MaxHoles);
    state.par = preset.par;
    state.activeCourse = index;
    resetScores();
}

bool GolfScoreApp::courseSlotInUse(uint8_t index) const
{
    if (index >= GolfScoreMaxCourses)
    {
        return false;
    }

    const auto &preset = state.courses[index];
    return preset.holeCount >= 1 && preset.holeCount <= MaxHoles;
}

const char *GolfScoreApp::getCourseName(uint8_t index) const
{
    if (!courseSlotInUse(index))
    {
        static const char empty[] = "";
        return empty;
    }
    return state.courses[index].name.data();
}

uint8_t GolfScoreApp::getCourseHoleCount(uint8_t index) const
{
    if (!courseSlotInUse(index))
    {
        return 0;
    }
    return state.courses[index].holeCount;
}

uint8_t GolfScoreApp::getActiveCourseIndex() const
{
    return state.activeCourse;
}

bool GolfScoreApp::exportRoundHistory() const
{
    Storage *storage = static_cast<Storage *>(furi_record_open(RECORD_STORAGE));
    if (!storage)
    {
        return false;
    }

    File *file = storage_file_alloc(storage);
    if (!file)
    {
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    char path[256];
    snprintf(path, sizeof(path), STORAGE_EXT_PATH_PREFIX "/apps_data/%s/data/%s", APP_ID, HistoryFileName);

    bool result = false;
    if (storage_file_open(file, path, FSAM_WRITE, FSOM_OPEN_APPEND))
    {
        uint64_t existing_size = storage_file_size(file);
        if (existing_size == 0)
        {
            FuriString *header = furi_string_alloc();
            furi_string_printf(header, "Date,Time,Course,HoleCount,Player,Total,Relative");
            for (uint8_t hole = 0; hole < MaxHoles; ++hole)
            {
                furi_string_cat_printf(header, ",H%u", static_cast<unsigned>(hole + 1));
            }
            furi_string_cat_str(header, "\r\n");
            storage_file_write(file, furi_string_get_cstr(header), furi_string_size(header));
            furi_string_free(header);
        }

        DateTime datetime;
        furi_hal_rtc_get_datetime(&datetime);

        char date_buf[16];
        char time_buf[16];
        snprintf(date_buf, sizeof(date_buf), "%04u-%02u-%02u", datetime.year, datetime.month, datetime.day);
        snprintf(time_buf, sizeof(time_buf), "%02u:%02u", datetime.hour, datetime.minute);

        char course_name[GolfScoreCourseNameLength];
        if (state.activeCourse != InvalidCourseIndex && courseSlotInUse(state.activeCourse))
        {
            sanitize_csv_field(state.courses[state.activeCourse].name.data(), course_name, sizeof(course_name));
        }
        else
        {
            snprintf(course_name, sizeof(course_name), "Custom");
        }

        for (uint8_t i = 0; i < state.playerCount; ++i)
        {
            char player_name[GolfScoreMaxNameLength];
            sanitize_csv_field(getPlayerName(i), player_name, sizeof(player_name));

            uint16_t total = getTotalScore(i);
            uint8_t holes_played = getPlayedHoleCount(i);
            int16_t rel = getRelativeToPar(i);

            char relation[8];
            if (holes_played == 0)
            {
                snprintf(relation, sizeof(relation), "--");
            }
            else if (rel > 0)
            {
                snprintf(relation, sizeof(relation), "+%d", rel);
            }
            else
            {
                snprintf(relation, sizeof(relation), "%d", rel);
            }

            FuriString *row = furi_string_alloc();
            furi_string_printf(row,
                               "%s,%s,%s,%u,%s,%u,%s",
                               date_buf,
                               time_buf,
                               course_name,
                               static_cast<unsigned>(state.holeCount),
                               player_name,
                               static_cast<unsigned>(total),
                               relation);

            for (uint8_t hole = 0; hole < MaxHoles; ++hole)
            {
                if (hole < state.holeCount)
                {
                    uint8_t strokes = state.strokes[i][hole];
                    if (strokes > 0)
                    {
                        furi_string_cat_printf(row, ",%u", static_cast<unsigned>(strokes));
                    }
                    else
                    {
                        furi_string_cat_str(row, ",");
                    }
                }
                else
                {
                    furi_string_cat_str(row, ",");
                }
            }

            furi_string_cat_str(row, "\r\n");

            size_t row_size = furi_string_size(row);
            if (storage_file_write(file, furi_string_get_cstr(row), row_size) != row_size)
            {
                furi_string_free(row);
                result = false;
                break;
            }

            furi_string_free(row);
            result = true;
        }

        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

bool GolfScoreApp::clearRoundHistory() const
{
    Storage *storage = static_cast<Storage *>(furi_record_open(RECORD_STORAGE));
    if (!storage)
    {
        return false;
    }

    char path[256];
    snprintf(path, sizeof(path), STORAGE_EXT_PATH_PREFIX "/apps_data/%s/data/%s", APP_ID, HistoryFileName);
    bool result = storage_common_remove(storage, path);

    if (!result)
    {
        File *file = storage_file_alloc(storage);
        if (file)
        {
            if (storage_file_open(file, path, FSAM_WRITE, FSOM_CREATE_ALWAYS))
            {
                storage_file_close(file);
                result = true;
            }
            storage_file_free(file);
        }
    }

    furi_record_close(RECORD_STORAGE);
    return result;
}

bool GolfScoreApp::readRoundHistory(FuriString *out) const
{
    if (!out)
    {
        return false;
    }

    Storage *storage = static_cast<Storage *>(furi_record_open(RECORD_STORAGE));
    if (!storage)
    {
        return false;
    }

    File *file = storage_file_alloc(storage);
    if (!file)
    {
        furi_record_close(RECORD_STORAGE);
        return false;
    }

    char path[256];
    snprintf(path, sizeof(path), STORAGE_EXT_PATH_PREFIX "/apps_data/%s/data/%s", APP_ID, HistoryFileName);

    bool result = false;
    if (storage_file_open(file, path, FSAM_READ, FSOM_OPEN_EXISTING))
    {
        furi_string_reset(out);
        char buffer[129];
        size_t read = 0;
        while ((read = storage_file_read(file, buffer, sizeof(buffer) - 1)) > 0)
        {
            buffer[read] = '\0';
            furi_string_cat_str(out, buffer);
        }
        result = furi_string_size(out) > 0;
        storage_file_close(file);
    }

    storage_file_free(file);
    furi_record_close(RECORD_STORAGE);
    return result;
}

void GolfScoreApp::requestCanvasRefresh()
{
    if (viewPort)
    {
        view_port_update(viewPort);
    }
}

extern "C"
{
    int32_t golf_score_main(void *p)
    {
        UNUSED(p);

        GolfScoreApp app;
        app.runDispatcher();

        return 0;
    }
}
