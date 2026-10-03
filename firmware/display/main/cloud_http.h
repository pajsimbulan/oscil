#pragma once
#include <stddef.h>
#include "esp_err.h"
#include "esp_http_client.h"

// One JSON request to Supabase. path starts with "/", e.g. "/auth/v1/token?grant_type=password".
// bearer: the user's access token, or NULL. prefer: a PostgREST "Prefer" header, or NULL.
// The reply lands in resp (truncated to n - 1 bytes); *status gets the HTTP status, -1 if none.
// Blocks for up to 15 s: never call it from the LVGL task.
esp_err_t cloud_json(esp_http_client_method_t method, const char *path, const char *bearer,
                     const char *prefer, const char *body, int *status, char *resp, size_t n);