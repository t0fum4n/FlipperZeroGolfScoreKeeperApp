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
    SettingsViewPars = 3,
    SettingsViewLoadCourse = 4,
    SettingsViewSaveCourse = 5,
    SettingsViewDeleteCourse = 6,
    SettingsViewPlayerName1 = 7,
    SettingsViewPlayerName2 = 8,
    SettingsViewPlayerName3 = 9,
    SettingsViewPlayerName4 = 10,
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
    VariableItem *variable_item_par_overview = nullptr;
    VariableItem *variable_item_load_course = nullptr;
    VariableItem *variable_item_save_course = nullptr;
    VariableItem *variable_item_delete_course = nullptr;
    VariableItemList *par_variable_item_list = nullptr;
    VariableItem *par_item_hole_selector = nullptr;
    VariableItem *par_item_value = nullptr;
    VariableItemList *course_variable_item_list = nullptr;
    std::array<VariableItem *, GolfScoreMaxPlayers> variable_item_player_names{};
    std::array<VariableItem *, GolfScoreMaxCourses> course_items{};
    struct ParItemContext
    {
        GolfScoreSettings *settings = nullptr;
        bool hole_selector = false;
    } par_hole_context{}, par_value_context{};
    uint8_t selected_par_hole = 0;
    std::array<char, 24> par_hole_label{};
    std::array<char, 24> par_value_label{};
    std::array<char, 32> par_summary_text{};
    static constexpr uint8_t HoleParOptionCount = GolfScoreMaxPar - GolfScoreMinPar + 1;
    bool suppress_par_updates = false;
    enum class CourseSelectionMode
    {
        Load,
        Save,
        Delete,
    };
    CourseSelectionMode course_selection_mode = CourseSelectionMode::Load;
    uint8_t pending_course_slot = 0;
    ViewDispatcher **view_dispatcher_ref;

    static uint32_t callbackToSubmenu(void *context);
    static uint32_t callbackToSettings(void *context);
    static uint32_t callbackToCourseList(void *context);
    void freeTextInput();
    bool initTextInput(uint32_t view);
    static void settingsItemSelectedCallback(void *context, uint32_t index);
    bool startTextInput(uint32_t view);
    void textUpdated(uint32_t view);
    void refreshValueTexts();
    void refreshParValues();
    bool ensureParList();
    bool ensureCourseList();
    void startCourseSelection(CourseSelectionMode mode);
    void updateCourseListDisplay();
    static void textUpdatedPlayer0Callback(void *context);
    static void textUpdatedPlayer1Callback(void *context);
    static void textUpdatedPlayer2Callback(void *context);
    static void textUpdatedPlayer3Callback(void *context);
    void updateParEditorDisplay();
    static void parHoleSelectorChangedCallback(VariableItem *item);
    static void parValueChangedCallback(VariableItem *item);
    void parHoleSelectorChanged(VariableItem *item);
    void parValueChanged(VariableItem *item);
    static void courseItemSelectedCallback(void *context, uint32_t index);
    void courseItemSelected(uint32_t index);
    bool startCourseNameInput(uint8_t slot);
    static void textUpdatedCourseCallback(void *context);
    void textUpdatedCourseName();
    void updateParSummary();
    uint8_t parToIndex(uint8_t par) const;
    uint8_t indexToPar(uint8_t index) const;

public:
    GolfScoreSettings(ViewDispatcher **view_dispatcher, void *appContext);
    ~GolfScoreSettings();

    void settingsItemSelected(uint32_t index);
};
