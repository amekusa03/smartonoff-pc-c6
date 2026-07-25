#include <esp_err.h>
#include <esp_log.h>
#include <esp_timer.h>
#include <nvs_flash.h>
#include <nvs.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <esp_mac.h>
#include <driver/gpio.h>

#include <esp_matter.h>
#include "pc_control.h"
#include "pc_monitor.h"
#include "wifi_creds.h"
#include "lcd_display.h"
#include "shutdown_client.h"

#include <app/server/Server.h>
#include <setup_payload/OnboardingCodesUtil.h>

static const char *TAG = "app_main";

using namespace esp_matter;
using namespace esp_matter::attribute;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;

static uint16_t s_endpoint_id = 1;

// attribute::update() による内部同期中は PRE_UPDATE コールバックでの
// コマンド実行を抑制するフラグ。
// on_state_mismatch → attribute::update → app_attribute_update_cb の
// 連鎖で PWR-SW が誤動作するのを防ぐ。
static volatile bool s_syncing_attribute = false;

// 乖離確定時のコールバック：実態を Matter 属性に反映してユーザーへ通知する。
// システムはここで ON/OFF 操作を行わない（フェイルセーフ原則）。
static void on_state_mismatch(bool actual_on)
{
    ESP_LOGE(TAG, "PC state mismatch! actual=%s -> syncing attribute, user action required",
             actual_on ? "ON" : "OFF");
    lcd_display_set_message("State Mismatch!");
    s_syncing_attribute = true;
    esp_matter_attr_val_t val = esp_matter_bool(actual_on);
    esp_matter::attribute::update(s_endpoint_id, OnOff::Id, OnOff::Attributes::OnOff::Id, &val);
    s_syncing_attribute = false;
}

#define GPIO_FACTORY_RESET    GPIO_NUM_9
#define FACTORY_RESET_HOLD_MS 3000

// Matter起動時のZCL初期化(NVS保存値→デフォルト値への変更)でコールバックが呼ばれ、
// PCの電源ボタンが誤押下されるのを防ぐ。WiFi接続・monitor起動完了後にtrueになる。
static volatile bool s_system_ready = false;

static void factory_reset_task(void *)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << GPIO_FACTORY_RESET),
        .mode         = GPIO_MODE_INPUT,
        .pull_up_en   = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);

    while (true) {
        if (gpio_get_level(GPIO_FACTORY_RESET) == 0) {
            int held = 0;
            while (gpio_get_level(GPIO_FACTORY_RESET) == 0 && held < FACTORY_RESET_HOLD_MS) {
                vTaskDelay(100 / portTICK_PERIOD_MS);
                held += 100;
            }
            if (held >= FACTORY_RESET_HOLD_MS) {
                ESP_LOGI(TAG, "Factory reset triggered");
                lcd_display_set_message("Factory Reset!");
                esp_matter::factory_reset();
            }
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

static void app_event_cb(const ChipDeviceEvent *event, intptr_t arg)
{
    switch (event->Type) {
    case chip::DeviceLayer::DeviceEventType::kInterfaceIpAddressChanged: {
        // Disable Wi-Fi power save mode (modem sleep) to prevent packet loss / mDNS timeout in Google Home
        esp_wifi_set_ps(WIFI_PS_NONE);
        ESP_LOGI(TAG, "Disabled Wi-Fi power save (WIFI_PS_NONE)");

        esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
        if (netif) {
            esp_netif_ip_info_t info;
            if (esp_netif_get_ip_info(netif, &info) == ESP_OK && info.ip.addr != 0) {
                char ip[20];
                snprintf(ip, sizeof(ip), IPSTR, IP2STR(&info.ip));
                ESP_LOGI(TAG, "IP: %s", ip);
                s_system_ready = true;
                lcd_display_update_wifi(WIFI_STATUS_CONNECTED, ip);
                lcd_display_set_message("System Ready");
            }
        }
        break;
    }
    case chip::DeviceLayer::DeviceEventType::kCommissioningComplete:
        ESP_LOGI(TAG, "Commissioning complete");
        lcd_display_set_message("Commissioned!");
        break;
    case chip::DeviceLayer::DeviceEventType::kFabricRemoved:
        ESP_LOGI(TAG, "Fabric removed");
        lcd_display_set_message("Fabric Removed");
        break;
    default:
        break;
    }
}

static esp_err_t app_identification_cb(identification::callback_type_t type, uint16_t endpoint_id,
                                        uint8_t effect_id, uint8_t effect_variant, void *priv_data)
{
    return ESP_OK;
}

static esp_err_t app_attribute_update_cb(attribute::callback_type_t type, uint16_t endpoint_id,
                                          uint32_t cluster_id, uint32_t attribute_id,
                                          esp_matter_attr_val_t *val, void *priv_data)
{
    if (type != PRE_UPDATE) {
        return ESP_OK;
    }
    if (cluster_id != OnOff::Id || attribute_id != OnOff::Attributes::OnOff::Id) {
        return ESP_OK;
    }
    if (!s_system_ready || s_syncing_attribute) {
        ESP_LOGW(TAG, "Ignoring OnOff update (ready=%d syncing=%d val=%d)",
                 s_system_ready, s_syncing_attribute, val->val.b);
        return ESP_OK;
    }
    // pc_execute_command は s_pressing フラグで再入を防ぐ。
    // Matter再送が複数回呼んでも1回しかボタンを押さない。
    esp_err_t err = pc_execute_command(val->val.b);
    if (err == ESP_OK) {
        // コマンド発行を monitor に通知：過渡期間の乖離検出を抑制する
        pc_monitor_notify_command(val->val.b);
        lcd_display_set_message(val->val.b ? "Cmd: Turn ON" : "Cmd: Received");
    }
    return err;
}

static void store_wifi_credentials(void)
{
    nvs_handle_t nvs;
    if (nvs_open_from_partition("nvs", "chip-config", NVS_READWRITE, &nvs) != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open chip-config namespace");
        return;
    }

    // SSID・パスワード両方が一致していれば書き込みをスキップする。
    // 毎起動の不要な NVS 書き込みを避け、Matter の WiFi 初期化との競合を防ぐ。
    char existing_ssid[64] = {};
    char existing_pass[64] = {};
    size_t ssid_len = sizeof(existing_ssid);
    size_t pass_len = sizeof(existing_pass);
    bool need_write = !(nvs_get_blob(nvs, "wifi-ssid", existing_ssid, &ssid_len) == ESP_OK
                        && ssid_len == strlen(WIFI_SSID)
                        && memcmp(existing_ssid, WIFI_SSID, ssid_len) == 0
                        && nvs_get_blob(nvs, "wifi-pass", existing_pass, &pass_len) == ESP_OK
                        && pass_len == strlen(WIFI_PASSWORD)
                        && memcmp(existing_pass, WIFI_PASSWORD, pass_len) == 0);

    if (need_write) {
        nvs_set_blob(nvs, "wifi-ssid", WIFI_SSID, strlen(WIFI_SSID));
        nvs_set_blob(nvs, "wifi-pass", WIFI_PASSWORD, strlen(WIFI_PASSWORD));
        nvs_commit(nvs);
        ESP_LOGI(TAG, "WiFi credentials written to chip-config: %s", WIFI_SSID);
    } else {
        ESP_LOGI(TAG, "WiFi credentials unchanged: %s", WIFI_SSID);
    }
    nvs_close(nvs);
}

// Matter が chip-config/unique-id から UniqueID を読む。
// erase-flash 後や未製造デバイスでは存在しないため、MAC アドレスから生成して永続化する。
// Google Home は Matter spec では Optional の UniqueID を必須として扱うため、
// この値が欠けると commissioning 後もオフライン表示になる。
static void ensure_unique_id(void)
{
    nvs_handle_t nvs;
    if (nvs_open_from_partition("nvs", "chip-config", NVS_READWRITE, &nvs) != ESP_OK) {
        ESP_LOGE(TAG, "ensure_unique_id: failed to open NVS");
        return;
    }
    char buf[48] = {};
    size_t len = sizeof(buf);
    bool missing = (nvs_get_str(nvs, "unique-id", buf, &len) != ESP_OK || strlen(buf) < 2);
    if (missing) {
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        // 16バイト (32文字 hex) の UniqueID を MAC アドレスから決定的に生成
        snprintf(buf, sizeof(buf),
                 "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X",
                 mac[0], mac[1], mac[2], mac[3], mac[4], mac[5],
                 (uint8_t)(~mac[0]), (uint8_t)(~mac[1]),
                 (uint8_t)(~mac[2]), (uint8_t)(~mac[3]),
                 (uint8_t)(~mac[4]), (uint8_t)(~mac[5]),
                 mac[0] ^ 0x55u, mac[1] ^ 0xAAu,
                 mac[2] ^ 0x33u, mac[3] ^ 0xCCu);
        nvs_set_str(nvs, "unique-id", buf);
        nvs_commit(nvs);
        ESP_LOGI(TAG, "Generated UniqueID: %s", buf);
    } else {
        ESP_LOGI(TAG, "UniqueID exists: %s", buf);
    }
    nvs_close(nvs);
}

static void connect_wifi_station(void)
{
    wifi_config_t wifi_config = {};
    strlcpy((char *)wifi_config.sta.ssid, WIFI_SSID, sizeof(wifi_config.sta.ssid));
    strlcpy((char *)wifi_config.sta.password, WIFI_PASSWORD, sizeof(wifi_config.sta.password));

    esp_wifi_set_mode(WIFI_MODE_STA);
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);
    esp_wifi_connect();
    ESP_LOGI(TAG, "Initiated Wi-Fi connection to %s", WIFI_SSID);
}

extern "C" void app_main()
{
    nvs_flash_init();
    ensure_unique_id();   // Google Home が必須とする UniqueID を NVS に確保
    shutdown_client_init();   // 軽量シャットダウンクライアントの初期化
    lcd_display_init();
    lcd_display_set_message("Starting Matter...");
    pc_control_init();
    store_wifi_credentials();

    node::config_t node_config;
    node_t *node = node::create(&node_config, app_attribute_update_cb, app_identification_cb);
    if (!node) {
        ESP_LOGE(TAG, "Failed to create Matter node");
        return;
    }

    on_off_plug_in_unit::config_t plug_config;
    plug_config.on_off.on_off = (pc_get_power_state() == PC_STATE_ON);
    endpoint_t *endpoint = on_off_plug_in_unit::create(node, &plug_config, ENDPOINT_FLAG_NONE, NULL);
    if (!endpoint) {
        ESP_LOGE(TAG, "Failed to create endpoint");
        return;
    }

    s_endpoint_id = endpoint::get_id(endpoint);
    ESP_LOGI(TAG, "Endpoint ID: %d", s_endpoint_id);

    esp_err_t err = esp_matter::start(app_event_cb);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start Matter: %d", err);
        return;
    }

    connect_wifi_station();

    PrintOnboardingCodes(chip::RendezvousInformationFlags(chip::RendezvousInformationFlag::kOnNetwork));

    pc_monitor_init(on_state_mismatch);

    xTaskCreate(factory_reset_task, "factory_reset", 4096, nullptr, 1, nullptr);
}

