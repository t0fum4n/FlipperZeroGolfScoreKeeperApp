#pragma once

#include <cstdint>

#include "easy_flipper/easy_flipper.h"

class GolfScoreApp;

class GolfScoreScorecard
{
private:
    void *appContext;
    bool shouldReturnToMenu;
    uint8_t activeHole = 0;
    uint8_t activePlayer = 0;

    void clampSelection();

public:
    GolfScoreScorecard(void *appContext);
    ~GolfScoreScorecard();

    bool isActive() const { return shouldReturnToMenu == false; }
    void updateDraw(Canvas *canvas);
    void updateInput(InputEvent *event);
};
