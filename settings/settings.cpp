#include "settings.hpp"
#include "app.hpp"

#include <cstdio>
#include <cstring>
#include <memory>
#include <furi/core/string.h>

GolfScoreSettings::GolfScoreSettings(ViewDispatcher **view_dispatcher, void *appContext) : appContext(appContext), view_dispatcher_ref(view_dispatcher)
{
    if (!easy_flipper_set_variable_item_list(&variable_item_list, GolfScoreViewSettings,
                                             settingsItemSelectedCallback, callbackToSubmenu, view_dispatcher, this))
    {
        return;
    }

    variable_item_player_count = variable_item_list_add(variable_item_list, "Players", 1, nullptr, nullptr);
    variable_item_hole_count = variable_item_list_add(variable_item_list, "Holes", 1, nullptr, nullptr);
    variable_item_reset = variable_item_list_add(variable_item_list, "Reset Round", 1, nullptr, nullptr);
    variable_item_par_overview = variable_item_list_add(variable_item_list, "Hole Pars", 1, nullptr, nullptr);
    variable_item_load_course = variable_item_list_add(variable_item_list, "Load Course", 1, nullptr, nullptr);
    variable_item_save_course = variable_item_list_add(variable_item_list, "Save Course", 1, nullptr, nullptr);
    variable_item_delete_course = variable_item_list_add(variable_item_list, "Delete Course", 1, nullptr, nullptr);
    variable_item_save_round = variable_item_list_add(variable_item_list, "Save Round", 1, nullptr, nullptr);
    variable_item_view_history = variable_item_list_add(variable_item_list, "View History", 1, nullptr, nullptr);
    variable_item_clear_history = variable_item_list_add(variable_item_list, "Clear History", 1, nullptr, nullptr);

    for (uint8_t i = 0; i < GolfScoreMaxPlayers; ++i)
    {
        char label[24];
        snprintf(label, sizeof(label), "Player %u Name", static_cast<unsigned>(i + 1));
        variable_item_player_names[i] = variable_item_list_add(variable_item_list, label, 1, nullptr, nullptr);
    }

    refreshValueTexts();
}

GolfScoreSettings::~GolfScoreSettings()
{
    freeTextInput();

    if (variable_item_list && view_dispatcher_ref && *view_dispatcher_ref)
    {
        view_dispatcher_remove_view(*view_dispatcher_ref, GolfScoreViewSettings);
        variable_item_list_free(variable_item_list);
        variable_item_list = nullptr;
    }

    if (par_variable_item_list && view_dispatcher_ref && *view_dispatcher_ref)
    {
        view_dispatcher_remove_view(*view_dispatcher_ref, GolfScoreViewParSettings);
        variable_item_list_free(par_variable_item_list);
        par_variable_item_list = nullptr;
    }

    if (course_variable_item_list && view_dispatcher_ref && *view_dispatcher_ref)
    {
        view_dispatcher_remove_view(*view_dispatcher_ref, GolfScoreViewCourseList);
        variable_item_list_free(course_variable_item_list);
        course_variable_item_list = nullptr;
    }

    if (history_widget && view_dispatcher_ref && *view_dispatcher_ref)
    {
        view_dispatcher_remove_view(*view_dispatcher_ref, GolfScoreViewHistory);
        widget_free(history_widget);
        history_widget = nullptr;
    }

    variable_item_player_count = nullptr;
    variable_item_hole_count = nullptr;
    variable_item_reset = nullptr;
    variable_item_par_overview = nullptr;
    variable_item_load_course = nullptr;
    variable_item_save_course = nullptr;
    variable_item_delete_course = nullptr;
    variable_item_save_round = nullptr;
    variable_item_view_history = nullptr;
    variable_item_clear_history = nullptr;
    variable_item_player_names.fill(nullptr);
    course_items.fill(nullptr);
    par_item_hole_selector = nullptr;
    par_item_value = nullptr;
    par_hole_label.fill('\0');
    par_value_label.fill('\0');
    par_summary_text.fill('\0');
    selected_par_hole = 0;
    course_selection_mode = CourseSelectionMode::Load;
    pending_course_slot = 0;
}

uint32_t GolfScoreSettings::callbackToSettings(void *context)
{
    UNUSED(context);
    return GolfScoreViewSettings;
}

uint32_t GolfScoreSettings::callbackToCourseList(void *context)
{
    UNUSED(context);
    return GolfScoreViewCourseList;
}

uint32_t GolfScoreSettings::callbackToSubmenu(void *context)
{
    UNUSED(context);
    return GolfScoreViewSubmenu;
}

void GolfScoreSettings::freeTextInput()
{
    if (text_input && view_dispatcher_ref && *view_dispatcher_ref)
    {
        view_dispatcher_remove_view(*view_dispatcher_ref, GolfScoreViewTextInput);
#ifndef FW_ORIGIN_Momentum
        uart_text_input_free(text_input);
#else
        text_input_free(text_input);
#endif
        text_input = nullptr;
    }

    text_input_buffer.reset();
    text_input_temp_buffer.reset();
}

bool GolfScoreSettings::initTextInput(uint32_t view)
{
    if (text_input_buffer || text_input_temp_buffer)
    {
        return false;
    }

    text_input_buffer = std::make_unique<char[]>(text_input_buffer_size);
    text_input_temp_buffer = std::make_unique<char[]>(text_input_buffer_size);

    if (!text_input_buffer || !text_input_temp_buffer)
    {
        return false;
    }

    std::memset(text_input_buffer.get(), 0, text_input_buffer_size);
    std::memset(text_input_temp_buffer.get(), 0, text_input_buffer_size);

    GolfScoreApp *app = static_cast<GolfScoreApp *>(appContext);
    if (!app)
    {
        return false;
    }

    if (view >= SettingsViewPlayerName1 && view <= SettingsViewPlayerName4)
    {
        uint8_t playerIndex = static_cast<uint8_t>(view - SettingsViewPlayerName1);
        const char *current = app->getPlayerName(playerIndex);
        if (current)
        {
            strncpy(text_input_temp_buffer.get(), current, text_input_buffer_size - 1);
            text_input_temp_buffer[text_input_buffer_size - 1] = '\0';
        }

        char header[24];
        snprintf(header, sizeof(header), "Player %u Name", static_cast<unsigned>(playerIndex + 1));

#ifndef FW_ORIGIN_Momentum
        switch (playerIndex)
        {
        case 0:
            return easy_flipper_set_uart_text_input(&text_input, GolfScoreViewTextInput, header, text_input_temp_buffer.get(), text_input_buffer_size,
                                                    textUpdatedPlayer0Callback, callbackToSettings, view_dispatcher_ref, this);
        case 1:
            return easy_flipper_set_uart_text_input(&text_input, GolfScoreViewTextInput, header, text_input_temp_buffer.get(), text_input_buffer_size,
                                                    textUpdatedPlayer1Callback, callbackToSettings, view_dispatcher_ref, this);
        case 2:
            return easy_flipper_set_uart_text_input(&text_input, GolfScoreViewTextInput, header, text_input_temp_buffer.get(), text_input_buffer_size,
                                                    textUpdatedPlayer2Callback, callbackToSettings, view_dispatcher_ref, this);
        case 3:
            return easy_flipper_set_uart_text_input(&text_input, GolfScoreViewTextInput, header, text_input_temp_buffer.get(), text_input_buffer_size,
                                                    textUpdatedPlayer3Callback, callbackToSettings, view_dispatcher_ref, this);
        default:
            break;
        }
#else
        switch (playerIndex)
        {
        case 0:
            return easy_flipper_set_text_input(&text_input, GolfScoreViewTextInput, header, text_input_temp_buffer.get(), text_input_buffer_size,
                                               textUpdatedPlayer0Callback, callbackToSettings, view_dispatcher_ref, this);
        case 1:
            return easy_flipper_set_text_input(&text_input, GolfScoreViewTextInput, header, text_input_temp_buffer.get(), text_input_buffer_size,
                                               textUpdatedPlayer1Callback, callbackToSettings, view_dispatcher_ref, this);
        case 2:
            return easy_flipper_set_text_input(&text_input, GolfScoreViewTextInput, header, text_input_temp_buffer.get(), text_input_buffer_size,
                                               textUpdatedPlayer2Callback, callbackToSettings, view_dispatcher_ref, this);
        case 3:
            return easy_flipper_set_text_input(&text_input, GolfScoreViewTextInput, header, text_input_temp_buffer.get(), text_input_buffer_size,
                                               textUpdatedPlayer3Callback, callbackToSettings, view_dispatcher_ref, this);
        default:
            break;
        }
#endif
    }

    return false;
}

void GolfScoreSettings::settingsItemSelected(uint32_t index)
{
    GolfScoreApp *app = static_cast<GolfScoreApp *>(appContext);
    if (!app)
    {
        return;
    }

    switch (index)
    {
    case SettingsViewPlayers:
        app->cyclePlayerCount();
        refreshValueTexts();
        break;
    case SettingsViewHoles:
        app->toggleHoleCount();
        refreshValueTexts();
        break;
    case SettingsViewReset:
        app->resetScores();
        easy_flipper_dialog("Scores Reset", "All player strokes cleared.");
        refreshValueTexts();
        break;
    case SettingsViewPars:
        if (view_dispatcher_ref && *view_dispatcher_ref)
        {
            if (ensureParList())
            {
                refreshParValues();
                view_dispatcher_switch_to_view(*view_dispatcher_ref, GolfScoreViewParSettings);
            }
        }
        break;
    case SettingsViewLoadCourse:
        startCourseSelection(CourseSelectionMode::Load);
        break;
    case SettingsViewSaveCourse:
        startCourseSelection(CourseSelectionMode::Save);
        break;
    case SettingsViewDeleteCourse:
        startCourseSelection(CourseSelectionMode::Delete);
        break;
    case SettingsViewSaveRound:
    {
        if (app->exportRoundHistory())
        {
            easy_flipper_dialog("Round Saved", "Entry added to history.");
        }
        else
        {
            easy_flipper_dialog("Save Failed", "Could not save round.");
        }
        break;
    }
    case SettingsViewViewHistory:
        showHistory();
        break;
    case SettingsViewClearHistory:
        clearHistory();
        updateCourseListDisplay();
        break;
    case SettingsViewPlayerName1:
    case SettingsViewPlayerName2:
    case SettingsViewPlayerName3:
    case SettingsViewPlayerName4:
        startTextInput(index);
        break;
    default:
        break;
    }
}

void GolfScoreSettings::settingsItemSelectedCallback(void *context, uint32_t index)
{
    auto *settings = static_cast<GolfScoreSettings *>(context);
    if (settings)
    {
        settings->settingsItemSelected(index);
    }
}

bool GolfScoreSettings::startTextInput(uint32_t view)
{
    freeTextInput();
    if (!initTextInput(view))
    {
        return false;
    }

    if (view_dispatcher_ref && *view_dispatcher_ref)
    {
        view_dispatcher_switch_to_view(*view_dispatcher_ref, GolfScoreViewTextInput);
        return true;
    }
    return false;
}

void GolfScoreSettings::textUpdated(uint32_t view)
{
    GolfScoreApp *app = static_cast<GolfScoreApp *>(appContext);
    if (!app)
    {
        return;
    }

    if (!text_input_buffer || !text_input_temp_buffer)
    {
        return;
    }

    strncpy(text_input_buffer.get(), text_input_temp_buffer.get(), text_input_buffer_size - 1);
    text_input_buffer[text_input_buffer_size - 1] = '\0';

    switch (view)
    {
    case SettingsViewPlayerName1:
    case SettingsViewPlayerName2:
    case SettingsViewPlayerName3:
    case SettingsViewPlayerName4:
    {
        uint8_t playerIndex = static_cast<uint8_t>(view - SettingsViewPlayerName1);
        app->setPlayerName(playerIndex, text_input_buffer.get());
        break;
    }
    default:
        break;
    }

    refreshValueTexts();

    if (view_dispatcher_ref && *view_dispatcher_ref)
    {
        view_dispatcher_switch_to_view(*view_dispatcher_ref, GolfScoreViewSettings);
    }
}

void GolfScoreSettings::refreshValueTexts()
{
    GolfScoreApp *app = static_cast<GolfScoreApp *>(appContext);
    if (!app)
    {
        return;
    }

    char buffer[32];

    if (variable_item_player_count)
    {
        snprintf(buffer, sizeof(buffer), "%u", static_cast<unsigned>(app->getPlayerCount()));
        variable_item_set_current_value_text(variable_item_player_count, buffer);
    }

    if (variable_item_hole_count)
    {
        snprintf(buffer, sizeof(buffer), "%u", static_cast<unsigned>(app->getHoleCount()));
        variable_item_set_current_value_text(variable_item_hole_count, buffer);
    }

    if (variable_item_reset)
    {
        variable_item_set_current_value_text(variable_item_reset, "Select to clear");
    }

    updateParSummary();

    if (variable_item_load_course)
    {
        char text[32];
        uint8_t active = app->getActiveCourseIndex();
        if (active != GolfScoreApp::InvalidCourseIndex && app->courseSlotInUse(active))
        {
            snprintf(text, sizeof(text), "%s", app->getCourseName(active));
        }
        else
        {
            snprintf(text, sizeof(text), "None");
        }
        variable_item_set_current_value_text(variable_item_load_course, text);
    }

    if (variable_item_save_course)
    {
        char text[32];
        snprintf(text, sizeof(text), "%u holes", static_cast<unsigned>(app->getHoleCount()));
        variable_item_set_current_value_text(variable_item_save_course, text);
    }

    if (variable_item_delete_course)
    {
        variable_item_set_current_value_text(variable_item_delete_course, "Select slot");
    }

    if (variable_item_save_round)
    {
        char text[32];
        snprintf(text, sizeof(text), "%u players", static_cast<unsigned>(app->getPlayerCount()));
        variable_item_set_current_value_text(variable_item_save_round, text);
    }

    if (variable_item_view_history)
    {
        variable_item_set_current_value_text(variable_item_view_history, "Open log");
    }

    if (variable_item_clear_history)
    {
        variable_item_set_current_value_text(variable_item_clear_history, "Delete log");
    }

    for (uint8_t i = 0; i < GolfScoreMaxPlayers; ++i)
    {
        if (!variable_item_player_names[i])
        {
            continue;
        }

        const char *name = app->getPlayerName(i);
        if (i < app->getPlayerCount())
        {
            variable_item_set_current_value_text(variable_item_player_names[i], name);
        }
        else
        {
            char inactive[32];
            snprintf(inactive, sizeof(inactive), "%s (off)", name);
            variable_item_set_current_value_text(variable_item_player_names[i], inactive);
        }
    }

    if (par_variable_item_list)
    {
        refreshParValues();
    }

    if (course_variable_item_list)
    {
        updateCourseListDisplay();
    }
}

void GolfScoreSettings::refreshParValues()
{
    if (!par_variable_item_list)
    {
        return;
    }

    updateParEditorDisplay();
}

bool GolfScoreSettings::ensureParList()
{
    if (par_variable_item_list)
    {
        return true;
    }

    if (!easy_flipper_set_variable_item_list(&par_variable_item_list, GolfScoreViewParSettings,
                                             nullptr, callbackToSettings, view_dispatcher_ref, this))
    {
        return false;
    }

    par_hole_context.settings = this;
    par_hole_context.hole_selector = true;
    par_value_context.settings = this;
    par_value_context.hole_selector = false;

    par_item_hole_selector = variable_item_list_add(par_variable_item_list, "Hole", 1, parHoleSelectorChangedCallback, &par_hole_context);
    par_item_value = variable_item_list_add(par_variable_item_list, "Par", HoleParOptionCount, parValueChangedCallback, &par_value_context);

    updateParEditorDisplay();

    return true;
}

bool GolfScoreSettings::ensureCourseList()
{
    if (course_variable_item_list)
    {
        return true;
    }

    if (!easy_flipper_set_variable_item_list(&course_variable_item_list, GolfScoreViewCourseList,
                                             courseItemSelectedCallback, callbackToSettings, view_dispatcher_ref, this))
    {
        return false;
    }

    for (uint8_t i = 0; i < GolfScoreMaxCourses; ++i)
    {
        char label[24];
        snprintf(label, sizeof(label), "Slot %u", static_cast<unsigned>(i + 1));
        course_items[i] = variable_item_list_add(course_variable_item_list, label, 1, nullptr, nullptr);
    }

    updateCourseListDisplay();
    return true;
}

void GolfScoreSettings::updateParSummary()
{
    if (!variable_item_par_overview)
    {
        return;
    }

    GolfScoreApp *app = static_cast<GolfScoreApp *>(appContext);
    if (!app)
    {
        return;
    }

    uint8_t hole_count = app->getHoleCount();
    uint16_t course_par = app->getCoursePar();
    snprintf(par_summary_text.data(), par_summary_text.size(), "%u holes, par %u", static_cast<unsigned>(hole_count), static_cast<unsigned>(course_par));
    variable_item_set_current_value_text(variable_item_par_overview, par_summary_text.data());
}

void GolfScoreSettings::startCourseSelection(CourseSelectionMode mode)
{
    if (!view_dispatcher_ref || !*view_dispatcher_ref)
    {
        return;
    }

    course_selection_mode = mode;
    pending_course_slot = 0;

    if (!ensureCourseList())
    {
        return;
    }

    updateCourseListDisplay();
    view_dispatcher_switch_to_view(*view_dispatcher_ref, GolfScoreViewCourseList);
}

void GolfScoreSettings::updateCourseListDisplay()
{
    GolfScoreApp *app = static_cast<GolfScoreApp *>(appContext);
    if (!app)
    {
        return;
    }

    for (uint8_t i = 0; i < GolfScoreMaxCourses; ++i)
    {
        if (!course_items[i])
        {
            continue;
        }

        char text[32];
        if (app->courseSlotInUse(i))
        {
            const char *name = app->getCourseName(i);
            uint8_t holes = app->getCourseHoleCount(i);
            snprintf(text, sizeof(text), "%s (%u)", name, static_cast<unsigned>(holes));
        }
        else
        {
            snprintf(text, sizeof(text), "Empty");
        }

        variable_item_set_current_value_text(course_items[i], text);
    }
}

bool GolfScoreSettings::ensureHistoryWidget()
{
    if (history_widget)
    {
        return true;
    }

    return easy_flipper_set_widget(&history_widget, GolfScoreViewHistory, nullptr, callbackToSettings, view_dispatcher_ref);
}

void GolfScoreSettings::showHistory()
{
    if (!view_dispatcher_ref || !*view_dispatcher_ref)
    {
        return;
    }

    if (!ensureHistoryWidget())
    {
        easy_flipper_dialog("History", "Unable to open history view.");
        return;
    }

    GolfScoreApp *app = static_cast<GolfScoreApp *>(appContext);
    if (!app)
    {
        return;
    }

    FuriString *history = furi_string_alloc();
    bool has_history = app->readRoundHistory(history);

    widget_reset(history_widget);

    if (has_history)
    {
        widget_add_text_scroll_element(history_widget, 0, 0, 128, 64, furi_string_get_cstr(history));
    }
    else
    {
        widget_add_text_scroll_element(history_widget, 0, 0, 128, 64, "No saved rounds yet.");
    }

    furi_string_free(history);

    view_dispatcher_switch_to_view(*view_dispatcher_ref, GolfScoreViewHistory);
}

void GolfScoreSettings::clearHistory()
{
    GolfScoreApp *app = static_cast<GolfScoreApp *>(appContext);
    if (!app)
    {
        return;
    }

    if (app->clearRoundHistory())
    {
        easy_flipper_dialog("Round History", "History cleared.");
        if (history_widget)
        {
            widget_reset(history_widget);
            widget_add_text_scroll_element(history_widget, 0, 0, 128, 64, "No saved rounds yet.");
        }
    }
    else
    {
        easy_flipper_dialog("Round History", "Failed to clear history.");
    }
}

void GolfScoreSettings::textUpdatedPlayer0Callback(void *context)
{
    auto *settings = static_cast<GolfScoreSettings *>(context);
    if (settings)
    {
        settings->textUpdated(SettingsViewPlayerName1);
    }
}

void GolfScoreSettings::textUpdatedPlayer1Callback(void *context)
{
    auto *settings = static_cast<GolfScoreSettings *>(context);
    if (settings)
    {
        settings->textUpdated(SettingsViewPlayerName2);
    }
}

void GolfScoreSettings::textUpdatedPlayer2Callback(void *context)
{
    auto *settings = static_cast<GolfScoreSettings *>(context);
    if (settings)
    {
        settings->textUpdated(SettingsViewPlayerName3);
    }
}

void GolfScoreSettings::textUpdatedPlayer3Callback(void *context)
{
    auto *settings = static_cast<GolfScoreSettings *>(context);
    if (settings)
    {
        settings->textUpdated(SettingsViewPlayerName4);
    }
}

void GolfScoreSettings::parHoleSelectorChangedCallback(VariableItem *item)
{
    if (!item)
    {
        return;
    }

    auto *context = static_cast<ParItemContext *>(variable_item_get_context(item));
    if (!context || !context->settings || !context->hole_selector)
    {
        return;
    }

    context->settings->parHoleSelectorChanged(item);
}

void GolfScoreSettings::parHoleSelectorChanged(VariableItem *item)
{
    GolfScoreApp *app = static_cast<GolfScoreApp *>(appContext);
    if (!app)
    {
        return;
    }

    uint8_t hole_count = app->getHoleCount();
    if (hole_count == 0)
    {
        hole_count = 1;
    }

    uint8_t index = variable_item_get_current_value_index(item);
    if (index >= hole_count)
    {
        index = static_cast<uint8_t>(hole_count - 1);
    }

    selected_par_hole = index;
    updateParEditorDisplay();
}

void GolfScoreSettings::parValueChangedCallback(VariableItem *item)
{
    if (!item)
    {
        return;
    }

    auto *context = static_cast<ParItemContext *>(variable_item_get_context(item));
    if (!context || !context->settings || context->hole_selector)
    {
        return;
    }

    context->settings->parValueChanged(item);
}

void GolfScoreSettings::parValueChanged(VariableItem *item)
{
    if (suppress_par_updates)
    {
        return;
    }

    GolfScoreApp *app = static_cast<GolfScoreApp *>(appContext);
    if (!app)
    {
        return;
    }

    uint8_t index = variable_item_get_current_value_index(item);
    if (index >= HoleParOptionCount)
    {
        index = static_cast<uint8_t>(HoleParOptionCount - 1);
    }

    uint8_t par = indexToPar(index);
    app->setPar(selected_par_hole, par);
    updateParEditorDisplay();
    updateParSummary();
}

void GolfScoreSettings::updateParEditorDisplay()
{
    if (!par_item_hole_selector || !par_item_value)
    {
        return;
    }

    GolfScoreApp *app = static_cast<GolfScoreApp *>(appContext);
    if (!app)
    {
        return;
    }

    uint8_t hole_count = app->getHoleCount();
    if (hole_count == 0)
    {
        hole_count = 1;
    }
    if (selected_par_hole >= hole_count)
    {
        selected_par_hole = static_cast<uint8_t>(hole_count - 1);
    }

    bool restore_flag = suppress_par_updates;
    suppress_par_updates = true;

    variable_item_set_values_count(par_item_hole_selector, hole_count);
    variable_item_set_current_value_index(par_item_hole_selector, selected_par_hole);
    snprintf(par_hole_label.data(), par_hole_label.size(), "Hole %u/%u", static_cast<unsigned>(selected_par_hole + 1), static_cast<unsigned>(hole_count));
    variable_item_set_current_value_text(par_item_hole_selector, par_hole_label.data());

    uint8_t par = app->getPar(selected_par_hole);
    uint8_t par_index = parToIndex(par);
    variable_item_set_current_value_index(par_item_value, par_index);
    snprintf(par_value_label.data(), par_value_label.size(), "%u", static_cast<unsigned>(par));
    variable_item_set_current_value_text(par_item_value, par_value_label.data());

    suppress_par_updates = restore_flag;
}

void GolfScoreSettings::courseItemSelectedCallback(void *context, uint32_t index)
{
    auto *settings = static_cast<GolfScoreSettings *>(context);
    if (settings)
    {
        settings->courseItemSelected(index);
    }
}

void GolfScoreSettings::courseItemSelected(uint32_t index)
{
    if (index >= GolfScoreMaxCourses)
    {
        return;
    }

    GolfScoreApp *app = static_cast<GolfScoreApp *>(appContext);
    if (!app)
    {
        return;
    }

    pending_course_slot = static_cast<uint8_t>(index);

    switch (course_selection_mode)
    {
    case CourseSelectionMode::Load:
        if (app->courseSlotInUse(pending_course_slot))
        {
            const char *name = app->getCourseName(pending_course_slot);
            app->applyCoursePreset(pending_course_slot);
            updateParSummary();
            refreshParValues();
            refreshValueTexts();
            updateCourseListDisplay();
            easy_flipper_dialog("Course Loaded", name);
            if (view_dispatcher_ref && *view_dispatcher_ref)
            {
                view_dispatcher_switch_to_view(*view_dispatcher_ref, GolfScoreViewSettings);
            }
        }
        else
        {
            easy_flipper_dialog("Empty Slot", "No course saved in this slot.");
        }
        break;
    case CourseSelectionMode::Save:
        if (!startCourseNameInput(pending_course_slot))
        {
            easy_flipper_dialog("Save Failed", "Unable to start name input.");
            if (view_dispatcher_ref && *view_dispatcher_ref)
            {
                view_dispatcher_switch_to_view(*view_dispatcher_ref, GolfScoreViewCourseList);
            }
        }
        break;
    case CourseSelectionMode::Delete:
        if (app->courseSlotInUse(pending_course_slot))
        {
            char name_copy[32];
            const char *name = app->getCourseName(pending_course_slot);
            snprintf(name_copy, sizeof(name_copy), "%s", name);
            app->deleteCoursePreset(pending_course_slot);
            updateCourseListDisplay();
            refreshValueTexts();
            easy_flipper_dialog("Course Deleted", name_copy);
        }
        else
        {
            easy_flipper_dialog("Empty Slot", "Nothing to remove.");
        }
        break;
    }
}

bool GolfScoreSettings::startCourseNameInput(uint8_t slot)
{
    GolfScoreApp *app = static_cast<GolfScoreApp *>(appContext);
    if (!app)
    {
        return false;
    }

    if (text_input_buffer || text_input_temp_buffer)
    {
        freeTextInput();
    }

    pending_course_slot = slot;

    text_input_buffer = std::make_unique<char[]>(text_input_buffer_size);
    text_input_temp_buffer = std::make_unique<char[]>(text_input_buffer_size);

    if (!text_input_buffer || !text_input_temp_buffer)
    {
        return false;
    }

    std::memset(text_input_buffer.get(), 0, text_input_buffer_size);
    std::memset(text_input_temp_buffer.get(), 0, text_input_buffer_size);

    if (app->courseSlotInUse(slot))
    {
        const char *existing = app->getCourseName(slot);
        if (existing && existing[0] != '\0')
        {
            strncpy(text_input_temp_buffer.get(), existing, text_input_buffer_size - 1);
            text_input_temp_buffer[text_input_buffer_size - 1] = '\0';
        }
    }
    else
    {
        snprintf(text_input_temp_buffer.get(), text_input_buffer_size, "Course %u", static_cast<unsigned>(slot + 1));
    }

    char header[24];
    snprintf(header, sizeof(header), "Save Slot %u", static_cast<unsigned>(slot + 1));

#ifndef FW_ORIGIN_Momentum
    bool ok = easy_flipper_set_uart_text_input(&text_input, GolfScoreViewTextInput, header, text_input_temp_buffer.get(), text_input_buffer_size,
                                               textUpdatedCourseCallback, callbackToCourseList, view_dispatcher_ref, this);
#else
    bool ok = easy_flipper_set_text_input(&text_input, GolfScoreViewTextInput, header, text_input_temp_buffer.get(), text_input_buffer_size,
                                          textUpdatedCourseCallback, callbackToCourseList, view_dispatcher_ref, this);
#endif

    if (!ok)
    {
        freeTextInput();
        return false;
    }

    if (view_dispatcher_ref && *view_dispatcher_ref)
    {
        view_dispatcher_switch_to_view(*view_dispatcher_ref, GolfScoreViewTextInput);
        return true;
    }

    return false;
}

void GolfScoreSettings::textUpdatedCourseCallback(void *context)
{
    auto *settings = static_cast<GolfScoreSettings *>(context);
    if (settings)
    {
        settings->textUpdatedCourseName();
    }
}

void GolfScoreSettings::textUpdatedCourseName()
{
    GolfScoreApp *app = static_cast<GolfScoreApp *>(appContext);
    if (!app)
    {
        return;
    }

    if (!text_input_buffer || !text_input_temp_buffer)
    {
        return;
    }

    strncpy(text_input_buffer.get(), text_input_temp_buffer.get(), text_input_buffer_size - 1);
    text_input_buffer[text_input_buffer_size - 1] = '\0';

    const char *name = text_input_buffer.get();
    if (!app->saveCoursePreset(pending_course_slot, name))
    {
        easy_flipper_dialog("Save Failed", "Could not save course.");
    }
    else
    {
        easy_flipper_dialog("Course Saved", app->getCourseName(pending_course_slot));
    }

    updateCourseListDisplay();
    refreshValueTexts();

    if (view_dispatcher_ref && *view_dispatcher_ref)
    {
        view_dispatcher_switch_to_view(*view_dispatcher_ref, GolfScoreViewCourseList);
    }
}


uint8_t GolfScoreSettings::parToIndex(uint8_t par) const
{
    if (par < GolfScoreMinPar)
    {
        par = GolfScoreMinPar;
    }
    else if (par > GolfScoreMaxPar)
    {
        par = GolfScoreMaxPar;
    }
    return static_cast<uint8_t>(par - GolfScoreMinPar);
}

uint8_t GolfScoreSettings::indexToPar(uint8_t index) const
{
    if (index >= HoleParOptionCount)
    {
        index = static_cast<uint8_t>(HoleParOptionCount - 1);
    }
    return static_cast<uint8_t>(GolfScoreMinPar + index);
}
