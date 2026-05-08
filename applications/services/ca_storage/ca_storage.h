#pragma once

#define RECORD_CA_STORAGE "ca_storage"

typedef struct CaStorage CaStorage;

/**
 * @brief Get the pointer to the CA certificate bundle in PEM format.
 *
 * @note The return value is guaranteed to be valid throughout the firmware's lifetime
 *       and the string it is pointing to is guaranteed to be zero-terminated.
 *
 * @param[in] instance
 * @returns pointer to the CA bundle string
 */
const char* ca_storage_get_pem_bundle(CaStorage* instance);
