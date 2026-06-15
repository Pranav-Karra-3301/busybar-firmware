#pragma once

#include <gui/gui.h>
#include <gui/scene_manager.h>

#include "scenes/widget_gallery_scenes.h"

typedef enum {
    WidgetGalleryCustomEventIndexMax = 0x80,
    WidgetGalleryCustomEventMax,
} WidgetGalleryCustomEvent;

typedef struct {
    FuriEventLoop* event_loop;
    FuriMessageQueue* input_queue;
    FuriMessageQueue* event_queue;
    SceneManager* scene_manager;
    Gui* gui;
    Widget* windows[GuiDisplayIdMax];
} WidgetGallery;

void widget_gallery_send_custom_event(WidgetGallery* instance, uint32_t event);
