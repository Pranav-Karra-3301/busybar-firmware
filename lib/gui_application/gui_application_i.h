#pragma once

#include "gui_application.h"

#include <furi.h>
#include <gui/gui.h>

#ifdef __cplusplus
extern "C" {
#endif

struct GuiApplication {
    FuriEventLoop* event_loop;
    FuriMessageQueue* event_queue;
    FuriMessageQueue* input_queue;
    SceneManager* scene_manager;
    Widget* windows[GuiDisplayIdMax];
    Gui* gui;
};

void gui_application_init(GuiApplication* instance, const GuiApplicationConfig* config);

void gui_application_clear(GuiApplication* instance);

#ifdef __cplusplus
}
#endif
