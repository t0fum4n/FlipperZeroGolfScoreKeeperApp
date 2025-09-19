#include "app.hpp"
#include "about/about.hpp"
#include "scorecard/scorecard.hpp"
#include "settings/settings.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include <storage/storage.h>

namespace
{
    constexpr uint8_t StateVersion = 1;
    constexpr const char *StateFileName = "state.bin";
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

    state.par.fill(4);

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
        if (state.par[hole] == 0)
        {
            state.par[hole] = 4;
        }
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
        result = storage_file_read(file, &data, sizeof(PersistentState)) == sizeof(PersistentState);
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
    if (hole >= MaxHoles || hole >= state.holeCount)
    {
        return 0;
    }
    return state.par[hole];
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
