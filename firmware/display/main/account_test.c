#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "account.h"
#include "account_test.h"
#include "net.h"

static const char *s_u, *s_p;

static void task(void *arg)
{
    xEventGroupWaitBits(g_net_bits, NET_ONLINE, pdFALSE, pdTRUE, portMAX_DELAY);
    //account_sign_out();                  // clear the saved session for this test
    char msg[96];
    if (!account_signed_in()) {
        esp_err_t e = account_sign_in(s_u, s_p, msg, sizeof msg);
        printf("sign in: %s (%s)\n", msg, esp_err_to_name(e));
    } else {
        printf("session from NVS: %s\n", account_username());
    }
    char *jwt = malloc(1536), uid[40];
    esp_err_t e = jwt ? account_token(jwt, 1536, uid, sizeof uid) : ESP_ERR_NO_MEM;
    printf("token: %s, %u characters, user %s\n", esp_err_to_name(e), jwt ? (unsigned)strlen(jwt) : 0, e ? "-" : uid);
    free(jwt);
    vTaskDelete(NULL);
}

void test_account(const char *user, const char *pass)
{
    s_u = user;
    s_p = pass;
    xTaskCreatePinnedToCore(task, "acct_test", 8192, NULL, 3, NULL, 0);
}