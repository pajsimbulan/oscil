#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "cJSON.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "nvs.h"
#include "account.h"
#include "cloud_http.h"

static const char *TAG = "account";

#define NS         "account"            // NVS namespace: user, uid, refresh
#define JWT_MAX    1536
#define RESP_MAX   6144                 // a token reply carries the whole user object

static SemaphoreHandle_t s_lock;        // one network operation at a time; guards everything below
static char  s_user[24], s_uid[40], s_refresh[128];
static char *s_access;                  // JWT_MAX bytes, heap; "" until the first refresh
static int64_t s_expires_us;            // esp_timer time when s_access stops being valid

static void save_session(void)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_str(h, "user", s_user);
    nvs_set_str(h, "uid", s_uid);
    nvs_set_str(h, "refresh", s_refresh);
    nvs_commit(h);
    nvs_close(h);
}

static void clear_session(void)
{
    s_user[0] = s_uid[0] = s_refresh[0] = s_access[0] = 0;
    s_expires_us = 0;
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_erase_all(h);
    nvs_commit(h);
    nvs_close(h);
}

esp_err_t account_init(void)
{
    s_lock = xSemaphoreCreateMutex();
    s_access = calloc(1, JWT_MAX);
    if (!s_lock || !s_access) return ESP_ERR_NO_MEM;
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) == ESP_OK) {
        size_t n = sizeof s_user;    if (nvs_get_str(h, "user", s_user, &n) != ESP_OK) s_user[0] = 0;
        n = sizeof s_uid;            if (nvs_get_str(h, "uid", s_uid, &n) != ESP_OK) s_uid[0] = 0;
        n = sizeof s_refresh;        if (nvs_get_str(h, "refresh", s_refresh, &n) != ESP_OK) s_refresh[0] = 0;
        nvs_close(h);
    }
    if (!s_refresh[0]) s_user[0] = 0;
    if (s_user[0]) ESP_LOGI(TAG, "signed in as %s", s_user);
    else           ESP_LOGI(TAG, "signed out");
    return ESP_OK;
}

bool account_signed_in(void)        { return s_refresh[0] != 0; }
const char *account_username(void)  { return s_user; }

// Store a token reply as the session. Caller holds s_lock. user: NULL keeps the current name.
static bool take_tokens(const char *resp, const char *user)
{
    cJSON *j = cJSON_Parse(resp);
    if (!j) return false;
    const cJSON *at = cJSON_GetObjectItem(j, "access_token");
    const cJSON *rt = cJSON_GetObjectItem(j, "refresh_token");
    const cJSON *ex = cJSON_GetObjectItem(j, "expires_in");
    const cJSON *u  = cJSON_GetObjectItem(j, "user");
    const cJSON *id = u ? cJSON_GetObjectItem(u, "id") : NULL;
    bool ok = cJSON_IsString(at) && cJSON_IsString(rt) && cJSON_IsNumber(ex) && cJSON_IsString(id) &&
              strlen(at->valuestring) < JWT_MAX && strlen(rt->valuestring) < sizeof s_refresh &&
              strlen(id->valuestring) < sizeof s_uid;
    if (ok) {
        strcpy(s_access, at->valuestring);
        strcpy(s_refresh, rt->valuestring);              // Supabase rotates it: keep the new one
        strcpy(s_uid, id->valuestring);
        if (user) strlcpy(s_user, user, sizeof s_user);
        s_expires_us = esp_timer_get_time() + ((int64_t)ex->valuedouble - 60) * 1000000;   // renew a minute early
        save_session();
    }
    cJSON_Delete(j);
    return ok;
}

// The server's own message if there is one: {"error": ...} from our function, {"msg": ...} from Auth
static void reply_text(const char *resp, char *msg, size_t n, const char *fallback)
{
    cJSON *j = cJSON_Parse(resp);
    const cJSON *e = j ? cJSON_GetObjectItem(j, "error") : NULL;
    if (!cJSON_IsString(e) && j) e = cJSON_GetObjectItem(j, "msg");
    snprintf(msg, n, "%s", cJSON_IsString(e) ? e->valuestring : fallback);
    cJSON_Delete(j);
}

static void lower(char *dst, const char *src, size_t n)
{
    size_t i = 0;
    for (; src[i] && i < n - 1; i++) dst[i] = (char)tolower((unsigned char)src[i]);
    dst[i] = 0;
}

static void wipe_free(char *p)                           // passwords were in there
{
    if (p) { memset(p, 0, strlen(p)); free(p); }
}

esp_err_t account_sign_in(const char *user_in, const char *pass, char *msg, size_t n)
{
    char user[24], email[48];
    lower(user, user_in, sizeof user);
    snprintf(email, sizeof email, "%s@oscil.local", user);   // Auth wants an email; nobody sees this one
    cJSON *b = cJSON_CreateObject();
    cJSON_AddStringToObject(b, "email", email);
    cJSON_AddStringToObject(b, "password", pass);
    char *body = cJSON_PrintUnformatted(b);                  // cJSON escapes quotes in passwords
    cJSON_Delete(b);
    char *resp = malloc(RESP_MAX);
    if (!body || !resp) { wipe_free(body); free(resp); snprintf(msg, n, "Out of memory"); return ESP_ERR_NO_MEM; }

    int st;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    esp_err_t e = cloud_json(HTTP_METHOD_POST, "/auth/v1/token?grant_type=password", NULL, NULL,
                             body, &st, resp, RESP_MAX);
    if (e != ESP_OK)                                   snprintf(msg, n, "Can't reach the cloud");
    else if (st == 200 && take_tokens(resp, user))     snprintf(msg, n, "Signed in as %s", user);
    else if (st == 400)                                { e = ESP_FAIL; snprintf(msg, n, "Wrong username or password"); }
    else                                               { e = ESP_FAIL; reply_text(resp, msg, n, "Sign-in failed"); }
    xSemaphoreGive(s_lock);
    wipe_free(body);
    free(resp);
    return e;
}

// POST to the account function. ok_text is shown on success.
static esp_err_t call_account(cJSON *b, char *msg, size_t n, const char *ok_text)
{
    char *body = cJSON_PrintUnformatted(b);
    cJSON_Delete(b);
    if (!body) { snprintf(msg, n, "Out of memory"); return ESP_ERR_NO_MEM; }
    char resp[256];
    int st;
    esp_err_t e = cloud_json(HTTP_METHOD_POST, "/functions/v1/account", NULL, NULL, body, &st, resp, sizeof resp);
    wipe_free(body);
    if (e != ESP_OK) { snprintf(msg, n, "Can't reach the cloud"); return e; }
    if (st != 200)   { reply_text(resp, msg, n, "Request failed"); return ESP_FAIL; }
    snprintf(msg, n, "%s", ok_text);
    return ESP_OK;
}

esp_err_t account_sign_up(const char *user, const char *pass, const char *phrase, char *msg, size_t n)
{
    cJSON *b = cJSON_CreateObject();
    cJSON_AddStringToObject(b, "action", "signup");
    cJSON_AddStringToObject(b, "username", user);
    cJSON_AddStringToObject(b, "password", pass);
    cJSON_AddStringToObject(b, "phrase", phrase);
    esp_err_t e = call_account(b, msg, n, "Account created");
    return e == ESP_OK ? account_sign_in(user, pass, msg, n) : e;     // straight in
}

esp_err_t account_reset(const char *user, const char *phrase, const char *new_pass, char *msg, size_t n)
{
    cJSON *b = cJSON_CreateObject();
    cJSON_AddStringToObject(b, "action", "reset");
    cJSON_AddStringToObject(b, "username", user);
    cJSON_AddStringToObject(b, "phrase", phrase);
    cJSON_AddStringToObject(b, "new_password", new_pass);
    return call_account(b, msg, n, "Password changed. Sign in with the new one");
}

void account_sign_out(void)
{
    xSemaphoreTake(s_lock, portMAX_DELAY);
    if (s_access[0] && esp_timer_get_time() < s_expires_us) {        // revoke on the server, best effort
        char resp[64];
        int st;
        cloud_json(HTTP_METHOD_POST, "/auth/v1/logout", s_access, NULL, NULL, &st, resp, sizeof resp);
    }
    clear_session();
    xSemaphoreGive(s_lock);
    ESP_LOGI(TAG, "signed out");
}

esp_err_t account_token(char *jwt, size_t n, char *uid, size_t un)
{
    jwt[0] = uid[0] = 0;
    xSemaphoreTake(s_lock, portMAX_DELAY);
    esp_err_t e = ESP_OK;
    if (!s_refresh[0]) {
        e = ESP_ERR_INVALID_STATE;
    } else if (!s_access[0] || esp_timer_get_time() > s_expires_us) {
        char body[192];
        snprintf(body, sizeof body, "{\"refresh_token\":\"%s\"}", s_refresh);   // token is URL-safe text
        char *resp = malloc(RESP_MAX);
        int st = -1;
        if (!resp) {
            e = ESP_ERR_NO_MEM;
        } else {
            e = cloud_json(HTTP_METHOD_POST, "/auth/v1/token?grant_type=refresh_token", NULL, NULL,
                           body, &st, resp, RESP_MAX);
            if (e == ESP_OK && !(st == 200 && take_tokens(resp, NULL))) {
                ESP_LOGW(TAG, "refresh refused (HTTP %d)", st);
                if (st == 400 || st == 401) clear_session();          // revoked or expired: sign in again
                e = ESP_ERR_INVALID_STATE;
            }
            free(resp);
        }
    }
    if (e == ESP_OK) {
        strlcpy(jwt, s_access, n);
        strlcpy(uid, s_uid, un);
    }
    xSemaphoreGive(s_lock);
    return e;
}