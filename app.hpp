#pragma once

#include <array>
#include <cstdint>
#include <memory>

#include "font/font.h"
#include "easy_flipper/easy_flipper.h"
#include "golf_score_config.hpp"

#define TAG "Golf Score"
#define VERSION "1.0"
#define VERSION_TAG TAG " " VERSION
#define APP_ID "golf_score"

typedef enum
{
    GolfScoreMenuRun = 0,
    GolfScoreMenuSettings = 1,
    GolfScoreMenuAbout = 2,
} GolfScoreMenuIndex;

typedef enum
{
    GolfScoreViewMain = 0,
    GolfScoreViewSubmenu = 1,
    GolfScoreViewAbout = 2,
    GolfScoreViewSettings = 3,
    GolfScoreViewTextInput = 4,
    GolfScoreViewParSettings = 5,
} GolfScoreView;

class GolfScoreScorecard;
class GolfScoreSettings;
class GolfScoreAbout;

class GolfScoreApp
{
public:
    static constexpr size_t MaxPlayers = GolfScoreMaxPlayers;
    static constexpr size_t MaxHoles = GolfScoreMaxHoles;
    static constexpr size_t MaxNameLength = GolfScoreMaxNameLength;

private:
    struct PersistentState
    {
        uint8_t version = 0;
        uint8_t playerCount = 1;
        uint8_t holeCount = 9;
        uint8_t reserved = 0;
        std::array<std::array<char, MaxNameLength>, MaxPlayers> playerNames{};
        std::array<std::array<uint8_t, MaxHoles>, MaxPlayers> strokes{};
        std::array<uint8_t, MaxHoles> par{};
    };

    std::unique_ptr<GolfScoreAbout> about;          // About view instance
    std::unique_ptr<GolfScoreScorecard> scorecard;  // Scorecard view instance
    std::unique_ptr<GolfScoreSettings> settings;    // Settings view instance
    Submenu *submenu = nullptr;                   // Application submenu
    FuriTimer *timer = nullptr;                   // Viewport refresh timer
    PersistentState state{};                      // Persisted round data

    static uint32_t callbackExitApp(void *context);
    void callbackSubmenuChoices(uint32_t index);
    void createAppDataPath();
    static void submenuChoicesCallback(void *context, uint32_t index);
    static void timerCallback(void *context);
    void applyDefaults();
    void loadState();
    void saveState() const;
    void ensureName(uint8_t index);
    bool writeStateToFile(const PersistentState &data) const;
    bool readStateFromFile(PersistentState &data) const;

public:
    GolfScoreApp();
    ~GolfScoreApp();

    Gui *gui = nullptr;                       // GUI instance for the app
    ViewDispatcher *viewDispatcher = nullptr; // View dispatcher for managing views
    ViewPort *viewPort = nullptr;             // Viewport for drawing and input handling

    void runDispatcher();
    static void viewPortDraw(Canvas *canvas, void *context);
    static void viewPortInput(InputEvent *event, void *context);

    uint8_t getHoleCount() const noexcept { return state.holeCount; }
    uint8_t getPlayerCount() const noexcept { return state.playerCount; }
    const char *getPlayerName(uint8_t index) const;
    uint8_t getScore(uint8_t player, uint8_t hole) const;
    uint16_t getTotalScore(uint8_t player) const;
    int16_t getRelativeToPar(uint8_t player) const;
    uint8_t getPar(uint8_t hole) const;
    uint16_t getCoursePar() const;
    uint8_t getPlayedHoleCount(uint8_t player) const;

    void adjustScore(uint8_t player, uint8_t hole, int8_t delta);
    void resetScores();
    void cyclePlayerCount();
    void setPlayerCount(uint8_t count);
    void toggleHoleCount();
    void setHoleCount(uint8_t count);
    void setPlayerName(uint8_t index, const char *name);
    void setPar(uint8_t hole, uint8_t value);
    void requestCanvasRefresh();
};
