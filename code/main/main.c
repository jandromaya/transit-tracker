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
#include "main.h"

#define CONNECTION_TIMEOUT_SEC 10

static const char *TAG = "transit-tracker";

#define BUS_URL_ROOT    "https://www.ctabustracker.com/bustime/api/v3/"     // root of url
#define BUS_URL_KEY     "key=" BUS_TRACKER_API_KEY  // api key
#define BUS_URL_FORMAT  "&format=json"  // format of the response
#define BUS_URL_ROUTES  "&rt=4,7,28"    // routes to get info about
#define BUS_URL_STPID   "&stpid=1583,4884,74"   // stops to get info about
#define BUS_URL_TOP     "&top=6"    // max number of predictions to receive
#define BUS_URL         BUS_URL_ROOT "getpredictions?" BUS_URL_KEY BUS_URL_ROUTES BUS_URL_STPID \
                        BUS_URL_TOP "&unixTime=true" BUS_URL_FORMAT

#define TRAIN_URL_ROOT "http://lapi.transitchicago.com/api/1.0/"    // root of url
#define TRAIN_URL_KEY "key=" TRAIN_TRACKER_API_KEY  // api key
#define TRAIN_URL_FORMAT "&outputType=JSON" // format of the response
#define TRAIN_URL_MAPID     "&mapid=41490"  // station to get info about
#define TRAIN_URL_MAX       "&max=5"    // max number of predictions to receive
#define TRAIN_URL       TRAIN_URL_ROOT "ttarrivals.aspx?" TRAIN_URL_KEY TRAIN_URL_MAPID \
                        TRAIN_URL_MAX TRAIN_URL_FORMAT

#define TIME_URL BUS_URL_ROOT "gettime?" BUS_URL_KEY "&unixTime=true" BUS_URL_FORMAT

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

void print_train_info(cJSON *destNm, cJSON *isApp, cJSON *isDly, cJSON *prdT, cJSON *arrT)
{
    char* time_to_arrival;
    if (strcmp(isApp->valuestring, "0") != 0) {
        // train is approaching
        time_to_arrival = "DUE";
    }
    else if (strcmp(isDly->valuestring, "0") != 0) {
        // train is delayed
        time_to_arrival = "DELAYED";
    }
    else {
        // train is in transit normally
        time_to_arrival = arrT->valuestring;
    }

    printf("Prediction: %s - %s\n", destNm->valuestring, time_to_arrival);
}

/**
 * Task that parses JSON data from queue passed into pvParameters. Rigt now, it only parses
 * prediciton requests.
 * @param pvParameters: expects handle to a queue of char* buffers
 */
void vParseAPIResponseTask(void *pvParameters)
{
    QueueHandle_t response_queue = (QueueHandle_t)pvParameters;
    BaseType_t ret;
    for (;;) {
        // get data from response queue
        QueueData_t queue_data;
        ret = xQueueReceive(response_queue, &queue_data, portMAX_DELAY);

        // if data was received from the queue successfully, process it
        if (ret == pdPASS) {
            ESP_LOGI(TAG, "Received a buffer, parsing...");

            // setting up JSON struct
            cJSON *json;
            json = cJSON_Parse(queue_data.buffer);
            if (json == NULL) {
                ESP_LOGE(TAG, "Failed to parse JSON response");
            }

            if (queue_data.response_type == e_bus_prediction){
                // getting predictions from JSON response
                cJSON *response;    // top level item
                cJSON *predictions; // predictions come in an array item with name "prd"
                cJSON *prediction;  // cJSON level for each individual prediction
                response = cJSON_GetObjectItem(json, "bustime-response");
                predictions = cJSON_GetObjectItem(response, "prd");

                // Since predictions come in an array, must iterate through each prediction
                // in predictions
                cJSON_ArrayForEach(prediction, predictions) {
                    cJSON *rt = cJSON_GetObjectItemCaseSensitive(prediction, "rtdd");
                    cJSON *prdctdn = cJSON_GetObjectItemCaseSensitive(prediction, "prdctdn");
                    cJSON *stpnm = cJSON_GetObjectItemCaseSensitive(prediction, "stpnm");
                    if ((rt == NULL) || (prdctdn == NULL) || (stpnm == NULL)) {
                        ESP_LOGE(TAG, "Couldn't parse bus prediction");
                    }
                    else {
                        printf("Prediction: %s - %s, %s\n", rt->valuestring, stpnm->valuestring, prdctdn->valuestring);
                    }
                }
            }
            else if (queue_data.response_type == e_cta_time) {
                // getting CTA system time from JSON response
                cJSON *response;    // top level response item
                cJSON *tm;          // cJSON object to hold time received

                response = cJSON_GetObjectItem(json, "bustime-response");
                tm = cJSON_GetObjectItem(response, "tm");
                if (tm == NULL) {
                    ESP_LOGE(TAG, "Couldn't parse time");
                }
                else {
                    printf("Time: %s\n", tm->valuestring);
                }
            }
            else if (queue_data.response_type == e_train_prediction) {
                // getting train prediction from JSON reponse
                cJSON *response;
                cJSON *predictions;
                cJSON *prediction;
                response = cJSON_GetObjectItem(json, "ctatt");
                predictions = cJSON_GetObjectItem(response, "eta");

                cJSON_ArrayForEach(prediction, predictions) {
                    cJSON *destNm = cJSON_GetObjectItem(prediction, "destNm");
                    cJSON *isApp = cJSON_GetObjectItem(prediction, "isApp");
                    cJSON *isDly = cJSON_GetObjectItem(prediction, "isDly");
                    cJSON *prdT = cJSON_GetObjectItem(prediction, "prdT");
                    cJSON *arrT = cJSON_GetObjectItem(prediction, "arrT");

                    if ((destNm == NULL) || (isApp == NULL) || (isDly == NULL) ||
                        (prdT == NULL) || (arrT == NULL) || (destNm == NULL)) {

                        ESP_LOGE(TAG, "Couldn't parse train prediction");
                    }
                    else {
                        print_train_info(destNm, isApp, isDly, prdT, arrT);
                    }
                }
            }
            
            cJSON_Delete(json);
            free(queue_data.buffer);
        }
        // if data was not received successfully, output an error
        else {
            ESP_LOGI(TAG, "Couldn't receive buffer (queue empty)? %s", ret);
        }
        ESP_LOGI(TAG, "Leaving parsing task");
        ESP_LOGI(TAG, "Parse task high water mark: %lu", uxTaskGetStackHighWaterMark(NULL));
    }    
}

/**
 * Task that performs get requests, sending the server response to the response queue for
 * parsing
 * @param pvParameters: Queue handle of the response queue
 */
void vPerformGetRequestTask(void *pvParameters)
{
    QueueHandle_t schedule_queue = (QueueHandle_t)pvParameters;
    QueueHandle_t response_queue = xQueueCreate(3, sizeof(QueueData_t));
    BaseType_t ret;
    esp_err_t esp_ret;

    xTaskCreate(vParseAPIResponseTask, "Parse API Response Task", 4096, (void *)response_queue, 4, NULL);

    for (;;) {
        // wait for a signal to start a get request
        QueueData_t schedule;
        ret = xQueueReceive(schedule_queue, &schedule, portMAX_DELAY);
        if (ret != pdPASS) {
            ESP_LOGE(TAG, "Couldn't receive data from queue (is it empty?)");
        }

        // perform get request
        char *URL = schedule.buffer;
        RequestType_t response_type = schedule.response_type;

        char *response_buf = malloc(MAX_HTTP_OUTPUT_BUFFER + 1);
        esp_ret = perform_get_request(URL, response_buf);
        if (esp_ret != ESP_OK) {
            ESP_LOGE(TAG, "Error (%d): Couldn't perform GET request", esp_ret);
            free(response_buf);
        }

        // send request results to queue for processing
        QueueData_t response;
        response.buffer = response_buf;
        response.response_type = response_type;
        if (xQueueSend(response_queue, &response, pdMS_TO_TICKS(100)) != pdPASS) {
            // queue was full, free the response buffer
            ESP_LOGE(TAG, "Processor queue full, dropping response");
            free(response_buf);
        }

        // leave task
        ESP_LOGI(TAG, "Leaving the GET request task...");
        UBaseType_t watermark = uxTaskGetStackHighWaterMark(NULL);
        ESP_LOGI(TAG, "GET request task high water mark %lu", watermark);
        vTaskDelay(pdMS_TO_TICKS(10000));
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

    QueueHandle_t schedule_queue = xQueueCreate(3, sizeof(QueueData_t));

    xTaskCreate(vPerformGetRequestTask, "Perform GET Request Task", 4096, (void *)schedule_queue, 3, NULL);

    // this can be improved to be less repetitive
    QueueData_t bus_msg;
    bus_msg.buffer = BUS_URL;
    bus_msg.response_type = e_bus_prediction;

    QueueData_t train_msg;
    train_msg.buffer = TRAIN_URL;
    train_msg.response_type = e_train_prediction;

    QueueData_t time_msg;
    time_msg.buffer = TIME_URL;
    time_msg.response_type = e_cta_time;

    for (;;) {
        // Buses
        xQueueSend(schedule_queue, &bus_msg, pdMS_TO_TICKS(100));
        vTaskDelay(pdMS_TO_TICKS(12000));
        // Trains
        xQueueSend(schedule_queue, &train_msg, pdMS_TO_TICKS(100));
        vTaskDelay(pdMS_TO_TICKS(12000));
        // Time
        xQueueSend(schedule_queue, &time_msg, pdMS_TO_TICKS(100));
        vTaskDelay(pdMS_TO_TICKS(6000));
    }
}
