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
#include <http_rest_client.h>
#include <cJSON.h>

#define BLINK_PERIOD (1000)
#define BLANK_PERIOD (100)

static const char *TAG = "MAIN";

esp_err_t convert_handler(httpd_req_t *req)
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
        if (request_content[i] == '-')
        {
            gpio_set_level(GPIO_NUM_2, 1);
            vTaskDelay(pdMS_TO_TICKS(BLINK_PERIOD * 3));
            gpio_set_level(GPIO_NUM_2, 0);
            vTaskDelay(pdMS_TO_TICKS(BLANK_PERIOD));
        }
        else if (request_content[i] == '.')
        {
            gpio_set_level(GPIO_NUM_2, 1);
            vTaskDelay(pdMS_TO_TICKS(BLINK_PERIOD));
            gpio_set_level(GPIO_NUM_2, 0);
            vTaskDelay(pdMS_TO_TICKS(BLANK_PERIOD));
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

esp_err_t message_handler(httpd_req_t *req)
{
    ESP_LOGI(TAG, "Received POST request for /message");

    char *request_content = (char *)calloc(req->content_len + 1, sizeof(char));
    if (request_content == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for request content");
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to allocate memory");
        return ESP_ERR_NO_MEM;
    }

    ssize_t ret = httpd_req_recv(req, request_content, req->content_len);
    if (ret <= 0)
    {

        ESP_LOGE(TAG, "Failed to receive request content");
        free(request_content);
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to receive request content");
        return ESP_FAIL;
    }

    request_content[ret] = '\0';

    httpd_req_recv(req, request_content, req->content_len);

    const char *PUSHOVER_ROOT_CA = "-----BEGIN CERTIFICATE-----\n"
                                   "MIIDjjCCAnagAwIBAgIQAzrx5qcRqaC7KGSxHQn65TANBgkqhkiG9w0BAQsFADBh\n"
                                   "MQswCQYDVQQGEwJVUzEVMBMGA1UEChMMRGlnaUNlcnQgSW5jMRkwFwYDVQQLExB3\n"
                                   "d3cuZGlnaWNlcnQuY29tMSAwHgYDVQQDExdEaWdpQ2VydCBHbG9iYWwgUm9vdCBH\n"
                                   "MjAeFw0xMzA4MDExMjAwMDBaFw0zODAxMTUxMjAwMDBaMGExCzAJBgNVBAYTAlVT\n"
                                   "MRUwEwYDVQQKEwxEaWdpQ2VydCBJbmMxGTAXBgNVBAsTEHd3dy5kaWdpY2VydC5j\n"
                                   "b20xIDAeBgNVBAMTF0RpZ2lDZXJ0IEdsb2JhbCBSb290IEcyMIIBIjANBgkqhkiG\n"
                                   "9w0BAQEFAAOCAQ8AMIIBCgKCAQEAuzfNNNx7a8myaJCtSnX/RrohCgiN9RlUyfuI\n"
                                   "2/Ou8jqJkTx65qsGGmvPrC3oXgkkRLpimn7Wo6h+4FR1IAWsULecYxpsMNzaHxmx\n"
                                   "1x7e/dfgy5SDN67sH0NO3Xss0r0upS/kqbitOtSZpLYl6ZtrAGCSYP9PIUkY92eQ\n"
                                   "q2EGnI/yuum06ZIya7XzV+hdG82MHauVBJVJ8zUtluNJbd134/tJS7SsVQepj5Wz\n"
                                   "tCO7TG1F8PapspUwtP1MVYwnSlcUfIKdzXOS0xZKBgyMUNGPHgm+F6HmIcr9g+UQ\n"
                                   "vIOlCsRnKPZzFBQ9RnbDhxSJITRNrw9FDKZJobq7nMWxM4MphQIDAQABo0IwQDAP\n"
                                   "BgNVHRMBAf8EBTADAQH/MA4GA1UdDwEB/wQEAwIBhjAdBgNVHQ4EFgQUTiJUIBiV\n"
                                   "5uNu5g/6+rkS7QYXjzkwDQYJKoZIhvcNAQELBQADggEBAGBnKJRvDkhj6zHd6mcY\n"
                                   "1Yl9PMWLSn/pvtsrF9+wX3N3KjITOYFnQoQj8kVnNeyIv/iPsGEMNKSuIEyExtv4\n"
                                   "NeF22d+mQrvHRAiGfzZ0JFrabA0UWTW98kndth/Jsw1HKj2ZL7tcu7XUIOGZX1NG\n"
                                   "Fdtom/DzMNU+MeKNhJ7jitralj41E6Vf8PlwUHBHQRFXGU7Aj64GxJUTFy8bJZ91\n"
                                   "8rGOmaFvE7FBcf6IKshPECBV1/MUReXgRPTqh5Uykw7+U0b6LJ3/iyK5S9kJRaTe\n"
                                   "pLiaWN0bfVKfjllDiIGknibVb63dDcY3fe0Dkhvld1927jyNxF1WW6LZZm6zNTfl\n"
                                   "MrY=\n"
                                   "-----END CERTIFICATE-----\n";

    const char *userToken = "ue9gui8abwvpgtu71kico75kkqs7r4";
    const char *apiToken = "ahx1fn2vhkea1z1zr6bhwo7m6b99r4";
    const char *pushoverApiEndpoint = "https://api.pushover.net/1/messages.json";

    esp_err_t wifi_init_err = esp_wifi_start();
    if (wifi_init_err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to initialize Wi-Fi: %d", wifi_init_err);
        free(request_content);
        return ESP_FAIL;
    }

    cJSON *notification = cJSON_CreateObject();
    cJSON_AddStringToObject(notification, "token", apiToken);
    cJSON_AddStringToObject(notification, "user", userToken);
    cJSON_AddStringToObject(notification, "message", request_content);
    cJSON_AddStringToObject(notification, "title", "ESP32 Notification");
    cJSON_AddStringToObject(notification, "device", "esp32");

    char *jsonStringNotification = cJSON_PrintUnformatted(notification);
    ESP_LOGE(TAG, "JSON: %s", jsonStringNotification);
    cJSON_Delete(notification);

    esp_http_client_config_t config = {
        .url = pushoverApiEndpoint,
        .cert_pem = PUSHOVER_ROOT_CA,
        .method = HTTP_METHOD_POST,
        .buffer_size = strlen(jsonStringNotification),
        .query = jsonStringNotification};

    if (config.cert_pem == NULL)
    {
        ESP_LOGE(TAG, "Failed to load root CA certificate");
        free(jsonStringNotification);
        free(request_content);
        return ESP_FAIL;
    }

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL)
    {
        ESP_LOGE(TAG, "Failed to initialize HTTP client");
        free(jsonStringNotification);
        free(request_content);
        return ESP_FAIL;
    }
    ESP_LOGE(TAG, "client initialized");

    esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded");

    char *payload = NULL;
    asprintf(&payload, "token=%s&user=%s&device=%s&title=%s&message=%s",
             apiToken, userToken, "esp32", "ESP32 Notification", request_content);

    esp_http_client_set_post_field(client, payload, strlen(payload));

    // Send the HTTP request
    esp_err_t http_err = esp_http_client_perform(client);
    if (http_err != ESP_OK)
    {
        ESP_LOGE(TAG, "HTTP POST request failed: %d", http_err);
    }
    else
    {
        ESP_LOGI(TAG, "HTTP POST request success");
    }

    int status_code = esp_http_client_get_status_code(client);
    ESP_LOGI(TAG, "HTTP Status = %d", status_code);

    httpd_resp_send(req, NULL, 0);

    free(request_content);
    free(jsonStringNotification);
    free(payload);

    return ESP_OK;
}

httpd_uri_t handler_1 = {
    .uri = "/convert",
    .method = HTTP_POST,
    .handler = convert_handler,
    .user_ctx = NULL};

httpd_uri_t handler_2 = {
    .uri = "/message",
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
        ESP_ERROR_CHECK(httpd_register_uri_handler(httpd_handle, &handler_2));
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
