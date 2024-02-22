#include <sys/param.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_mac.h"

#include "nvs_flash.h"
#include "esp_wifi.h"
#include "esp_netif.h"
#include "lwip/inet.h"

#include "esp_http_server.h"
#include "dns_server.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "led_strip.h"
#include "esp_task_wdt.h"
#include "lwip/opt.h"

#include "lwip/lwip_napt.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "lwip/inet.h"


#define EXAMPLE_ESP_WIFI_SSID "esp32_will"
#define EXAMPLE_ESP_WIFI_PASS "123456789"
#define EXAMPLE_MAX_STA_CONN 4

// GPIO assignment
#define LED_STRIP_BLINK_GPIO  4
// Numbers of the LED in the strip
#define LED_STRIP_LED_NUMBERS 1
// 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define LED_STRIP_RMT_RES_HZ  (10 * 1000 * 1000)

extern const char root_start[] asm("_binary_root_html_start");
extern const char root_end[] asm("_binary_root_html_end");

static const char *TAG = "HK";

static int blink_led = 0; // 控制LED闪烁的变量

int brightness = 0;

uint8_t red = 255;
uint8_t green = 255;
uint8_t blue = 255;

led_strip_handle_t led_strip = NULL;

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t *event = (wifi_event_ap_staconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t *event = (wifi_event_ap_stadisconnected_t *)event_data;
        ESP_LOGI(TAG, "station " MACSTR " leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if(event_id == IP_EVENT_STA_GOT_IP){
    	ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
    	ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
    } else if(event_id == WIFI_EVENT_STA_START) {
    	esp_err_t status = esp_wifi_connect();
    	ESP_LOGI(TAG, "esp_wifi_connect %d", status);
    }
}


static esp_netif_t* _esp_netif_sta = NULL;
static esp_netif_t* _esp_netif_ap = NULL;

static void wifi_init_softap(void)
{
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    _esp_netif_ap  = esp_netif_create_default_wifi_ap();
    _esp_netif_sta = esp_netif_create_default_wifi_sta();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_got_ip;

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
														IP_EVENT_STA_GOT_IP,
														&wifi_event_handler,
														NULL,
														&instance_got_ip));

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .ssid_len = strlen(EXAMPLE_ESP_WIFI_SSID),
            .password = EXAMPLE_ESP_WIFI_PASS,
            .max_connection = EXAMPLE_MAX_STA_CONN,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };

    wifi_config_t wifi_sta_config = {
        .sta = {
            .ssid = "PDCN",
            .password = "dz20230605.",
            .threshold.authmode = WIFI_AUTH_OPEN,
	        .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
        },
    };

    if (strlen(EXAMPLE_ESP_WIFI_PASS) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_sta_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    esp_netif_ip_info_t ip_info;
    esp_netif_get_ip_info(esp_netif_get_handle_from_ifkey("WIFI_AP_DEF"), &ip_info);

    char ip_addr[16];
    inet_ntoa_r(ip_info.ip.addr, ip_addr, 16);
    ESP_LOGI(TAG, "Set up softAP with IP: %s", ip_addr);

    ESP_LOGI(TAG, "wifi_init_softap finished. SSID:'%s' password:'%s'",
             EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
}

// HTTP GET Handler
static esp_err_t root_get_handler(httpd_req_t *req)
{
    const uint32_t root_len = root_end - root_start;

    ESP_LOGI(TAG, "Serve root");
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, root_start, root_len);

    return ESP_OK;
}

static const httpd_uri_t root = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = root_get_handler
};

// 控制LED的处理函数
esp_err_t led_control_handler(httpd_req_t *req) {
    char*  buf;
    size_t buf_len;

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if(httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            char param[32];
            if (httpd_query_key_value(buf, "state", param, sizeof(param)) == ESP_OK) {
                if (strcmp(param, "on") == 0) {
                    blink_led = 1;
                    ESP_LOGI(TAG, "on");
                    // gpio_set_level(LED_GPIO, 1); // 打开LED
                } else if (strcmp(param, "off") == 0) {
                    blink_led = 0;
                    ESP_LOGI(TAG, "off");
                    // gpio_set_level(LED_GPIO, 0); // 关闭LED
                }
            }
        }
        free(buf);
    }
    httpd_resp_send(req, NULL, 0); // 发送空响应
    return ESP_OK;
}

// 获取LED状态的处理函数
esp_err_t get_led_status_handler(httpd_req_t *req) {

    httpd_resp_send(req, blink_led?"LED is ON":"LED is OFF", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t set_led_brightness_handler(httpd_req_t *req) {
    char buf[100];
    int ret;

    // 从请求中读取亮度值
    ret = httpd_req_recv(req, buf, sizeof(buf));
    if (ret <= 0) { // 错误或连接关闭
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }
    buf[ret] = '\0'; // 确保字符串终止

    // 解析亮度值
    sscanf(buf, "brightness=%d", &brightness);
    ESP_LOGI(TAG, "brightness=%d",brightness);

    // 发送响应
    httpd_resp_send(req, "Brightness updated", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

esp_err_t set_color_handler(httpd_req_t *req) {
    char* buf;
    size_t buf_len;

    buf_len = httpd_req_get_url_query_len(req) + 1;
    if (buf_len > 1) {
        buf = malloc(buf_len);
        if(!buf){
            httpd_resp_send_500(req);
            return ESP_FAIL;
        }
        if (httpd_req_get_url_query_str(req, buf, buf_len) == ESP_OK) {
            // 解析RGB值
            sscanf(buf, "r=%hhu&g=%hhu&b=%hhu", &red, &green, &blue);
            ESP_LOGI(TAG, "r=%hhu&g=%hhu&b=%hhu",red, green, blue);
        }
        free(buf);
    }
    httpd_resp_send(req, "Color set", HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

// URI结构体
httpd_uri_t led_control = {
    .uri       = "/control",
    .method    = HTTP_GET,
    .handler   = led_control_handler,
    .user_ctx  = NULL
};

httpd_uri_t uri_set_color = {
    .uri       = "/set-color",
    .method    = HTTP_GET,
    .handler   = set_color_handler,
    .user_ctx  = NULL
};

httpd_uri_t get_status = {
    .uri       = "/status",
    .method    = HTTP_GET,
    .handler   = get_led_status_handler,
    .user_ctx  = NULL
};

httpd_uri_t set_led_brightness = {
    .uri       = "/set-brightness",
    .method    = HTTP_POST,
    .handler   = set_led_brightness_handler,
    .user_ctx  = NULL
};



// HTTP Error (404) Handler - Redirects all requests to the root page
esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    // Set status
    httpd_resp_set_status(req, "302 Temporary Redirect");
    // Redirect to the "/" root directory
    httpd_resp_set_hdr(req, "Location", "/");
    // iOS requires content in the response to detect a captive portal, simply redirecting is not sufficient.
    httpd_resp_send(req, "Redirect to the captive portal", HTTPD_RESP_USE_STRLEN);

    ESP_LOGI(TAG, "Redirecting to root");
    return ESP_OK;
}

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.max_open_sockets = 13;
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &root);
        httpd_register_uri_handler(server, &led_control);
        httpd_register_uri_handler(server, &get_status);
        httpd_register_uri_handler(server, &set_led_brightness);
        httpd_register_uri_handler(server, &uri_set_color);
        httpd_register_err_handler(server, HTTPD_404_NOT_FOUND, http_404_error_handler);
    }
    return server;
}

led_strip_handle_t configure_led(void)
{
    // LED strip general initialization, according to your led board design
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_BLINK_GPIO,   // The GPIO that connected to the LED strip's data line
        .max_leds = LED_STRIP_LED_NUMBERS,        // The number of LEDs in the strip,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB, // Pixel format of your LED strip
        .led_model = LED_MODEL_WS2812,            // LED strip model
        .flags.invert_out = false,                // whether to invert the output signal
    };

    // LED strip backend configuration: RMT
    led_strip_rmt_config_t rmt_config = {
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
        .rmt_channel = 0,
#else
        .clk_src = RMT_CLK_SRC_DEFAULT,        // different clock source can lead to different power consumption
        .resolution_hz = LED_STRIP_RMT_RES_HZ, // RMT counter clock frequency
        .flags.with_dma = false,               // DMA feature is available on ESP target like ESP32-S3
#endif
    };

    // LED Strip object handle
    led_strip_handle_t led_strip;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    ESP_LOGI(TAG, "Created LED strip object with RMT backend");
    return led_strip;
}

// LED任务
void blink_task(void *pvParameter) {
    // gpio_pad_select_gpio(LED_GPIO);
    // gpio_set_direction(LED_GPIO, GPIO_MODE_OUTPUT);
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));
    while(1) {
        if (blink_led) {
                    uint8_t reds = (red * brightness) / 100;
                    uint8_t greens = (green * brightness) / 100;
                    uint8_t blues = (blue * brightness) / 100;

                    ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 0, reds, greens, blues));
                    /* Refresh the strip to send data */
                    ESP_ERROR_CHECK(led_strip_refresh(led_strip));
        } else {
            
            ESP_ERROR_CHECK(led_strip_clear(led_strip));
        }
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        esp_task_wdt_reset();
    }
    // 在任务即将结束时移除任务看门狗监控
    ESP_ERROR_CHECK(esp_task_wdt_delete(NULL));

}

void app_main(void)
{
    /*
        Turn of warnings from HTTP server as redirecting traffic will yield
        lots of invalid requests
    */
    esp_log_level_set("httpd_uri", ESP_LOG_ERROR);
    esp_log_level_set("httpd_txrx", ESP_LOG_ERROR);
    esp_log_level_set("httpd_parse", ESP_LOG_ERROR);

    led_strip = configure_led();


    // Initialize networking stack
    ESP_ERROR_CHECK(esp_netif_init());

    // Create default event loop needed by the  main app
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    // Initialize NVS needed by Wi-Fi
    ESP_ERROR_CHECK(nvs_flash_init());

    // Initialize Wi-Fi including netif with default config
    // esp_netif_create_default_wifi_ap();

    // esp_netif_create_default_wifi_sta();

    // Initialise ESP32 in SoftAP mode
    wifi_init_softap();

    #if IP_NAPT
        u32_t napt_netif_ip = 0xC0A80401; // Set to ip address of softAP netif (Default is 192.168.4.1)
        ip_napt_enable(htonl(napt_netif_ip), 1);
        ESP_LOGI(TAG, "------------------------>NAT is enabled");
    #endif

    // Start the server for the first time
    start_webserver();

    // // Start the DNS server that will redirect all queries to the softAP IP
    // start_dns_server();

    xTaskCreate(&blink_task, "blink_task", 2048, NULL, 5, NULL);
}
