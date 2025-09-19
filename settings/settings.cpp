#include "settings.hpp"
#include "app.hpp"

#include <cstdio>
#include <cstring>
#include <memory>

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

    variable_item_player_count = nullptr;
    variable_item_hole_count = nullptr;
    variable_item_reset = nullptr;
    variable_item_player_names.fill(nullptr);
}

uint32_t GolfScoreSettings::callbackToSettings(void *context)
{
    UNUSED(context);
    return GolfScoreViewSettings;
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
