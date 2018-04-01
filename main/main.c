#include <string.h>

#include "freertos/FreeRTOS.h"

#include "esp_wifi.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_event_loop.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

#include "../components/mongoose/mongoose.h"

esp_err_t event_handler(void *ctx, system_event_t *event)
{
    return ESP_OK;
}

static void mg_ev_handler(struct mg_connection *nc, int ev, void *p) {
    switch (ev) {
        case MG_EV_ACCEPT: {
            char addr[32];
            mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr), MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
            printf("Connection %p from %s\n", nc, addr);
            break;
        }
        case MG_EV_HTTP_REQUEST: {
            char addr[32];
            struct http_message *hm = (struct http_message*) p;
            mg_sock_addr_to_str(&nc->sa, addr, sizeof(addr), MG_SOCK_STRINGIFY_IP | MG_SOCK_STRINGIFY_PORT);
            printf("HTTP request from %s: %.*s %.*s\n", addr, (int) hm->method.len, hm->method.p, (int) hm->uri.len, hm->uri.p);
            nc->flags |= MG_F_SEND_AND_CLOSE;
            break;
        }
        case MG_EV_CLOSE: {
            printf("Connection %p closed\n", nc);
            break;
        }
    }
}

void app_main()
{
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

    gpio_set_direction(CONFIG_BLINK_GPIO, GPIO_MODE_OUTPUT);
    int level = 0;
    while (true) {
        mg_mgr_poll(&mgr, 1000);
        gpio_set_level(CONFIG_BLINK_GPIO, level);
        level = !level;
        vTaskDelay(300 / portTICK_PERIOD_MS);
    }
}
