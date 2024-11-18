/* Simple HTTP Server Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_log.h>
#include <esp_system.h>
#include <nvs_flash.h>
#include <sys/param.h>
#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_eth.h"
#include "protocol_examples_common.h"
#include "esp_tls_crypto.h"
#include <esp_http_server.h>
#include <mdns.h>

#include "driver/gpio.h"
#include "driver/ledc.h"

/* A simple example that demonstrates how to create GET and POST
 * handlers for the web server.
 */

static const char *TAG = "living";
static const char *MY_MDNS_HOSTNAME = "living";
static const char *MY_MDNS_INSTANCE_NAME = "living room lights";

#define LED_PWM_FREQ 1000

// Base un-dimmed colour, and dimmer level (all 0..255)
unsigned col_r, col_g, col_b, col_w, level;

ledc_channel_config_t led_chan_g = {
  .gpio_num = GPIO_NUM_16,
  .speed_mode = LEDC_LOW_SPEED_MODE,
  .channel = LEDC_CHANNEL_0,
  .intr_type = LEDC_INTR_DISABLE,
  .timer_sel = LEDC_TIMER_0,
  .duty = 0,
  .hpoint = 0,
  .flags = {.output_invert = 1}
};

ledc_channel_config_t led_chan_r = {
  .gpio_num = GPIO_NUM_17,
  .speed_mode = LEDC_LOW_SPEED_MODE,
  .channel = LEDC_CHANNEL_1,
  .intr_type = LEDC_INTR_DISABLE,
  .timer_sel = LEDC_TIMER_0,
  .duty = 0,
  .hpoint = 0,
  .flags = {.output_invert = 1}
};

ledc_channel_config_t led_chan_b = {
  .gpio_num = GPIO_NUM_18,
  .speed_mode = LEDC_LOW_SPEED_MODE,
  .channel = LEDC_CHANNEL_2,
  .intr_type = LEDC_INTR_DISABLE,
  .timer_sel = LEDC_TIMER_0,
  .duty = 0,
  .hpoint = 0,
  .flags = {.output_invert = 1}
};

ledc_channel_config_t led_chan_w = {
  .gpio_num = GPIO_NUM_19,
  .speed_mode = LEDC_LOW_SPEED_MODE,
  .channel = LEDC_CHANNEL_3,
  .intr_type = LEDC_INTR_DISABLE,
  .timer_sel = LEDC_TIMER_0,
  .duty = 0,
  .hpoint = 0,
  .flags = {.output_invert = 1}
};

#define SETTINGS_NAMESPACE "living"
#define SETTINGS_LEVEL_KEY "l"
#define SETTINGS_R_KEY "r"
#define SETTINGS_G_KEY "g"
#define SETTINGS_B_KEY "b"
#define SETTINGS_W_KEY "w"

void store_settings(unsigned l, unsigned r, unsigned g, unsigned b, unsigned w) {
  nvs_handle_t settings_rw;

  if (nvs_open(SETTINGS_NAMESPACE, NVS_READWRITE, &settings_rw) == ESP_OK) {
    if (nvs_set_u8(settings_rw, SETTINGS_R_KEY, r) == ESP_OK &&
        nvs_set_u8(settings_rw, SETTINGS_G_KEY, g) == ESP_OK &&
        nvs_set_u8(settings_rw, SETTINGS_B_KEY, b) == ESP_OK &&
        nvs_set_u8(settings_rw, SETTINGS_W_KEY, w) == ESP_OK &&
        nvs_set_u8(settings_rw, SETTINGS_LEVEL_KEY, l) == ESP_OK)
    {
      nvs_commit(settings_rw);
        ESP_LOGI(TAG, "Stored settings :)");
    } else {
        ESP_LOGI(TAG, "Failed to store settings :(");
    }
    nvs_close(settings_rw);
  } else {
    ESP_LOGI(TAG, "Failed to open nvs namespace :(");
  }
}

void set_level(unsigned new_level) {
      level = new_level;

      led_chan_r.duty = (col_r*level)/255;
      led_chan_g.duty = (col_g*level)/255;
      led_chan_b.duty = (col_b*level)/255;
      led_chan_w.duty = (col_w*level)/255;

      ledc_channel_config(&led_chan_g);
      ledc_channel_config(&led_chan_r);
      ledc_channel_config(&led_chan_b);
      ledc_channel_config(&led_chan_w);
}

/* An HTTP GET handler */
static esp_err_t read_get_handler(httpd_req_t *req)
{
    size_t buf_len;
    char* resp_str = malloc(200);

    buf_len = sprintf(resp_str,
      "{\"r\":%d,\"g\":%d,\"b\":%d,\"w\":%d,"
      "\"level\":%d,"
      "\"dutyr\":%lu,\"dutyg\":%lu,\"dutyb\":%lu,\"dutyw\":%lu,"
      "\"freq\":%d}",
      col_r, col_g, col_b, col_w,
      level,
      led_chan_r.duty, led_chan_g.duty, led_chan_b.duty, led_chan_w.duty,
      LED_PWM_FREQ);
    httpd_resp_set_hdr(req, "Content-Type", "application/json");
    httpd_resp_send(req, resp_str, buf_len);
    free(resp_str);
    return ESP_OK;
}

static esp_err_t index_get_handler(httpd_req_t *req)
{
    extern char *index_html;

    httpd_resp_set_hdr(req, "Content-Type", "text/html");
    httpd_resp_send(req, index_html, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t ctrl_put_handler(httpd_req_t *req)
{
    char buf[32];
    int ret;
    int r, g, b, w;

    if ((ret = httpd_req_recv(req, buf, 32)) <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }

    if (sscanf(buf, "W%d,%d,%d,%d", &r, &g, &b, &w) == 4) {

      store_settings(level, r, g, b, w);

      col_r = r;
      col_g = g;
      col_b = b;
      col_w = w;

      set_level(level);

      ESP_LOGI(TAG, "Set: r=%d g=%d b=%d w=%d\n", r, g, b, w);

      httpd_resp_send(req, "OK\r\n", HTTPD_RESP_USE_STRLEN);
    } else {
      httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "400 Expected format: W<r>,<g>,<b>,<w>");
    }

    /* Respond with empty body */
    return ESP_OK;
}

static esp_err_t level_put_handler(httpd_req_t *req)
{
    char buf[32];
    int ret;

    if ((ret = httpd_req_recv(req, buf, 32)) <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            httpd_resp_send_408(req);
        }
        return ESP_FAIL;
    }

    int new_level;
    if (sscanf(buf, "L%d", &new_level) == 1) {

      set_level(new_level);

      ESP_LOGI(TAG, "Set level=%d\n", new_level);

      httpd_resp_send(req, "OK\r\n", HTTPD_RESP_USE_STRLEN);
    } else {
      httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "400 Expected format: L<level 0..255>");
    }

    /* Respond with empty body */
    return ESP_OK;
}

static const httpd_uri_t ctrl_route = {
    .uri       = "/ctrl",
    .method    = HTTP_PUT,
    .handler   = ctrl_put_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t level_route = {
    .uri       = "/level",
    .method    = HTTP_PUT,
    .handler   = level_put_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t read_route = {
    .uri       = "/read",
    .method    = HTTP_GET,
    .handler   = read_get_handler,
    .user_ctx  = NULL
};

static const httpd_uri_t index_route = {
    .uri       = "/",
    .method    = HTTP_GET,
    .handler   = index_get_handler,
    .user_ctx  = NULL
};

static httpd_handle_t start_webserver(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    // Start the httpd server
    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &read_route);
        httpd_register_uri_handler(server, &index_route);
        httpd_register_uri_handler(server, &ctrl_route);
        httpd_register_uri_handler(server, &level_route);
        return server;
    }

    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

static esp_err_t stop_webserver(httpd_handle_t server)
{
    return httpd_stop(server);
}

static void disconnect_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server) {
        ESP_LOGI(TAG, "Stopping webserver");
        if (stop_webserver(*server) == ESP_OK) {
            *server = NULL;
        } else {
            ESP_LOGE(TAG, "Failed to stop http server");
        }
    }
}

static void connect_handler(void* arg, esp_event_base_t event_base,
                            int32_t event_id, void* event_data)
{
    httpd_handle_t* server = (httpd_handle_t*) arg;
    if (*server == NULL) {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver();
    }
}


void app_main(void)
{
    static httpd_handle_t server = NULL;


    // living room strip = 6 metres
    // current per 70" = 178 cm
    // W   .650 A
    // R   .643 A
    // G   .675 A
    // B   .619 A
    // white + blue = 1.269 A ... 4.27 A per 6 m
    // mix 1 W + 0.25 (R,G,B) ... 3.82 A per 6 m

    gpio_config_t config = {
      .pin_bit_mask = (1 << GPIO_NUM_16)
                    | (1 << GPIO_NUM_17)
                    | (1 << GPIO_NUM_18)
                    | (1 << GPIO_NUM_19),
      .intr_type = GPIO_INTR_DISABLE,
      .mode = GPIO_MODE_DEF_OUTPUT,
      .pull_down_en = GPIO_PULLDOWN_DISABLE,
      .pull_up_en = GPIO_PULLUP_DISABLE
    };

    col_r = 255;
    col_g = 255;
    col_b = 255;
    col_w = 240;

    set_level(0); // do this before setting outputs to prevent a brief flash after power-on(?)

    gpio_config(&config);

/*
// cycle colours test pattern
    for (int i = 0; ; ++i) {
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        int n = i % 4;
        // levels are inverted: 0 = on, 1 = off
        gpio_set_level(GPIO_NUM_16, n != 0); // g
        gpio_set_level(GPIO_NUM_17, n != 1); // r
        gpio_set_level(GPIO_NUM_18, n != 2); // b
        gpio_set_level(GPIO_NUM_19, n != 3); // w
    }
*/

    ledc_timer_config_t led_timer = {
      .speed_mode = LEDC_LOW_SPEED_MODE,                /*!< LEDC speed speed_mode, high-speed mode or low-speed mode */
      .duty_resolution = LEDC_TIMER_8_BIT,      /*!< LEDC channel duty resolution */
      .timer_num = LEDC_TIMER_0,               /*!< The timer source of channel (0 - 3) */
      .freq_hz = LED_PWM_FREQ,                      /*!< LEDC timer frequency (Hz) */
      .clk_cfg = LEDC_USE_RC_FAST_CLK
    };

    ledc_timer_config(&led_timer);


    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* This helper function configures Wi-Fi or Ethernet, as selected in menuconfig.
     * Read "Establishing Wi-Fi or Ethernet Connection" section in
     * examples/protocols/README.md for more information about this function.
     */
    ESP_ERROR_CHECK(example_connect());

    /* Register event handlers to stop the server when Wi-Fi or Ethernet is disconnected,
     * and re-start it upon connection.
     */
#ifdef CONFIG_EXAMPLE_CONNECT_WIFI
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));
#endif // CONFIG_EXAMPLE_CONNECT_WIFI
#ifdef CONFIG_EXAMPLE_CONNECT_ETHERNET
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &connect_handler, &server));
    ESP_ERROR_CHECK(esp_event_handler_register(ETH_EVENT, ETHERNET_EVENT_DISCONNECTED, &disconnect_handler, &server));
#endif // CONFIG_EXAMPLE_CONNECT_ETHERNET



    mdns_init();
    mdns_hostname_set(MY_MDNS_HOSTNAME);
    mdns_instance_name_set(MY_MDNS_INSTANCE_NAME);
    mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0);

    nvs_handle_t settings;
    uint8_t stored_level, stored_r, stored_g, stored_b, stored_w;

    if (nvs_open(SETTINGS_NAMESPACE, NVS_READONLY, &settings) == ESP_OK) {
      if( nvs_get_u8(settings, SETTINGS_LEVEL_KEY, &stored_level) == ESP_OK &&
          nvs_get_u8(settings, SETTINGS_R_KEY, &stored_r) == ESP_OK &&
          nvs_get_u8(settings, SETTINGS_G_KEY, &stored_g) == ESP_OK &&
          nvs_get_u8(settings, SETTINGS_B_KEY, &stored_b) == ESP_OK &&
          nvs_get_u8(settings, SETTINGS_W_KEY, &stored_w) == ESP_OK )
      {
        ESP_LOGI(TAG, "Found settings: L=%d R=%d G=%d B=%d W=%d",
          stored_level, stored_r, stored_g, stored_b, stored_w);

        col_r = stored_r;
        col_g = stored_g;
        col_b = stored_b;
        col_w = stored_w;

        set_level(stored_level);
      } else {
        ESP_LOGI(TAG, "Did not get settings :(");
      }
      nvs_close(settings);
    } else {
      ESP_LOGI(TAG, "Failed to open nvs namespace :(");
    }

    server = start_webserver();
}
