#pragma once

#include <gui/scene_manager.h>

typedef enum {
    WidgetGallerySceneIdStart,
    WidgetGallerySceneIdWidget,
    WidgetGallerySceneIdLabel,
    WidgetGallerySceneIdImage,
    WidgetGallerySceneIdCanvas,
    WidgetGallerySceneIdMenu,
    WidgetGallerySceneIdSubmenu,
    WidgetGallerySceneIdVarItemList,
    WidgetGallerySceneIdAnimPlayer,

    WidgetGallerySceneIdMax,
} WidgetGallerySceneId;

extern const Scene* const widget_gallery_scenes[WidgetGallerySceneIdMax];
