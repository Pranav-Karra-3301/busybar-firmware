/**
 * @file ca_storage.h
 * @brief Certificate Authority (CA) storage API.
 */
#pragma once

/**
 * @brief The string key for CaStorage instance access
 *
 * Get the instance pointer by calling `furi_record_open(RECORD_CA_STORAGE);`
 */
#define RECORD_CA_STORAGE "ca_storage"

typedef struct CaStorage CaStorage;

/**
 * @brief Get the pointer to the CA certificate bundle in PEM format.
 *
 * @note The return value is guaranteed to be valid throughout the firmware's lifetime
 *       and the string it is pointing to is guaranteed to be zero-terminated.
 *
 * @param[in] instance Pointer the the CaStorage instance
 * @returns pointer to the CA bundle string
 */
const char* ca_storage_get_pem_bundle(CaStorage* instance);
