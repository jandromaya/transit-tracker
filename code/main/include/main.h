#ifndef MAIN_H
#define MAIN_H

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
#include "hub75.h"
#include <lvgl.h>

/**
 * Request type, either a prediction request or a time request
 * predict_bus: a request for a prediction for a bus
 * predict_train: a request for a prediction for a train
 * cta_time: a request for the CTA system time
 */
typedef enum
{
    e_bus_prediction,
    e_train_prediction,
    e_cta_time,
} RequestType_t;

/**
 * Struct for API response buffers. Contains the type of the request (RequestType_t) and
 * a pointer to the buffer.
 */
typedef struct main
{
    RequestType_t response_type;
    char* buffer;
    const char* url;
} QueueData_t;
#endif
