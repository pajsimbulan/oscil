#pragma once
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"

// Network calls block: call them from a worker task, never the LVGL task.
// msg gets a short sentence for the screen ("Signed in as paul", "Wrong username or password").
esp_err_t   account_init(void);                  // after settings_init(): loads the session from NVS
bool        account_signed_in(void);
const char *account_username(void);              // "" when signed out
esp_err_t   account_sign_in(const char *user, const char *pass, char *msg, size_t n);
esp_err_t   account_sign_up(const char *user, const char *pass, const char *phrase, char *msg, size_t n);
esp_err_t   account_reset(const char *user, const char *phrase, const char *new_pass, char *msg, size_t n);
void        account_sign_out(void);
// A valid access token and user id, renewed with the refresh token if it has expired.
// ESP_ERR_INVALID_STATE: not signed in, or the session was revoked (sign in again).
esp_err_t   account_token(char *jwt, size_t n, char *uid, size_t un);