#include <stdio.h>
#include <sys/time.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "wifi_sta.h"
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_crt_bundle.h"
#include "https_client.h"

#include "secrets.h"

#define CONNECTION_TIMEOUT_SEC 10
static const char *TAG = "transit-tracker";

static char *URL = "https://ctabustracker.com/bustime/api/v3/getpredictions?key=" 
                         BUS_TRACKER_API_KEY "&rt=151&stpid=68&format=json";

TaskHandle_t send_task_handle = NULL;
TaskHandle_t recv_task_handle = NULL;

esp_err_t connect_to_wifi(void) {
    esp_err_t esp_ret;
    EventGroupHandle_t wifi_event_group = xEventGroupCreate();
    
    // start nvs
    esp_ret = nvs_flash_init();
    if (esp_ret != ESP_OK) {
        if ((esp_ret == ESP_ERR_NVS_NO_FREE_PAGES) ||
            (esp_ret == ESP_ERR_NVS_NEW_VERSION_FOUND)) {
            ESP_ERROR_CHECK(nvs_flash_erase());
            esp_ret = nvs_flash_init();
        }
        if (esp_ret != ESP_OK) {
            ESP_LOGE(TAG, "Error (%d): Could not initialize NVS", esp_ret);
            return ESP_FAIL;
        }
    }

    // initialize TCP/IP stack
    esp_ret = esp_netif_init();
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Error (%d): Couldn't initialize TCP/IP stack", esp_ret);
        return ESP_FAIL;
    }

    // create default event loop
    esp_ret = esp_event_loop_create_default();
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Error (%d): Couldn't create default event loop", esp_ret);
        return ESP_FAIL;
    }

    // initialize wifi
    esp_ret = wifi_sta_init(wifi_event_group);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Error (%d): Failed to initialize wifi", esp_ret);
        return ESP_FAIL;
    }

    // wait for connection (blocking)
    while (!wait_for_wifi(wifi_event_group, CONNECTION_TIMEOUT_SEC)) {
        ESP_LOGI(TAG, "Reinitializing Wi-Fi after a failed attempt...");
        esp_ret = wifi_sta_reconnect();
        if (esp_ret != ESP_OK) {
            ESP_LOGE(TAG, "Error (%d): Failed to connect to Wi-Fi");
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

void vSendHTTPRequestTask(void *pvParameters)
{
    // infinite loop like most tasks
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        ESP_LOGI(TAG, "In the HTTP send taskk");
        perform_get_request(URL);
        vTaskDelay(pdMS_TO_TICKS(5000));
        ESP_LOGI(TAG, "Leaving the HTTP send taskk...");
        xTaskNotifyGive(recv_task_handle);
        vTaskDelay(pdMS_TO_TICKS(5000));
        UBaseType_t watermark = uxTaskGetStackHighWaterMark(NULL);
        ESP_LOGI(TAG, "Send task high water mark %lu", watermark);
    }
}

void vReceiveHTTPRequestTask(void *pvParameters)
{
    // infinite loop like most tasks
    for (;;) {
        ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
        ESP_LOGI(TAG, "In the HTTP receive taskk");
        vTaskDelay(pdMS_TO_TICKS(5000));
        ESP_LOGI(TAG, "Leaving the HTTP receive taskk...");
        xTaskNotifyGive(send_task_handle);
        vTaskDelay(pdMS_TO_TICKS(5000));
        UBaseType_t watermark = uxTaskGetStackHighWaterMark(NULL);
        ESP_LOGI(TAG, "Receive task high water mark %lu", watermark);
    }
}

void app_main(void)
{
    esp_err_t esp_ret;
    
    esp_ret = connect_to_wifi();
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi connection failed. Aborting.");
        abort();
    }

    xTaskCreate(vSendHTTPRequestTask, "HTTP send task", 4096, NULL, 3, &send_task_handle);
    xTaskCreate(vReceiveHTTPRequestTask, "HTTP receive task", 2048, NULL, 3, &recv_task_handle);
    xTaskNotifyGive(send_task_handle);

    for (;;) {
        ESP_LOGI(TAG, "This is a test: %s, %s", TRAIN_TRACKER_API_KEY, BUS_TRACKER_API_KEY);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
