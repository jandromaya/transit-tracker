#include "main.h"

/**************************************************************************************
 * MACRO DEFINITIONS
 */
// defining function macros
#define STR_HELPER(x) #x
#define STR(x) STR_HELPER(x)

#define CEIL(x,y) (((x) + (y) - 1)/(y))

// building  bus URL for API calls
#define BUS_URL_ROOT        "https://www.ctabustracker.com/bustime/api/v3/"     // root of url
#define BUS_URL_KEY         "key=" BUS_TRACKER_API_KEY  // api key
#define BUS_URL_FORMAT      "&format=json"  // format of the response
#define BUS_URL_ROUTES      "&rt=" CONFIG_TRACKER_BUS_ROUTES    // routes to get info about
#define BUS_URL_STPID       "&stpid="  CONFIG_TRACKER_BUS_STPIDS  // stops to get info about
#define BUS_URL_TOP         "&top="   STR(CONFIG_TRACKER_BUS_TOP) // max number of predictions to receive
#define BUS_URL             BUS_URL_ROOT "getpredictions?" BUS_URL_KEY BUS_URL_ROUTES BUS_URL_STPID \
                            BUS_URL_TOP "&unixTime=true" BUS_URL_FORMAT

// building train URL for API calls
#define TRAIN_URL_ROOT      "http://lapi.transitchicago.com/api/1.0/"    // root of url
#define TRAIN_URL_KEY       "key=" TRAIN_TRACKER_API_KEY  // api key
#define TRAIN_URL_FORMAT    "&outputType=JSON" // format of the response
#define TRAIN_URL_MAPID     "&mapid=" CONFIG_TRACKER_TRAIN_MAPID // station to get info about
#define TRAIN_URL_MAX       "&max=" STR(CONFIG_TRACKER_TRAIN_MAX)    // max number of predictions to receive
#define TRAIN_URL           TRAIN_URL_ROOT "ttarrivals.aspx?" TRAIN_URL_KEY TRAIN_URL_MAPID \
                            TRAIN_URL_MAX TRAIN_URL_FORMAT

#define TIME_URL BUS_URL_ROOT "gettime?" BUS_URL_KEY "&unixTime=true" BUS_URL_FORMAT

// defining helpful compile-time constants
#define CONNECTION_TIMEOUT_SEC  10
#define ROWS_PER_SCREEN         3
#define BUS_COLS_PER_ROW        3
#define TRAIN_COLS_PER_ROW      2
#define NUM_BUS_SCREENS         CEIL(CONFIG_TRACKER_BUS_TOP, ROWS_PER_SCREEN)
#define NUM_TRAIN_SCREENS       CEIL(CONFIG_TRACKER_TRAIN_MAX, ROWS_PER_SCREEN)
#define BUS_PREDICTION_BIT      BIT0
#define TRAIN_PREDICTION_BIT    BIT1
#define GET_FAIL_BIT            BIT3

/**************************************************************************************
 * GLOBALS DEFINITIONS
 */
static const char *TAG = "transit-tracker";

// Global HUB75 driver instance (needed in flush callback)
static Hub75Driver *g_driver = nullptr;

// LVGL mutex for thread safety
static SemaphoreHandle_t lvgl_mutex = nullptr;

// Screen constants
Screen_t *bus_screen_array[NUM_BUS_SCREENS];
Screen_t *train_screen_array[NUM_TRAIN_SCREENS];

uint16_t curr_bus_predictions = 0;
uint16_t curr_train_predictions = 0;

EventGroupHandle_t prediction_event_group;

/**************************************************************************************
 * FUNCTION DEFINITIONS
 */
// Forward declaration
extern "C" void lvgl_ui(lv_obj_t *scr, lv_obj_t **label_array, int num_rows, int num_cols, bool train_ui);

// LVGL mutex lock/unlock helpers
static bool lvgl_lock(int timeout_ms) {
  const TickType_t timeout_ticks = (timeout_ms < 0) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
  return xSemaphoreTakeRecursive(lvgl_mutex, timeout_ticks) == pdTRUE;
}

static void lvgl_unlock() { xSemaphoreGiveRecursive(lvgl_mutex); }

// LVGL display flush callback
// Called by LVGL when a screen region needs updating
static void lvgl_flush_cb(lv_display_t *disp, const lv_area_t *area, uint8_t *px_map) {
  if (g_driver == nullptr) {
    ESP_LOGE(TAG, "FLUSH: HUB75 driver is NULL!");
    lv_display_flush_ready(disp);
    return;
  }

  // Get area bounds and draw pixels
  const uint16_t x = area->x1;
  const uint16_t y = area->y1;
  const uint16_t w = area->x2 - area->x1 + 1;
  const uint16_t h = area->y2 - area->y1 + 1;

  g_driver->draw_pixels(x, y, w, h, px_map, Hub75PixelFormat::RGB565);

#ifdef CONFIG_HUB75_DOUBLE_BUFFER
  g_driver->flip_buffer();
#endif

  lv_display_flush_ready(disp);
}

// LVGL timer task - calls lv_timer_handler() periodically
static void lvgl_timer_task(void *arg) {
  ESP_LOGI(TAG, "LVGL timer task started");

  TickType_t last_wake_time = xTaskGetTickCount();

  while (1) {
    // Lock LVGL mutex
    if (lvgl_lock(10)) {
      // Calculate elapsed time and update LVGL tick (required for animations)
      TickType_t current_time = xTaskGetTickCount();
      uint32_t elapsed_ms = pdTICKS_TO_MS(current_time - last_wake_time);
      last_wake_time = current_time;
      lv_tick_inc(elapsed_ms);

      // Handle LVGL timers and tasks (triggers redraws and animations)
      uint32_t sleep_ms = lv_timer_handler();
      lvgl_unlock();

      // Ensure reasonable sleep bounds for smooth animations
      if (sleep_ms > 100) {
        sleep_ms = 100;  // Max 100ms for responsive animations
      } else if (sleep_ms < 1) {
        sleep_ms = 1;  // Min 1ms to prevent busy-wait
      }

      vTaskDelay(pdMS_TO_TICKS(sleep_ms));
    } else {
      ESP_LOGW(TAG, "Could not get LVGL lock");
      vTaskDelay(pdMS_TO_TICKS(50));
    }
  }
}


/**
 * Function that performs all the Wi-Fi set up (starting NVS, initializing TCP/IP stack,
 * creating default event loop, initializing the Wi-Fi driver. Blocks while waiting for
 * a Wi-Fi connection.)
 * @param none
 * @return ESP_OK if connection is successful, ESP_FAIL otherwise (and prints error code)
 */
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
 * Updates screen labels with properly formatted train info. Predictions are in minutes,
 * and it prints the current station name in the color of the route, the destination name, and the prediction
 * (or prints DUE/DLY if train is due or delayed instead of a prediction).
 * @param destNm cJSON* corresponding to the destNm output of API
 * @param isApp cJSON* corresponding to the isApp output of API
 * @param isDly cJSON* corresponding to the isDly output of API
 * @param prdT cJSON* corresponding to the prdT output of API
 * @param arrT cJSON* corresponding to the arrT output of API
 * @param rt CSON* corresponding to the rt output of API
 * @param staNm cJSON* corresopnding to the staNm output of API
 * @param prediction_count size_t corresponding to the current total number of predictions
 * that have been processed (used to update correct label, 0 = first prediction, 1 = second, etc)
 */
esp_err_t print_train_info(cJSON *destNm, cJSON *isApp, cJSON *isDly, cJSON *prdT,
                      cJSON *arrT, cJSON *rt, cJSON *staNm, size_t prediction_count)
{
    // format train API response
    char time_to_arrival[16];
    if (strcmp(isApp->valuestring, "0") != 0) {
        // train is approaching
        snprintf(time_to_arrival, sizeof(time_to_arrival), "DUE");
    }
    else if (strcmp(isDly->valuestring, "0") != 0) {
        // train is delayed
        snprintf(time_to_arrival, sizeof(time_to_arrival), "DLY");
    }
    else {
        // train will arrive, predict when (done by converting API output to unix time)
        // and then comparing the unix time vaues with difftime
        struct tm tm_arrT;
        
        if (strptime(arrT->valuestring, "%Y-%m-%dT%T", &tm_arrT) == NULL) {
            ESP_LOGE(TAG, "Couldn't change arrT to tm struct");
            return ESP_FAIL;
        }
        struct tm tm_prdT;
        
        if (strptime(prdT->valuestring, "%Y-%m-%dT%T", &tm_prdT) == NULL) {
            ESP_LOGE(TAG, "Couldn't change prdT to tm struct");
            return ESP_FAIL;
        }

        time_t unix_arrT;
        time_t unix_prdT;
        unix_arrT = mktime(&tm_arrT);
        unix_prdT = mktime(&tm_prdT);
        
        if ((unix_arrT == -1) || (unix_prdT == -1)) {
            ESP_LOGE(TAG, "Couldn't change arrT or prdT to unix time");
            return ESP_FAIL;
        }
        int diff = difftime(unix_arrT, unix_prdT)/60;
        
        if (diff < 0) {
            ESP_LOGE(TAG, "Error, train prediction is negative (%.2f)", diff);
            return ESP_FAIL;
        }
        snprintf(time_to_arrival, sizeof(time_to_arrival), "%d", diff);
    }

    // Setting color codes based on CTA standard
    char sta_and_dest[128];
    for (int i = 0; i < NUM_ROUTES; i++) {
        if (strcmp(rt->valuestring, route_colors[i].route) == 0) {
            snprintf(sta_and_dest, sizeof(sta_and_dest), "%s %s#: %s", route_colors[i].color_hex,
                     staNm->valuestring, destNm->valuestring);
        }
    }

    // print the formatted response to the labels
    size_t screen_idx = prediction_count / ROWS_PER_SCREEN;
    size_t label_idx = (prediction_count % ROWS_PER_SCREEN) * TRAIN_COLS_PER_ROW;

    Screen_t *curr_screen = train_screen_array[screen_idx];
    if (lvgl_lock(portMAX_DELAY)) {
        lv_label_set_text(curr_screen->label_array[label_idx], sta_and_dest);
        lv_label_set_text(curr_screen->label_array[label_idx+1], time_to_arrival);
        lvgl_unlock();
    }
    printf("Prediction: %s, %s - %s\n", rt->valuestring, destNm->valuestring, time_to_arrival);
    return ESP_OK;
}

/**
 * Task that parses JSON data from queue passed into pvParameters. It will parse bus and train
 * predictions and print them on the correct screen labels. It will also parse time requests 
 * and print those to the console.
 * @param pvParameters: expects handle to a queue of char* buffers
 */
void vParseAPIResponseTask(void *pvParameters)
{
    QueueHandle_t response_queue = (QueueHandle_t)pvParameters;
    BaseType_t ret;
    esp_err_t esp_ret;
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
                uint32_t prediction_count = 0;
                int screen_idx = 0;
                cJSON_ArrayForEach(prediction, predictions) {
                    cJSON *rt = cJSON_GetObjectItemCaseSensitive(prediction, "rtdd");
                    cJSON *prdctdn = cJSON_GetObjectItemCaseSensitive(prediction, "prdctdn");
                    cJSON *stpnm = cJSON_GetObjectItemCaseSensitive(prediction, "stpnm");
                    cJSON *rtdir = cJSON_GetObjectItemCaseSensitive(prediction, "rtdir");
                    if ((rt == NULL) || (prdctdn == NULL) || (stpnm == NULL) || (rtdir == NULL)) {
                        ESP_LOGE(TAG, "Couldn't parse bus prediction");
                    }
                    else {
                        // current screen to update
                        Screen_t *curr_screen = bus_screen_array[screen_idx];
                        // label_idx basically just says what row we are currently updating 
                        size_t label_idx = (prediction_count % ROWS_PER_SCREEN) * 3;

                        // combining direction and stop data for the middle label
                        char dir_and_stop[128];
                        if (strcmp(rtdir->valuestring, "Northbound") == 0) {
                            snprintf(dir_and_stop, sizeof(dir_and_stop), "N: %s", stpnm->valuestring);
                        } else if (strcmp(rtdir->valuestring, "Southbound") == 0) {
                            snprintf(dir_and_stop, sizeof(dir_and_stop), "S: %s", stpnm->valuestring);
                        } else if (strcmp(rtdir->valuestring, "Eastbound") == 0) {
                            snprintf(dir_and_stop, sizeof(dir_and_stop), "E: %s", stpnm->valuestring);
                        } else {
                            snprintf(dir_and_stop, sizeof(dir_and_stop), "W: %s", stpnm->valuestring);
                        }

                        // setting the text for the labels on the screen
                        if (label_idx + 2 < ROWS_PER_SCREEN * BUS_COLS_PER_ROW){
                            if (lvgl_lock(portMAX_DELAY)) {
                                lv_label_set_text(curr_screen->label_array[label_idx], rt->valuestring);
                                lv_label_set_text(curr_screen->label_array[label_idx+1], dir_and_stop);
                                lv_label_set_text(curr_screen->label_array[label_idx+2], prdctdn->valuestring);
                                lvgl_unlock();
                            } else {
                                ESP_LOGW(TAG, "Skipped label update bc lock failed");
                            }
                        } 
                        printf("Prediction: %s - %s, %s\n", rt->valuestring, stpnm->valuestring, prdctdn->valuestring);
                    }

                    prediction_count++;
                    if (prediction_count % ROWS_PER_SCREEN == 0)
                        screen_idx++;
                }

                curr_bus_predictions = prediction_count;
                xEventGroupSetBits(prediction_event_group, BUS_PREDICTION_BIT);
            }
            else if (queue_data.response_type == e_train_prediction) {
                // getting train prediction from JSON reponse
                cJSON *response;
                cJSON *predictions;
                cJSON *prediction;
                response = cJSON_GetObjectItem(json, "ctatt");
                predictions = cJSON_GetObjectItem(response, "eta");

                size_t prediction_count = 0;
                cJSON_ArrayForEach(prediction, predictions) {
                    cJSON *destNm = cJSON_GetObjectItem(prediction, "destNm");
                    cJSON *isApp = cJSON_GetObjectItem(prediction, "isApp");
                    cJSON *isDly = cJSON_GetObjectItem(prediction, "isDly");
                    cJSON *prdT = cJSON_GetObjectItem(prediction, "prdT");
                    cJSON *arrT = cJSON_GetObjectItem(prediction, "arrT");
                    cJSON *rt = cJSON_GetObjectItem(prediction, "rt");
                    cJSON *staNm = cJSON_GetObjectItem(prediction, "staNm");

                    if ((destNm == NULL) || (isApp == NULL) || (isDly == NULL) || (staNm == NULL) ||
                        (prdT == NULL) || (arrT == NULL) || (destNm == NULL) || (rt == NULL)) {

                        ESP_LOGE(TAG, "Couldn't parse train prediction");
                    }
                    else {
                        esp_ret = print_train_info(destNm, isApp, isDly, prdT, arrT, rt, staNm, prediction_count);
                        if (esp_ret != ESP_OK) {    // if it fails, this prediction won't be printed, so decrease prediction count
                            prediction_count--;
                        }
                    }
                    
                    prediction_count++;
                }
                curr_train_predictions = prediction_count;
                xEventGroupSetBits(prediction_event_group, TRAIN_PREDICTION_BIT);
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
            xEventGroupSetBits(prediction_event_group, GET_FAIL_BIT);
        }

        // perform get request
        const char *URL = schedule.url;
        RequestType_t response_type = schedule.response_type;

        char *response_buf = (char *)malloc(MAX_HTTP_OUTPUT_BUFFER + 1);
        esp_ret = perform_get_request(URL, response_buf);
        if (esp_ret != ESP_OK) {
            ESP_LOGE(TAG, "Error (%d): Couldn't perform GET request", esp_ret);
            free(response_buf);
            xEventGroupSetBits(prediction_event_group, GET_FAIL_BIT);
            continue;
        }

        // send request results to queue for processing
        QueueData_t response;
        response.buffer = response_buf;
        response.response_type = response_type;
        if (xQueueSend(response_queue, &response, pdMS_TO_TICKS(100)) != pdPASS) {
            // queue was full, free the response buffer
            ESP_LOGE(TAG, "Processor queue full, dropping response");
            free(response_buf);
            xEventGroupSetBits(prediction_event_group, GET_FAIL_BIT);
            continue;
        }

        // leave task
        ESP_LOGI(TAG, "Leaving the GET request task...");
        UBaseType_t watermark = uxTaskGetStackHighWaterMark(NULL);
        ESP_LOGI(TAG, "GET request task high water mark %lu", watermark);
    }
}

/**
 * Function that creates screens and populates screen_array with the newly-created screens.
 * @param screen_array screen array to populate
 * @param num_screens number of screens to create
 * @param disp pointer to the display that the screens should be on
 */
void create_screens(Screen_t **screen_array, size_t num_screens, lv_disp_t *disp, bool train_ui) {
    int cols;
    if (train_ui) 
        cols = TRAIN_COLS_PER_ROW;
    else
        cols = BUS_COLS_PER_ROW;

    if (lvgl_lock(0)) {
        for (size_t i = 0; i < num_screens; i++) {     // initialize each screen
            lv_obj_t *scr = lv_obj_create(NULL);
            lv_obj_t **label_array = (lv_obj_t **)malloc(sizeof(lv_obj_t *) * ROWS_PER_SCREEN * cols);
            Screen_t *screen = (Screen_t *)malloc(sizeof(Screen_t));
            screen->screen = scr;
            screen->label_array = label_array;
            screen_array[i] = screen;
            lvgl_ui(screen->screen, screen->label_array, ROWS_PER_SCREEN, cols, train_ui);
            lv_obj_invalidate(lv_screen_active());
            lv_refr_now(disp);
        } 
        lvgl_unlock();
    } else {
        ESP_LOGE(TAG, "Could not lock LVGL for UI creation!");
    }
}

void delete_screens(){
    // TODO
}

/**
 * Helper function to begin the Hub75 driver. It loads the configuration from menuconfig,
 * creates and initializes the driver based on the configuration, and performs a 1-second hardware test.
 * @returns ESP_OK if success, ESP_FAIL otherwise.
 */
esp_err_t hub75_begin(Hub75Driver &driver, Hub75Config &config) 
{
    if (!driver.begin()) {
        ESP_LOGE(TAG, "Failed to initialize HUB75 driver!");
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "HUB75 driver initialized");
    ESP_LOGI(TAG, "  Display: %ux%u pixels", driver.get_width(), driver.get_height());
    ESP_LOGI(TAG, "  Clock: %lu Hz, Bit depth: %u, Refresh: %u Hz", (unsigned long) config.output_clock_speed,
            HUB75_BIT_DEPTH, config.min_refresh_rate);
    ESP_LOGI(TAG, "  Pins - R1=%d G1=%d B1=%d R2=%d G2=%d B2=%d", config.pins.r1, config.pins.g1, config.pins.b1,
            config.pins.r2, config.pins.g2, config.pins.b2);
    ESP_LOGI(TAG, "  Pins - A=%d B=%d C=%d D=%d E=%d CLK=%d LAT=%d OE=%d", config.pins.a, config.pins.b, config.pins.c,
            config.pins.d, config.pins.e, config.pins.clk, config.pins.lat, config.pins.oe);

    // Quick hardware test - RGB color bars
    driver.clear();
    ESP_LOGI(TAG, "Drawing RGB test pattern...");
    const uint16_t bar_width = driver.get_width() / 3;
    for (uint16_t y = 0; y < driver.get_height(); y++) {
        for (uint16_t x = 0; x < driver.get_width(); x++) {
            if (x < bar_width) {
                driver.set_pixel(x, y, 255, 0, 0);  // Red
            } else if (x < bar_width * 2) {
                driver.set_pixel(x, y, 0, 0, 255);  // Green
            } else {
                driver.set_pixel(x, y, 0, 255, 0);  // Blue
            }
        }
    }
    vTaskDelay(pdMS_TO_TICKS(1000));
    driver.clear();

    return ESP_OK;
}

esp_err_t lvgl_init(Hub75Driver &driver, lv_display_t **disp)
{
    // Initialize LVGL
    ESP_LOGI(TAG, "Initializing LVGL...");

    lvgl_mutex = xSemaphoreCreateRecursiveMutex();
    if (lvgl_mutex == nullptr) {
        ESP_LOGE(TAG, "Failed to create LVGL mutex!");
        return ESP_FAIL;
    }

    lv_init();

    *disp = lv_display_create(driver.get_width(), driver.get_height());
    if (disp == nullptr) {
        ESP_LOGE(TAG, "Failed to create LVGL display!");
        return ESP_FAIL;
    }

    // Set color format and create draw buffer (LVGL manages memory)
    lv_display_set_color_format(*disp, LV_COLOR_FORMAT_RGB565);
    lv_draw_buf_t *draw_buf = lv_draw_buf_create(driver.get_width(), driver.get_height(), LV_COLOR_FORMAT_RGB565, 0);
    if (draw_buf == nullptr) {
        ESP_LOGE(TAG, "Failed to create LVGL draw buffer!");
        return ESP_FAIL;
    }
    lv_display_set_draw_buffers(*disp, draw_buf, nullptr);
    lv_display_set_render_mode(*disp, LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(*disp, lvgl_flush_cb);

    ESP_LOGI(TAG, "LVGL initialized");
    return ESP_OK;
}

esp_err_t run_prediction_cycle(RequestType_t prediction_type, QueueHandle_t &schedule_queue,
                               QueueData_t &msg, uint16_t *curr_predictions, uint32_t time_per_screen) 
{
    EventBits_t event_ret;
    size_t screens_needed;
    uint16_t remainder_rows;
    int16_t cols_per_row;
    Screen_t **screen_array;

    // send message to schedule queue to awaken GET request task
    xQueueSend(schedule_queue, &msg, pdMS_TO_TICKS(100));

    if (prediction_type == e_bus_prediction) {
        cols_per_row = BUS_COLS_PER_ROW;
        screen_array = bus_screen_array;
        event_ret = xEventGroupWaitBits(prediction_event_group,
                            BUS_PREDICTION_BIT | GET_FAIL_BIT,
                            pdTRUE,
                            pdFALSE,
                            pdMS_TO_TICKS(CONNECTION_TIMEOUT_SEC * 1000));
    } else {
        cols_per_row = TRAIN_COLS_PER_ROW;
        screen_array = train_screen_array;
        event_ret = xEventGroupWaitBits(prediction_event_group,
                            TRAIN_PREDICTION_BIT | GET_FAIL_BIT,
                            pdTRUE,
                            pdFALSE,
                            pdMS_TO_TICKS(CONNECTION_TIMEOUT_SEC * 1000));
    }

    if ((event_ret & GET_FAIL_BIT) != 0) {
        ESP_LOGE(TAG, "GET request failed. Dropping request.");
        return ESP_FAIL;
    }

    // Figure out how many screens are needed to display all the data
    screens_needed = CEIL(*curr_predictions, ROWS_PER_SCREEN);
    ESP_LOGI(TAG, "received %d predictions; need %d screens", *curr_predictions, screens_needed);

    // clearing old rows in screen
    remainder_rows = *curr_predictions % ROWS_PER_SCREEN;
    if (screens_needed > 0 && remainder_rows > 0) {
        Screen_t *last_screen = screen_array[screens_needed - 1];
        if (lvgl_lock(portMAX_DELAY)) {
            for (int i = remainder_rows * cols_per_row; i < ROWS_PER_SCREEN * cols_per_row; i++) {
                lv_label_set_text(last_screen->label_array[i], "");
            }
            lvgl_unlock();
        }
    }
    
    // switch active display periodically to show all screens needed in 15 seconds
    for (int i = 0; i < screens_needed; i++) {
        if (lvgl_lock(portMAX_DELAY)) {
            lv_screen_load(screen_array[i]->screen);
            lvgl_unlock();
        }
        
        vTaskDelay(pdMS_TO_TICKS(time_per_screen));
    }

    return ESP_OK;
}

extern "C" void app_main(void)
{
    esp_err_t esp_ret;
    BaseType_t rtos_ret;
    lv_display_t *disp;

    prediction_event_group = xEventGroupCreate();
    Hub75Config config = getMenuConfigSettings();   // Load configuration from menuconfig
    static Hub75Driver driver(config);  // Create and initialize HUB75 driver
    g_driver = &driver;


    ESP_LOGI(TAG, "Transit tracker starting...");


    ESP_LOGI(TAG, "Configuration:");
    ESP_LOGI(TAG, "  Panel: %dx%d pixels", config.panel_width, config.panel_height);
    ESP_LOGI(TAG, "  Double buffering: %s", config.double_buffer ? "ENABLED" : "DISABLED");

    esp_ret = hub75_begin(driver, config);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "HUB75 driver initialization failed. Aborting.");
        abort();
    }

    esp_ret = lvgl_init(driver, &disp);
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "LVGL initialization failed. Aborting.");
        abort();
    }

    // Creating screens
    create_screens(bus_screen_array, NUM_BUS_SCREENS, disp, false);
    create_screens(train_screen_array, NUM_TRAIN_SCREENS, disp, true);
    
    esp_ret = connect_to_wifi();
    if (esp_ret != ESP_OK) {
        ESP_LOGE(TAG, "Wi-Fi connection failed. Aborting.");
        abort();
    }

    QueueHandle_t schedule_queue = xQueueCreate(3, sizeof(QueueData_t));

    rtos_ret = xTaskCreate(vPerformGetRequestTask, "Perform GET Request Task", 4096, (void *)schedule_queue, 3, NULL);
    if (rtos_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create GET request task!");
        abort();
    }

    rtos_ret = xTaskCreate(lvgl_timer_task, "lvgl timer task", 4096, NULL, 4, NULL);
    if (rtos_ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create LVGL timer task!");
        abort();
    }

    ESP_LOGI(TAG, "LVGL running, demo UI displayed");

    // Setting messages for GET request task
    QueueData_t bus_msg;
    bus_msg.url = BUS_URL;
    bus_msg.response_type = e_bus_prediction;

    QueueData_t train_msg;
    train_msg.url = TRAIN_URL;
    train_msg.response_type = e_train_prediction;
    
    lv_screen_load(bus_screen_array[0]->screen);

    for (;;) {

        esp_ret = run_prediction_cycle(e_bus_prediction, schedule_queue, bus_msg, &curr_bus_predictions, 8000);
        if (esp_ret != ESP_OK) {
            lv_obj_t **label = bus_screen_array[0]->label_array;

            // Setting error text and clearing rest of screen
            lv_label_set_text(label[0], "Bus prediction failed...");
            for (int i = 1; i < BUS_COLS_PER_ROW * ROWS_PER_SCREEN; i++) {
                lv_label_set_text(label[i], "");
            }

            lv_screen_load(bus_screen_array[0]->screen);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }

        esp_ret = run_prediction_cycle(e_train_prediction, schedule_queue, train_msg, &curr_train_predictions, 8000);
        if (esp_ret != ESP_OK) {
            lv_obj_t **label = train_screen_array[0]->label_array;
            
            // Setting error text and clearing rest of screen
            lv_label_set_text(label[0], "Train prediction failed...");
            for (int i = 1; i < TRAIN_COLS_PER_ROW * ROWS_PER_SCREEN; i++) {
                lv_label_set_text(label[i], "");
            }

            lv_screen_load(train_screen_array[0]->screen);
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }
}
