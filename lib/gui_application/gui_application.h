/**
 * @file gui_application.h
 * @brief GUI application library.
 */
#pragma once

#include <gui/scene_manager.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct GuiApplication GuiApplication;

typedef struct {
    const SceneArray* const scenes;
    uint32_t scenes_count;
} GuiApplicationConfig;

GuiApplication* gui_application_alloc(const GuiApplicationConfig* config);

void gui_application_free(GuiApplication* instance);

void gui_application_run(GuiApplication* instance);

void gui_application_send_custom_event(GuiApplication* instance, uint32_t event);

#ifdef __cplusplus
}
#endif
