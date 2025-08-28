#pragma once

#include <gui/scene_manager.h>

typedef enum {
    SettingsAppSceneIdStart,
    SettingsAppSceneIdMain,
    SettingsAppSceneIdSound,
    SettingsAppSceneIdBrightness,
    SettingsAppSceneIdLanguage,

    SettingsAppSceneIdsCount
} SettingsAppSceneId;

extern const Scene* const settings_scenes[SettingsAppSceneIdsCount];
