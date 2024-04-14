set(requires "")
idf_build_get_property(target IDF_TARGET)

if(${target} STREQUAL "linux")
    list(APPEND requires esp_stubs esp_event esp-tls esp_http_client protocol_examples_common nvs_flash esp_netif)
endif()

idf_component_register(SRCS "jianti.c" "main.c"
                    INCLUDE_DIRS "."
                    REQUIRES ${requires})
