#pragma once

#include <furi.h>

/**
 * UART channels
 */
typedef enum {
    FuriHalSerialIdUsart0,
    FuriHalSerialIdUart1,
    FuriHalSerialIdUlpuart,
    FuriHalSerialIdMax,
} FuriHalSerialId;

typedef struct FuriHalSerialHandle FuriHalSerialHandle;
