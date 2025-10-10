#pragma once

#include "ble_intercom_types.h"

typedef void (*BleEngineCommandPreProcess)(BleIntercomFrameGeneric* frame, void* context);
typedef void (*BleEngineCommandPostProcess)(BleIntercomFrameGeneric* frame, void* context);

typedef bool (*BleRequestCommandHandler)(BleIntercomFrameGeneric* frame, void* context);
typedef bool (*BleResponseCommandHandler)(BleIntercomFrameGeneric* frame, void* context);

///TODO: rename to BleCommand, when enum will renamed to BleSystemCommand
typedef struct {
    BleRequestCommandHandler request;
    BleResponseCommandHandler response;
} BleCommandItem;

// const BleCommandItem commands[] = {[0] = {.request = }};

typedef struct BleCommandEngine BleCommandEngine;

BleCommandEngine* ble_command_engine_alloc(
    const BleCommandItem* commands,
    uint8_t commands_count,
    BleEngineCommandPreProcess pre_process,
    BleEngineCommandPostProcess post_process);

bool ble_command_engine_run(
    BleCommandEngine* instance,
    BleIntercomFrameGeneric* frame,
    void* context);
