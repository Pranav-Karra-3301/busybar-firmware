/**
 * @file l10n.h
 * @brief Localization service
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "l10n_common.h"

#include <furi.h>

#define RECORD_L10N "l10n"

typedef struct L10nSrv L10nSrv;
typedef struct L10nContext L10nContext;

/**
 * @brief Gets information about a locale
 * 
 * @param[in] id Locale ID
 * 
 * @returns Locale info
 */
const L10nLocaleInfo* l10n_locale_info(L10nLocale id);

/**
 * @brief Sets the global locale
 * 
 * @warning The locale will be applied after a reboot, not immediately.
 * 
 * @param[in] service Service handle
 * @param[in] id Locale ID
 */
void l10n_set_global_locale(L10nSrv* service, L10nLocale id);

/**
 * @brief Gets the global locale
 * 
 * @param[in] service Service handle
 * 
 * @returns id Locale ID
 */
L10nLocale l10n_get_global_locale(L10nSrv* service);

/**
 * @brief Where to fetch the templates from
 */
typedef enum {
    L10nSourceStorage, // <! Fetch templates from Storage
    L10nSourceFlash, // <! Fetch templates from the flash
} L10nSource;

/**
 * @brief Creates a translation context
 * 
 * @param[in] service Localization handle
 * @param[in] app_id_or_path Application id (when `L10nSourceFlash`) or path to
 *                           directory containing the locale files (when
 *                           `L10nSourceStorage`)
 * @param[in] source Where to get the templates from
 * 
 * @returns Translation context
 */
L10nContext* l10n_context_open(L10nSrv* service, const char* app_id_or_path, L10nSource source);

/**
 * @brief Closes a translation context
 * 
 * @param[in] context Context handle
 */
void l10n_context_close(L10nContext* context);

/**
 * @brief Fetches and fills in a translation template
 * 
 * @param[in] context Context handle
 * @param[in] key Key to fetch the template
 * @param[in] ... Args to paste into the template
 * 
 * @warning The returned string is only valid until the next call to this
 * function
 * 
 * @returns C-string. Valid until the next call to this function
 */
const char* l10n_get(L10nContext* context, L10nKey key, ...);

#ifdef __cplusplus
}
#endif
