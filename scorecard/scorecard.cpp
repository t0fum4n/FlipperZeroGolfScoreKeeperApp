#include "scorecard/scorecard.hpp"
#include "app.hpp"

#include <algorithm>
#include <cstdio>
#include <cstring>

GolfScoreScorecard::GolfScoreScorecard(void *appContext) : appContext(appContext), shouldReturnToMenu(false)
{
    clampSelection();
}

GolfScoreScorecard::~GolfScoreScorecard()
{
}

void GolfScoreScorecard::clampSelection()
{
    auto *app = static_cast<GolfScoreApp *>(appContext);
    if (!app)
    {
        return;
    }

    uint8_t playerCount = std::max<uint8_t>(1, app->getPlayerCount());
    if (activePlayer >= playerCount)
    {
        activePlayer = playerCount - 1;
    }

    uint8_t holeCount = std::max<uint8_t>(1, app->getHoleCount());
    if (activeHole >= holeCount)
    {
        activeHole = holeCount - 1;
    }
}

void GolfScoreScorecard::updateDraw(Canvas *canvas)
{
    auto *app = static_cast<GolfScoreApp *>(appContext);
    if (!app || !canvas)
    {
        return;
    }

    clampSelection();

    canvas_clear(canvas);

    canvas_set_font_custom(canvas, FONT_SIZE_LARGE);
    char header[32];
    snprintf(header, sizeof(header), "Hole %u/%u", static_cast<unsigned>(activeHole + 1), static_cast<unsigned>(app->getHoleCount()));
    canvas_draw_str(canvas, 2, 13, header);

    canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
    char par_text[16];
    snprintf(par_text, sizeof(par_text), "Par %u", static_cast<unsigned>(app->getPar(activeHole)));
    canvas_draw_str(canvas, 98, 13, par_text);

    canvas_set_font_custom(canvas, FONT_SIZE_MEDIUM);
    uint8_t y = 26;
    uint8_t playerCount = app->getPlayerCount();
    for (uint8_t index = 0; index < playerCount; ++index)
    {
        bool highlight = index == activePlayer;

        char name[GolfScoreApp::MaxNameLength];
        strncpy(name, app->getPlayerName(index), sizeof(name));
        name[sizeof(name) - 1] = '\0';

        if (strlen(name) > 8)
        {
            name[8] = '\0';
        }

        char holeScore[4];
        uint8_t strokes = app->getScore(index, activeHole);
        if (strokes == 0)
        {
            strcpy(holeScore, "--");
        }
        else
        {
            snprintf(holeScore, sizeof(holeScore), "%u", strokes);
        }

        char totalStr[8];
        uint16_t total = app->getTotalScore(index);
        if (total == 0)
        {
            strcpy(totalStr, "--");
        }
        else
        {
            snprintf(totalStr, sizeof(totalStr), "%u", total);
        }

        char relation[8];
        uint8_t played = app->getPlayedHoleCount(index);
        int16_t rel = app->getRelativeToPar(index);
        if (played == 0)
        {
            strcpy(relation, "--");
        }
        else if (rel == 0)
        {
            strcpy(relation, "E");
        }
        else if (rel > 0)
        {
            snprintf(relation, sizeof(relation), "+%d", rel);
        }
        else
        {
            snprintf(relation, sizeof(relation), "%d", rel);
        }

        char line[64];
        snprintf(line, sizeof(line), "%c%-8s Stk%-2s Tot%-3s %s", highlight ? '>' : ' ', name, holeScore, totalStr, relation);

        if (highlight)
        {
            canvas_set_color(canvas, ColorBlack);
            canvas_draw_box(canvas, 0, y - 9, 128, 11);
            canvas_set_color(canvas, ColorWhite);
        }
        else
        {
            canvas_set_color(canvas, ColorBlack);
        }

        canvas_draw_str(canvas, 2, y, line);

        if (highlight)
        {
            canvas_set_color(canvas, ColorBlack);
        }

        y += 11;
    }

    canvas_set_font_custom(canvas, FONT_SIZE_SMALL);
    char footer[64];
    snprintf(footer, sizeof(footer), "Up/Down +/-   OK next (P%u)", static_cast<unsigned>(activePlayer + 1));
    canvas_draw_str(canvas, 0, 63, footer);
}

void GolfScoreScorecard::updateInput(InputEvent *event)
{
    if (!event)
    {
        return;
    }

    auto *app = static_cast<GolfScoreApp *>(appContext);
    if (!app)
    {
        return;
    }

    clampSelection();

    if (event->type == InputTypeShort || event->type == InputTypeRepeat)
    {
        switch (event->key)
        {
        case InputKeyUp:
            app->adjustScore(activePlayer, activeHole, 1);
            break;
        case InputKeyDown:
            app->adjustScore(activePlayer, activeHole, -1);
            break;
        case InputKeyLeft:
        {
            uint8_t holeCount = std::max<uint8_t>(1, app->getHoleCount());
            activeHole = (activeHole == 0) ? static_cast<uint8_t>(holeCount - 1) : static_cast<uint8_t>(activeHole - 1);
            app->requestCanvasRefresh();
            break;
        }
        case InputKeyRight:
        {
            uint8_t holeCount = std::max<uint8_t>(1, app->getHoleCount());
            activeHole = static_cast<uint8_t>((activeHole + 1) % holeCount);
            app->requestCanvasRefresh();
            break;
        }
        case InputKeyOk:
        {
            uint8_t playerCount = std::max<uint8_t>(1, app->getPlayerCount());
            activePlayer = static_cast<uint8_t>((activePlayer + 1) % playerCount);
            app->requestCanvasRefresh();
            break;
        }
        case InputKeyBack:
        if (app->isRoundComplete() && !app->isRoundSaved())
        {
            if (!app->finishRound())
            {
                easy_flipper_dialog("Round", "Unable to save round.");
            }
        }
        shouldReturnToMenu = true;
        break;
        default:
            break;
        }
    }
    else if (event->type == InputTypeLong && event->key == InputKeyOk)
    {
        int8_t current = static_cast<int8_t>(app->getScore(activePlayer, activeHole));
        if (current > 0)
        {
            app->adjustScore(activePlayer, activeHole, -current);
        }
    }
}
