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
#include "cJSON.h"

#include "secrets.h"

#define CONNECTION_TIMEOUT_SEC 10

static const char *TAG = "transit-tracker";

#define BUS_URL_ROOT "https://www.ctabustracker.com/bustime/api/v3/"
#define BUS_URL_KEY "key=" BUS_TRACKER_API_KEY
#define BUS_URL_FORMAT "&format=json"

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

/**
 * Task that makes GET request for bus route predictions, sending predicitons to queue in pvParameters
 * @param pvParameters: expects handle to a queue of char* buffers
 * 
 */
void vGetBusPredictionsTask(void *pvParameters)
{
    char *URL = BUS_URL_ROOT "getpredictions?" BUS_URL_KEY 
                "&rt=1,7,28&stpid=1583,4884,74&top=6"
                BUS_URL_FORMAT;
    QueueHandle_t buffer_queue = (QueueHandle_t)pvParameters;
    // infinite loop like most tasks
    for (;;) {
        ESP_LOGI(TAG, "In the bus predictions task");
        char *buf = malloc(MAX_HTTP_OUTPUT_BUFFER + 1);
        perform_get_request(URL, buf);
        xQueueSend(buffer_queue, &buf, pdMS_TO_TICKS(100));
        ESP_LOGI(TAG, "Leaving the bus predictions task...");
        free(buf);
        UBaseType_t watermark = uxTaskGetStackHighWaterMark(NULL);
        ESP_LOGI(TAG, "Bus prediction task high water mark %lu", watermark);
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

/**
 * Task that makes GET request for CTA's system time, printing response
 */
void vGetCTATimeTask(void *pvParameters)
{
    // infinite loop like most tasks
    char *URL = BUS_URL_ROOT "gettime?" BUS_URL_KEY BUS_URL_FORMAT;
    for (;;) {
        ESP_LOGI(TAG, "In the CTA time task");
        char *buf = malloc(MAX_HTTP_OUTPUT_BUFFER + 1);
        perform_get_request(URL, buf);
        printf("%s\n", buf);
        
        ESP_LOGI(TAG, "Leaving the CTA time task...");
        free(buf);
        UBaseType_t watermark = uxTaskGetStackHighWaterMark(NULL);
        ESP_LOGI(TAG, "CTA time prediction task high water mark %lu", watermark);
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

/**
 * Task that parses JSON data from queue passed into pvParameters. Rigt now, it only parses
 * prediciton requests.
 * @param pvParameters: expects handle to a queue of char* buffers
 */
void vParseAPIResponse(void *pvParameters)
{
    QueueHandle_t buffer_queue = (QueueHandle_t)pvParameters;
    BaseType_t ret;
    for (;;) {
        char *buf;
        ret = xQueueReceive(buffer_queue, &buf, portMAX_DELAY);
        if (ret == pdPASS) {
            ESP_LOGI(TAG, "Received a buffer, parsing...");
            cJSON *json;
            cJSON *response;
            cJSON *predictions;
            cJSON *prediction;
            json = cJSON_Parse(buf);
            if (json == NULL) {
                ESP_LOGE(TAG, "Failed to parse JSON response");
            }
            else {
                response = cJSON_GetObjectItem(json, "bustime-response");
                predictions = cJSON_GetObjectItem(response, "prd");
                cJSON_ArrayForEach(prediction, predictions) {
                    cJSON *rt = cJSON_GetObjectItemCaseSensitive(prediction, "rt");
                    cJSON *prdctdn = cJSON_GetObjectItemCaseSensitive(prediction, "prdctdn");
                    if ((rt == NULL) || (prdctdn == NULL)) {
                        ESP_LOGE(TAG, "Couldn't parse");
                    }
                    else {
                        printf("Prediction: %s, %s\n", rt->valuestring, prdctdn->valuestring);
                    }
            }
            }
            
            cJSON_Delete(json);
        }
        else {
            ESP_LOGI(TAG, "Couldn't receive buffer (queue empty)? %s", ret);
        }
        ESP_LOGI(TAG, "Leaving parsing task");
        ESP_LOGI(TAG, "Parse task high water mark: %lu", uxTaskGetStackHighWaterMark(NULL));
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
    QueueHandle_t buffer_queue = xQueueCreate(2, sizeof(char*));

    xTaskCreate(vParseAPIResponse, "Parse Response Task", 4096, (void *)buffer_queue, 4, NULL); // is that the right priority? idk consider
    xTaskCreate(vGetCTATimeTask, "CTA Time Task", 4096, NULL, 3, NULL);
    vTaskDelay(pdMS_TO_TICKS(25000));
    xTaskCreate(vGetBusPredictionsTask, "Bus Predictions Task", 4096, (void *)buffer_queue, 3, NULL);
    

    for (;;) {
        // Get current free heap
        ESP_LOGD(TAG, "Free heap: %lu bytes\n", esp_get_free_heap_size());

        // Get the lowest the heap has ever been since boot
        ESP_LOGD(TAG, "Minimum free heap ever: %lu bytes\n", esp_get_minimum_free_heap_size());
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
