#pragma once

#include <array>
#include <memory>

#include "easy_flipper/easy_flipper.h"
#include "golf_score_config.hpp"

class GolfScoreApp;

typedef enum
{
    SettingsViewPlayers = 0,
    SettingsViewHoles = 1,
    SettingsViewReset = 2,
    SettingsViewPlayerName1 = 3,
    SettingsViewPlayerName2 = 4,
    SettingsViewPlayerName3 = 5,
    SettingsViewPlayerName4 = 6,
} SettingsViewChoice;

class GolfScoreSettings
{
private:
    void *appContext;
#ifndef FW_ORIGIN_Momentum
    UART_TextInput *text_input = nullptr;
#else
    TextInput *text_input = nullptr;
#endif
    std::unique_ptr<char[]> text_input_buffer;
    uint32_t text_input_buffer_size = 32;
    std::unique_ptr<char[]> text_input_temp_buffer;
    VariableItemList *variable_item_list = nullptr;
    VariableItem *variable_item_player_count = nullptr;
    VariableItem *variable_item_hole_count = nullptr;
    VariableItem *variable_item_reset = nullptr;
    std::array<VariableItem *, GolfScoreMaxPlayers> variable_item_player_names{};
    ViewDispatcher **view_dispatcher_ref;

    static uint32_t callbackToSubmenu(void *context);
    static uint32_t callbackToSettings(void *context);
    void freeTextInput();
    bool initTextInput(uint32_t view);
    static void settingsItemSelectedCallback(void *context, uint32_t index);
    bool startTextInput(uint32_t view);
    void textUpdated(uint32_t view);
    void refreshValueTexts();
    static void textUpdatedPlayer0Callback(void *context);
    static void textUpdatedPlayer1Callback(void *context);
    static void textUpdatedPlayer2Callback(void *context);
    static void textUpdatedPlayer3Callback(void *context);

public:
    GolfScoreSettings(ViewDispatcher **view_dispatcher, void *appContext);
    ~GolfScoreSettings();

    void settingsItemSelected(uint32_t index);
};
