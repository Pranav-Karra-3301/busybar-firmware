#include "widget_gallery_scenes.h"

extern const Scene widget_gallery_scene_start;
extern const Scene widget_gallery_scene_widget;
extern const Scene widget_gallery_scene_label;
extern const Scene widget_gallery_scene_image;
extern const Scene widget_gallery_scene_canvas;
extern const Scene widget_gallery_scene_menu;
extern const Scene widget_gallery_scene_submenu;
extern const Scene widget_gallery_scene_var_item_list;
extern const Scene widget_gallery_scene_anim_player;

const Scene* const widget_gallery_scenes[WidgetGallerySceneIdMax] = {
    [WidgetGallerySceneIdStart] = &widget_gallery_scene_start,
    [WidgetGallerySceneIdWidget] = &widget_gallery_scene_widget,
    [WidgetGallerySceneIdLabel] = &widget_gallery_scene_label,
    [WidgetGallerySceneIdImage] = &widget_gallery_scene_image,
    [WidgetGallerySceneIdCanvas] = &widget_gallery_scene_canvas,
    [WidgetGallerySceneIdMenu] = &widget_gallery_scene_menu,
    [WidgetGallerySceneIdSubmenu] = &widget_gallery_scene_submenu,
    [WidgetGallerySceneIdVarItemList] = &widget_gallery_scene_var_item_list,
    [WidgetGallerySceneIdAnimPlayer] = &widget_gallery_scene_anim_player,
};
