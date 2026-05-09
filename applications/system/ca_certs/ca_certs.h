/**
 * @file ca_certs.h
 * CA Certs keeper
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <furi.h>

typedef struct {
    size_t length;
    uint8_t data[];
} CaCerts;

#define RECORD_CA_CERTS "ca_certs"

#ifdef __cplusplus
}
#endif
