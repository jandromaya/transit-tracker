#include <stdio.h>
#include "esp_log.h"

#include "secrets.h"

static const char *TAG = "transit-tracker";

void app_main(void)
{
    ESP_LOGI(TAG, "This is a test: %s, %s", TRAIN_TRACKER_API_KEY, BUS_TRACKER_API_KEY);
    
}
