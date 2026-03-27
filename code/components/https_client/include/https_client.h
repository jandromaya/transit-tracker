#pragma once

#include <sys/param.h>
#include "esp_log.h"
#include "esp_event.h"
#include "esp_tls.h"
#include "esp_crt_bundle.h"
#include "freertos/FreeRTOS.h"
#include "esp_http_client.h"

esp_err_t perform_get_request(char *request_url);