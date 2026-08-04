/**
 * @file tokens.h
 * 
 * Manages HTTP access tokens
 */

#pragma once

#include <furi.h>
#include <time.h>

#define RECORD_TOKENS          "tokens"
#define TOKENS_LENGTH          (32)
#define TOKENS_SHORT_ID_LENGTH (8)

typedef struct Tokens Tokens;

/**
 * @brief Generates a new HTTP API access token.
 * 
 * @param[inout] tokens Tokens service
 * @param[in] owner Human-readable name for the token
 * @param[inout] full_token_out Newly created token in full
 */
void tokens_mint(Tokens* tokens, const char* owner, FuriString* full_token_out);

/**
 * @brief Information about an HTTP API access token
 */
typedef struct {
    char* short_id;
    char* display_id;
    char* owner;
    char* full_token_hash;
    time_t created_at;
    time_t last_used_at;
} TokensEntry;

/**
 * @brief Callback for token entry in `tokens_list`.
 * 
 * User code should call `tokens_entry_free` to free the provided entry.
 */
typedef void (*TokensListCallback)(const TokensEntry* entry, void* context);

/**
 * @brief List all HTTP API access tokens
 * 
 * @param[in] tokens Tokens service
 * @param[in] callback Callback with info about one token
 * @param[in] context Custom context to pass to callback verbatim
 */
void tokens_list(Tokens* tokens, TokensListCallback callback, void* context);

/**
 * @brief Revoke one HTTP API access token
 * 
 * @param[inout] tokens Tokens service
 * @param[in] short_id Token short ID
 * 
 * @returns `false` if requested token was not found
 */
bool tokens_revoke(Tokens* tokens, const char* short_id);

/**
 * @brief Revoke all HTTP API access tokens
 * 
 * @param[inout] tokens Tokens service
 */
void tokens_reset_all(Tokens* tokens);

/**
 * @brief Check that the provided access token is valid, and update its
 * `last_used_at` property
 * 
 * @param[inout] tokens Tokens service
 * @param[in] full_token Full access token
 * 
 * @returns `true` if access should be granted
 */
bool tokens_validate_and_record_usage(Tokens* tokens, const char* full_token);
