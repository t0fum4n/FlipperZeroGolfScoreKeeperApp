#pragma once
#include "easy_flipper/easy_flipper.h"

class GolfScoreAbout
{
private:
    Widget *widget;
    ViewDispatcher **viewDispatcherRef;

    static constexpr const uint32_t GolfScoreViewSubmenu = 1; // View ID for submenu
    static constexpr const uint32_t GolfScoreViewAbout = 2;   // View ID for about

    static uint32_t callbackToSubmenu(void *context);

public:
    GolfScoreAbout(ViewDispatcher **viewDispatcher);
    ~GolfScoreAbout();
};
