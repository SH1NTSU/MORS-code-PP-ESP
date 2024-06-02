#include <stdio.h>
#include <driver/gpio.h>
#include <freertos/FreeRTOS.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <esp_wifi.h>
#include <esp_http_server.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <string.h>

#define BLINK_PERIOD (1000)

static const char *TAG = "MAIN";

esp_err_t message_handler(httpd_req_t *req)
{
    char *request_content = (char *)calloc(req->content_len + 1, sizeof(char));
    if (request_content == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for request content");
        return ESP_ERR_NO_MEM;
    }

    httpd_req_recv(req, request_content, req->content_len);
    size_t length = strlen(request_content);
    for (size_t i = 0; i < length; i++)
    {
        if (request_content[i] == '_')
        {
            gpio_set_level(GPIO_NUM_2, 1);
            vTaskDelay(pdMS_TO_TICKS(BLINK_PERIOD * 3));
            gpio_set_level(GPIO_NUM_2, 0);
            vTaskDelay(pdMS_TO_TICKS(BLINK_PERIOD));
        }
        else if (request_content[i] == '.')
        {
            gpio_set_level(GPIO_NUM_2, 1);
            vTaskDelay(pdMS_TO_TICKS(BLINK_PERIOD));
            gpio_set_level(GPIO_NUM_2, 0);
            vTaskDelay(pdMS_TO_TICKS(BLINK_PERIOD));
        }
        else if (request_content[i] == ' ')
        {
            vTaskDelay(pdMS_TO_TICKS(BLINK_PERIOD * 3));
        }
    }

    free(request_content);
    httpd_resp_send(req, NULL, 0);
    return ESP_OK;
}

httpd_uri_t handler_1 = {
    .uri = "/convert",
    .method = HTTP_POST,
    .handler = message_handler,
    .user_ctx = NULL};

static void got_ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(TAG, "Got IP Address: " IPSTR, IP2STR(&event->ip_info.ip));
}

void app_main(void)
{
    ESP_ERROR_CHECK(gpio_set_direction(GPIO_NUM_2, GPIO_MODE_OUTPUT));

    esp_log_level_set(TAG, ESP_LOG_DEBUG);

    esp_err_t ret = nvs_flash_init();
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize NVS flash: %s", esp_err_to_name(ret));
        return;
    }

    ESP_ERROR_CHECK(esp_netif_init());

    ESP_ERROR_CHECK(esp_event_loop_create_default());

    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    if (!sta_netif)
    {
        ESP_LOGE(TAG, "Failed to create default WiFi station");
        return;
    }

    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&wifi_init_cfg));

    wifi_config_t wifi_cfg = {
        .sta = {
            .ssid = "UPC7912814",
            .password = "Jv8zspbckxHs"}};
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg));
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());

    esp_event_handler_instance_t instance_any_id;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &got_ip_event_handler, NULL, &instance_any_id));

    httpd_config_t httpd_cfg = HTTPD_DEFAULT_CONFIG();
    httpd_handle_t httpd_handle;

    if (httpd_start(&httpd_handle, &httpd_cfg) == ESP_OK)
    {
        ESP_LOGI(TAG, "HTTP server started");
        ESP_ERROR_CHECK(httpd_register_uri_handler(httpd_handle, &handler_1));
    }
    else
    {
        ESP_LOGE(TAG, "Failed to start HTTP server");
    }

    ESP_LOGI(TAG, "Waiting for Wi-Fi connection...");
    vTaskDelay(pdMS_TO_TICKS(10000));

    wifi_ap_record_t ap_info;
    esp_err_t wifi_ret = esp_wifi_sta_get_ap_info(&ap_info);
    if (wifi_ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Connected to Wi-Fi network: %s", ap_info.ssid);
    }
    else
    {
        ESP_LOGE(TAG, "Failed to connect to Wi-Fi network");
    }
}
