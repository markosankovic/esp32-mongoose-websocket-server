#include <string.h>
#include <time.h>
#include <stdlib.h>

#include "freertos/FreeRTOS.h"

#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "stdint.h"

#include "../components/mongoose/mongoose.h"

#define DATA_SIZE 16
#define MSG_DATA_SIZE 32

esp_err_t event_handler(void *ctx, system_event_t *event) {
    return ESP_OK;
}

static int is_websocket(const struct mg_connection *nc) {
    return nc->flags & MG_F_IS_WEBSOCKET;
}

static void broadcast(struct mg_connection *nc, const struct mg_str msg) {
    struct mg_connection *c;
    char addr[32];
    // char buf[32];
    // memcpy(buf, msg.p, msg.len);
    mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr), MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
    for (c = mg_next(nc->mgr, NULL); c != NULL; c = mg_next(nc->mgr, c)) {
        if (is_websocket(c)) {
            mg_send_websocket_frame(c, WEBSOCKET_OP_BINARY, msg.p, msg.len);
        }
    }
}

static void mg_ev_handler(struct mg_connection *nc, int ev, void *ev_data) {
    switch (ev) {
        case MG_EV_ACCEPT: {
            char addr[32];
            mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr), MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
            printf("Connection %p from %s\n", nc, addr);
            break;
        }
        case MG_EV_WEBSOCKET_HANDSHAKE_DONE: {
            broadcast(nc, mg_mk_str("++ joined"));
            break;
        }
        case MG_EV_WEBSOCKET_FRAME: {
            struct websocket_message *wm = (struct websocket_message *) ev_data;
            struct mg_str d = {(char *) wm->data, wm->size};
            broadcast(nc, d);
            break;
        }
        case MG_EV_CLOSE: {
            printf("Connection %p closed\n", nc);
            if (is_websocket(nc)) {
                broadcast(nc, mg_mk_str("-- left"));
            }
            break;
        }
    }
}

void app_main() {
    nvs_flash_init();
    tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_init(event_handler, NULL));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    wifi_config_t sta_config = {
        .sta = {
            .ssid = CONFIG_WIFI_SSID,
            .password = CONFIG_WIFI_PASS,
            .bssid_set = false
        }
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());

    struct mg_mgr mgr;
    struct mg_connection *nc;

    mg_mgr_init(&mgr, NULL);

    nc = mg_bind(&mgr, CONFIG_WSS_PORT, mg_ev_handler);
    if (nc == NULL) {
        printf("Error setting up listener!\n");
        return;
    }
    mg_set_protocol_http_websocket(nc);
    printf("Started on port %s\n", CONFIG_WSS_PORT);

    gpio_set_direction(CONFIG_BLINK_GPIO, GPIO_MODE_OUTPUT);
    int level = 0;
    int val1 = 0;
    int val2 = 0;
    while (true) {
        int i;
        mg_mgr_poll(&mgr, 500);
        gpio_set_level(CONFIG_BLINK_GPIO, level);
        val1 += 32;
        val2 -= 64;
        const int16_t data[DATA_SIZE] = {val1, val2, 1024, 2048, 4096, 8192, 16384, 32767, 0, 0, 0, 0, 0, 0, 0, 0};
        char msg_data[MSG_DATA_SIZE];
        for (i = 0; i < DATA_SIZE; i++) {
            msg_data[i*2] = data[i] &0xFF;
            msg_data[i*2+1] = (data[i] >> 8) & 0xFF;
        }
        for (i = 0; i < MSG_DATA_SIZE; i++) {
            printf("%d ", msg_data[i]);
        }
        printf("\n");
        const struct mg_str msg = {
            .p = msg_data,
            .len = MSG_DATA_SIZE
        };
        broadcast(nc, msg);
        level = !level;
        // vTaskDelay(pdMS_TO_TICKS(100));
    }
    mg_mgr_free(&mgr);
}
