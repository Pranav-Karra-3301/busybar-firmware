#pragma once

#include <assert.h>

#include "FreeRTOSConfig.h"

typedef enum ThreadLocalStoragePointerId {
    ThreadLocalStoragePointerIdLwip,
    ThreadLocalStoragePointerIdJerryscript,

    ThreadLocalStoragePointerIdMax
} ThreadLocalStoragePointerId;

static_assert(ThreadLocalStoragePointerIdMax == configNUM_THREAD_LOCAL_STORAGE_POINTERS - 1);
