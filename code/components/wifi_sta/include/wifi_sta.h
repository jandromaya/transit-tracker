#ifndef WIFI_STA_H
#define WIFI_STA_H

#include "esp_err.h"

/**
 * @brief Event group bits for Wifi events
 */
#define WIFI_STA_CONNECTED_BIT  BIT0
#define WIFI_STA_IPV4_OBTAINED_BIT  BIT1
#define WIFI_STA_IPV6_OBTAINED_BIT  BIT2

/**
 * @brief Initialize WiFi in station mode
 * 
 * Set up the WiFi interface and connect to a WiFi network. You can use the
 * event group to wait for a conneciton and IP address assignment.
 * 
 * Important! You must call esp_netif_init() and esp_event_loop_create_default()
 * before calling this function.
 * 
 * @param[in] event_group Event group handle for WiFi and IP evnets. Pass NULL to use
 *                          the existing event group
 * 
 * @return 
 *  - ESP_OK in success
 *  - Other errors on failure. 
 */
esp_err_t wifi_sta_init(EventGroupHandle_t event_group);

/**
 * @brief Disable WiFi
 * 
 * @return
 *  - ESP_OK on success
 *  - Other errors on failure. See esp_err.h for error codes.
 */
esp_err_t wifi_sta_stop(void);

/**
 * @brief Attempt to reconnect WiFi
 * 
 * @return 
 *  - ESP_OK on success
 *  - Other errors on failure.
 */
esp_err_t wifi_sta_reconnect(void);

/**
 * @brief Wait for the WIFI_STA_CONNECTED_BIT to be set and then wait for the
 * WIFI_STA_IPV4_OBTAINED_BIT or the WIFI_STA_IPV6_OBTAINED_BIT to be set
 * 
 * @return
 *  - true if WIFI_STA_CONNECTED_BIT and either WIFI_STA_IPV4_OBTAINED_BIT or WIFI_STA_IPV6_OBTAINED_BIT are set
 *  - false if any of the bits not set after waiting for seconds specified by timeout_sec
 */
bool wait_for_wifi(EventGroupHandle_t wifi_event_group, int32_t timeout_sec);
#endif