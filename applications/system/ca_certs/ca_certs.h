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

/**
 * @brief Gets the contents of the CA bundle file
 * 
 * @returns Structure with contents, or `NULL` if file hasn't been loaded yet or
 * will never load due to an error.
 */
const CaCerts* ca_certs_get(void);

#ifdef __cplusplus
}
#endif
