#include "about/about.hpp"

GolfScoreAbout::GolfScoreAbout(ViewDispatcher **viewDispatcher) : widget(nullptr), viewDispatcherRef(viewDispatcher)
{
    easy_flipper_set_widget(&widget, GolfScoreViewAbout,
                            "Track golf rounds for up to 4 players.\n"
                            "Use Round Setup to adjust players and reset scores.\n\n"
                            "Template adapted for score keeping.",
                            callbackToSubmenu, viewDispatcherRef);
}

GolfScoreAbout::~GolfScoreAbout()
{
    if (widget && viewDispatcherRef && *viewDispatcherRef)
    {
        view_dispatcher_remove_view(*viewDispatcherRef, GolfScoreViewAbout);
        widget_free(widget);
        widget = nullptr;
    }
}

uint32_t GolfScoreAbout::callbackToSubmenu(void *context)
{
    UNUSED(context);
    return GolfScoreViewSubmenu;
}
