#include "wifi_config.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include <string.h>

#define TAG "WIFI_MANAGER"
#define AP_SSID "ESP32_C3_CONFIG"
#define AP_PASS "12345678"
#define MAX_WIFI_CREDENTIALS 5
#define MAX_RETRY_PER_WIFI 3

// NVS keys
#define NVS_NAMESPACE "wifi_manager"
#define NVS_KEY_CREDENTIALS "credentials"

// Struct for one WiFi credential
typedef struct
{
    char ssid[33];
    char password[65];
} wifi_credential_t;

// Struct to hold all credentials
typedef struct
{
    int count;
    wifi_credential_t credentials[MAX_WIFI_CREDENTIALS];
} wifi_credentials_list_t;

static httpd_handle_t server = NULL;
static wifi_connected_cb_t connected_callback = NULL;
static SemaphoreHandle_t connection_semaphore; // To signal connection success/failure
static SemaphoreHandle_t state_mutex;          // To protect access to the global state
static volatile wifi_manager_state_t g_current_state = WIFI_STATE_UNINITIALIZED;

// --- State Management ---
void wifi_config_set_state(wifi_manager_state_t state)
{
    if (xSemaphoreTake(state_mutex, portMAX_DELAY))
    {
        g_current_state = state;
        xSemaphoreGive(state_mutex);
    }
}

wifi_manager_state_t wifi_config_get_state(void)
{
    wifi_manager_state_t current_state = WIFI_STATE_UNINITIALIZED;
    if (xSemaphoreTake(state_mutex, portMAX_DELAY))
    {
        current_state = g_current_state;
        xSemaphoreGive(state_mutex);
    }
    return current_state;
}

// --- Foward declarations ---
static void start_softap_mode(void);

// --- Web Server ---
static const char html_page[] =
    "<!DOCTYPE html><html><head>"
    "<meta name='viewport' content='width=device-width,initial-scale=1'>"
    "<title>WiFi Config</title>"
    "<style>"
    "body{font-family:Arial;text-align:center;margin:50px;background-color:#f2f2f2;}"
    "h2{color:#333;}"
    "form{background-color:white;padding:20px;border-radius:10px;display:inline-block;box-shadow:0 4px 8px rgba(0,0,0,0.1);}"
    "input{padding:10px;margin:10px;width:250px;font-size:16px;border:1px solid #ccc;border-radius:5px;}"
    "button{padding:12px 30px;font-size:16px;background-color:#4CAF50;color:white;border:none;border-radius:5px;cursor:pointer;}"
    "button:hover{background-color:#45a049;}"
    "</style></head><body>"
    "<h2>ESP32 WiFi Configuration</h2>"
    "<form action='/save' method='post'>"
    "<input type='text' name='ssid' placeholder='WiFi SSID' required><br>"
    "<input type='password' name='pass' placeholder='Password'><br>"
    "<button type='submit'>Save & Connect</button>"
    "</form></body></html>";

static const char success_page[] =
    "<!DOCTYPE html><html><head><title>Success</title>"
    "<meta http-equiv='refresh' content='5;url=/' />"
    "<style>body{font-family:Arial;text-align:center;margin:50px;}</style></head><body>"
    "<h2>WiFi Configured!</h2><p>ESP32 will restart and attempt to connect to the new WiFi network...</p>"
    "</body></html>";

static esp_err_t root_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, html_page, HTTPD_RESP_USE_STRLEN);
    return ESP_OK;
}

static esp_err_t save_handler(httpd_req_t *req)
{
    char buf[256];
    wifi_credential_t new_cred = {0};
    int ret, remaining = req->content_len;

    ret = httpd_req_recv(req, buf, MIN(remaining, sizeof(buf) - 1));
    if (ret <= 0)
    {
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    buf[ret] = '\0';

    if (httpd_query_key_value(buf, "ssid", new_cred.ssid, sizeof(new_cred.ssid)) != ESP_OK ||
        httpd_query_key_value(buf, "pass", new_cred.password, sizeof(new_cred.password)) != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to parse SSID or password");
        httpd_resp_set_status(req, "400 Bad Request");
        httpd_resp_send(req, "Failed to parse SSID or password", HTTPD_RESP_USE_STRLEN);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Received new WiFi credential - SSID: %s", new_cred.ssid);

    // Load existing credentials
    wifi_credentials_list_t creds_list;
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error opening NVS handle: %s", esp_err_to_name(err));
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }

    size_t required_size = 0;
    err = nvs_get_blob(nvs_handle, NVS_KEY_CREDENTIALS, NULL, &required_size);
    if (err == ESP_OK)
    {
        nvs_get_blob(nvs_handle, NVS_KEY_CREDENTIALS, &creds_list, &required_size);
    }
    else
    {
        creds_list.count = 0; // No credentials saved yet
    }

    // Add new credential to the front, shifting others
    for (int i = creds_list.count; i > 0; i--)
    {
        if (i < MAX_WIFI_CREDENTIALS)
        {
            memcpy(&creds_list.credentials[i], &creds_list.credentials[i - 1], sizeof(wifi_credential_t));
        }
    }
    memcpy(&creds_list.credentials[0], &new_cred, sizeof(wifi_credential_t));
    if (creds_list.count < MAX_WIFI_CREDENTIALS)
    {
        creds_list.count++;
    }

    // Save updated list to NVS
    err = nvs_set_blob(nvs_handle, NVS_KEY_CREDENTIALS, &creds_list, sizeof(creds_list));
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Failed to save credentials to NVS: %s", esp_err_to_name(err));
    }
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);

    httpd_resp_send(req, success_page, HTTPD_RESP_USE_STRLEN);

    ESP_LOGI(TAG, "Restarting in 2 seconds...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    esp_restart();

    return ESP_OK;
}

static httpd_handle_t start_webserver(void)
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.uri_match_fn = httpd_uri_match_wildcard;

    if (httpd_start(&server, &config) == ESP_OK)
    {
        httpd_uri_t root_uri = {"/", HTTP_GET, root_handler, NULL};
        httpd_register_uri_handler(server, &root_uri);
        httpd_uri_t save_uri = {"/save", HTTP_POST, save_handler, NULL};
        httpd_register_uri_handler(server, &save_uri);
        ESP_LOGI(TAG, "HTTP Server started on port 80");
        return server;
    }
    ESP_LOGE(TAG, "Failed to start HTTP server");
    return NULL;
}

// --- WiFi Logic ---
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGW(TAG, "Disconnected from WiFi.");
        // Signal that the connection attempt failed. The connection loop will handle retries.
        xSemaphoreGive(connection_semaphore);
    }
    else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP address: " IPSTR, IP2STR(&event->ip_info.ip));
        wifi_config_set_state(WIFI_STATE_CONNECTED);
        if (connected_callback)
        {
            connected_callback();
        }
        // Signal that the connection was successful
        xSemaphoreGive(connection_semaphore);
    }
}

static void start_softap_mode(void)
{
    ESP_LOGI(TAG, "Starting SoftAP mode...");
    wifi_config_set_state(WIFI_STATE_AP_MODE);

    esp_netif_t *ap_netif = esp_netif_create_default_wifi_ap();
    if (ap_netif == NULL)
    {
        ESP_LOGE(TAG, "Failed to create default wifi ap");
        esp_restart();
    }

    wifi_config_t wifi_config = {
        .ap = {
            .ssid = AP_SSID,
            .ssid_len = strlen(AP_SSID),
            .password = AP_PASS,
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK},
    };
    if (strlen(AP_PASS) == 0)
    {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_AP));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "SoftAP started. SSID: %s, Password: %s", AP_SSID, AP_PASS);
    ESP_LOGI(TAG, "Connect to this network and navigate to http://192.168.4.1");

    start_webserver();
}

static void try_connect_all_saved_wifi(void)
{
    wifi_credentials_list_t creds_list;

    // Load credentials from NVS
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGI(TAG, "No NVS data found. Starting SoftAP.");
        start_softap_mode();
        return;
    }

    size_t required_size = sizeof(wifi_credentials_list_t);
    err = nvs_get_blob(nvs_handle, NVS_KEY_CREDENTIALS, &creds_list, &required_size);
    nvs_close(nvs_handle);

    if (err != ESP_OK || creds_list.count == 0)
    {
        ESP_LOGI(TAG, "No WiFi credentials found in NVS. Starting SoftAP.");
        start_softap_mode();
        return;
    }

    ESP_LOGI(TAG, "Found %d saved WiFi credentials. Trying to connect...", creds_list.count);

    esp_netif_t *sta_netif = esp_netif_create_default_wifi_sta();
    if (sta_netif == NULL)
    {
        ESP_LOGE(TAG, "Failed to create default wifi sta");
        start_softap_mode();
        return;
    }
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));

    // Loop through all saved credentials
    for (int i = 0; i < creds_list.count; i++)
    {
        wifi_credential_t *cred = &creds_list.credentials[i];
        ESP_LOGI(TAG, "Trying to connect to SSID: '%s'", cred->ssid);

        wifi_config_t wifi_config = {0};
        strcpy((char *)wifi_config.sta.ssid, cred->ssid);
        strcpy((char *)wifi_config.sta.password, cred->password);
        wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;

        ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

        // Try to connect to this specific network with retries
        for (int retry = 0; retry < MAX_RETRY_PER_WIFI; retry++)
        {
            wifi_config_set_state(WIFI_STATE_CONNECTING);
            ESP_ERROR_CHECK(esp_wifi_start());

            // Wait for connection result (success or disconnect event)
            if (xSemaphoreTake(connection_semaphore, pdMS_TO_TICKS(15000)) == pdTRUE)
            { // 15s timeout
                if (wifi_config_get_state() == WIFI_STATE_CONNECTED)
                {
                    ESP_LOGI(TAG, "Successfully connected to '%s'!", cred->ssid);
                    return; // Exit function on success
                }
            }
            else
            {
                ESP_LOGW(TAG, "Connection attempt to '%s' timed out.", cred->ssid);
            }

            ESP_LOGW(TAG, "Failed to connect to '%s' (attempt %d/%d).", cred->ssid, retry + 1, MAX_RETRY_PER_WIFI);
            esp_wifi_stop(); // Stop wifi before next attempt
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
    }

    // If we get here, all credentials failed
    ESP_LOGE(TAG, "Failed to connect to any of the %d saved WiFi networks.", creds_list.count);
    esp_wifi_stop();
    esp_netif_destroy_default_wifi(esp_netif_get_handle_from_ifkey("WIFI_STA_DEF"));

    start_softap_mode();
}

esp_err_t wifi_config_start(wifi_connected_cb_t callback)
{
    connected_callback = callback;
    connection_semaphore = xSemaphoreCreateBinary();
    state_mutex = xSemaphoreCreateMutex();
    wifi_config_set_state(WIFI_STATE_UNINITIALIZED);

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Register event handlers
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL));

    try_connect_all_saved_wifi();

    return ESP_OK;
}

esp_err_t wifi_config_reset(void)
{
    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK)
    {
        ESP_LOGE(TAG, "Error opening NVS handle to erase: %s", esp_err_to_name(err));
        return err;
    }
    err = nvs_erase_key(nvs_handle, NVS_KEY_CREDENTIALS);
    if (err != ESP_OK && err != ESP_ERR_NVS_NOT_FOUND)
    {
        ESP_LOGE(TAG, "Error erasing NVS key: %s", esp_err_to_name(err));
    }
    else
    {
        ESP_LOGI(TAG, "WiFi credentials erased from NVS.");
        err = ESP_OK; // Treat not_found as success
    }
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return err;
}
