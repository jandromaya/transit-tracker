idf_component_register(
    SRCS "https_client.c"
    INCLUDE_DIRS "include"
    REQUIRES esp_http_client esp-tls
)
