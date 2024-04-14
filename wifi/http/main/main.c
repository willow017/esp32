#include <string.h>
#include <sys/param.h>
#include <stdlib.h>
#include <ctype.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "protocol_examples_common.h"
#include "protocol_examples_utils.h"
#include "esp_tls.h"
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#include "esp_crt_bundle.h"
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"

#include "esp_http_client.h"
#include "cJSON.h"
#include "esp_sntp.h"
#include "esp_netif_sntp.h"
#include "time.h"

#include "lvgl.h"
#include "lv_demos.h"
#include "bsp/esp-bsp.h"
#include "jianti.c"
#include "freertos/semphr.h"

#include "esp_task_wdt.h"

#define MAX_HTTP_RECV_BUFFER 512
#define MAX_HTTP_OUTPUT_BUFFER 2048
static const char *TAG = "HK";
LV_FONT_DECLARE(jianti)
/* 假定有个 int 类型的变量需要显示 */
int number = 2123;
int hs = 23;
int ms = 12;


/* 创建一个足够大的字符串缓冲区来存放整数及其终止符 */
char str[200] = ""; /* 10 应该足够存放一个 int 型数字 */
lv_obj_t *label;// = lv_label_create(lv_scr_act());
lv_obj_t *h;// = lv_label_create(lv_scr_act());
lv_obj_t *m;// = lv_label_create(lv_scr_act());
lv_obj_t *strs;

// 互斥锁句柄
SemaphoreHandle_t xMutex;

esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    static char *output_buffer;  // Buffer to store response of http request from event handler
    static int output_len;       // Stores number of bytes read
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            // Clean the buffer in case of a new request
            if (output_len == 0 && evt->user_data) {
                // we are just starting to copy the output data into the use
                memset(evt->user_data, 0, MAX_HTTP_OUTPUT_BUFFER);
            }
            /*
             *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
             *  However, event handler can also be used in case chunked encoding is used.
             */
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // If user_data buffer is configured, copy the response into the buffer
                int copy_len = 0;
                if (evt->user_data) {
                    // The last byte in evt->user_data is kept for the NULL character in case of out-of-bound access.
                    copy_len = MIN(evt->data_len, (MAX_HTTP_OUTPUT_BUFFER - output_len));
                    if (copy_len) {
                        memcpy(evt->user_data + output_len, evt->data, copy_len);
                    }
                } else {
                    int content_len = esp_http_client_get_content_length(evt->client);
                    if (output_buffer == NULL) {
                        // We initialize output_buffer with 0 because it is used by strlen() and similar functions therefore should be null terminated.
                        output_buffer = (char *) calloc(content_len + 1, sizeof(char));
                        output_len = 0;
                        if (output_buffer == NULL) {
                            ESP_LOGE(TAG, "Failed to allocate memory for output buffer");
                            return ESP_FAIL;
                        }
                    }
                    copy_len = MIN(evt->data_len, (content_len - output_len));
                    if (copy_len) {
                        memcpy(output_buffer + output_len, evt->data, copy_len);
                    }
                }
                output_len += copy_len;
            }

            break;
        case HTTP_EVENT_ON_FINISH:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            if (output_buffer != NULL) {
                // Response is accumulated in output_buffer. Uncomment the below line to print the accumulated response
                // ESP_LOG_BUFFER_HEX(TAG, output_buffer, output_len);
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            int mbedtls_err = 0;
            esp_err_t err = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
            if (err != 0) {
                ESP_LOGI(TAG, "Last esp error code: 0x%x", err);
                ESP_LOGI(TAG, "Last mbedtls failure: 0x%x", mbedtls_err);
            }
            if (output_buffer != NULL) {
                free(output_buffer);
                output_buffer = NULL;
            }
            output_len = 0;
            break;
        case HTTP_EVENT_REDIRECT:
            ESP_LOGD(TAG, "HTTP_EVENT_REDIRECT");
            esp_http_client_set_header(evt->client, "From", "user@example.com");
            esp_http_client_set_header(evt->client, "Accept", "text/html");
            esp_http_client_set_redirection(evt->client);
            break;
    }
    return ESP_OK;
}

static void https_with_url(void)
{
    char local_response_buffer[MAX_HTTP_OUTPUT_BUFFER + 1] = {0};
    esp_http_client_config_t config = {
        .url = "https://api.vvhan.com/api/dailyEnglish?type=sj",
        .event_handler = _http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .user_data = local_response_buffer, 
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_err_t err = esp_http_client_perform(client);

    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %"PRId64,
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "Error perform http request %s", esp_err_to_name(err));
    }
    ESP_LOG_BUFFER_HEX(TAG, local_response_buffer, strlen(local_response_buffer));
    ESP_LOGI(TAG, "The hexadecimal string is: %s", local_response_buffer);
    esp_http_client_cleanup(client);
    cJSON *pJsonRoot = cJSON_Parse(local_response_buffer);
    cJSON *pValue = cJSON_GetObjectItem(pJsonRoot, "data");      // 解析value字段内容
    if (!pValue)                                                 // 判断value字段是否json格式
    {
        ESP_LOGI(TAG, "The data is not json data");
        return;  
    }                                         
    else
    {
        cJSON *en = cJSON_GetObjectItem(pValue, "en");         // 解析子节点pValue的day字段字符串内容
        if (!en) return;                                        // 判断day字段是否json格式
        else
        {
            if (cJSON_IsString(en))                            
            {
                strcpy(str, en->valuestring);                // 拷贝内容到字符串数组
                ESP_LOGI(TAG, "The en string is: %s", str);
            }
        }
    }
    cJSON_Delete(pJsonRoot);      

}

static void http_test_task(void *pvParameters)
{
    esp_task_wdt_add(NULL);
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
    https_with_url();
#endif
    lv_label_set_long_mode(strs, LV_LABEL_LONG_SCROLL_CIRCULAR);
    lv_obj_set_style_text_font(strs, &jianti, 0);
    lv_obj_set_width(strs, 300);
    lv_label_set_text(strs, str);
    lv_obj_align(strs, LV_ALIGN_TOP_MID, 0, 0);
    //lv_obj_set_pos(strs, 50, 140);
    while (1)
    {
        esp_task_wdt_reset();
        vTaskDelay(pdMS_TO_TICKS(500));
    }
    esp_task_wdt_delete(NULL);
    
}

void app_main(void)
{
    time_t timer;//time_t就是long int 类型
    struct tm *tblock;

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }

    ESP_ERROR_CHECK(ret);
    bsp_i2c_init();
    bsp_display_start();
    /* Set display brightness to 100% */
    bsp_display_backlight_on();

    ESP_LOGI(TAG, "Display LVGL demo");
    bsp_display_lock(0);
    bsp_display_unlock();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    ESP_ERROR_CHECK(example_connect());
    ESP_LOGI(TAG, "Connected to AP, begin http example");
    ESP_LOGI(TAG, "Calibrating time...");

    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG("ntp.tencent.com");
    esp_netif_sntp_init(&config);
    if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000)) != ESP_OK) {
        printf("Failed to update system time within 10s timeout");
    }
    setenv("TZ", "CST-8", 1);
    tzset();
    ESP_LOGI(TAG, "Calibration time successful!");

    h = lv_label_create(lv_scr_act());
    strs = lv_label_create(lv_scr_act());

    /* 设置标签上的文字为我们的数字字符串 */
    lv_obj_set_style_text_font(h, &jianti, 0);
    lv_obj_align(h, LV_ALIGN_TOP_RIGHT, 0, 0);


    xTaskCreate(&http_test_task, "http_test_task", 8192, NULL, 5, NULL);
    //lv_label_set_text(strs, str);
    ESP_LOGI(TAG, "Thee zh string is: %s", str);
    while (1)
    {
        //esp_task_wdt_reset();
        timer = time(NULL);
        tblock = localtime(&timer);

        lv_label_set_text_fmt(h,"%d:%d",tblock->tm_hour,tblock->tm_min); 
        lv_obj_align(h, LV_ALIGN_TOP_RIGHT, 0, 0);
 
        vTaskDelay(1000/portTICK_PERIOD_MS);
    }


}
