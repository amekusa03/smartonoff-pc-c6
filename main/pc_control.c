#include "pc_control.h"
#include "shutdown_client.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp_log.h>
#include <driver/usb_serial_jtag.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <string.h>
#include <stdio.h>
#include "wifi_creds.h"

// TCP shutdown client task configuration
#define SHUTDOWN_TASK_STACK_SIZE  4096
#define SHUTDOWN_TASK_PRIORITY    5

static void shutdown_task(void *arg)
{
    esp_err_t err = shutdown_client_execute();
    if (err != ESP_OK) {
        ESP_LOGE("pc_control", "Shutdown client execution failed: 0x%x", err);
    }
    vTaskDelete(NULL);
}

static const char *TAG = "pc_control";

void pc_control_init(void)
{
    ESP_LOGI(TAG, "Initializing PC control subsystem (WOL mode)...");
    // No GPIO initialization needed for WOL
}

pc_power_state_t pc_get_power_state(void)
{
    // USB Serial/JTAG の通信・接続状態により PC が ON / OFF か判定
    bool usb_connected = usb_serial_jtag_is_connected();
    return usb_connected ? PC_STATE_ON : PC_STATE_OFF;
}

static esp_err_t send_wol_packet(void)
{
    ESP_LOGI(TAG, "Sending WOL packet to %s...", PC_MAC_ADDRESS);
    unsigned int m[6];
    if (sscanf(PC_MAC_ADDRESS, "%x:%x:%x:%x:%x:%x", 
            &m[0], &m[1], &m[2], &m[3], &m[4], &m[5]) != 6) {
        ESP_LOGE(TAG, "Invalid MAC address format");
        return ESP_FAIL;
    }
    uint8_t mac[6];
    for (int i = 0; i < 6; i++) {
        mac[i] = (uint8_t)m[i];
    }

    uint8_t magic_packet[102];
    memset(magic_packet, 0xFF, 6);
    for (int i = 0; i < 16; i++) {
        memcpy(&magic_packet[6 + i * 6], mac, 6);
    }

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Failed to create socket");
        return ESP_FAIL;
    }

    int broadcast_enable = 1;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, &broadcast_enable, sizeof(broadcast_enable));

    struct sockaddr_in dest_addr = {};
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(9);
    dest_addr.sin_addr.s_addr = INADDR_BROADCAST;

    int err = sendto(sock, magic_packet, sizeof(magic_packet), 0, (struct sockaddr *)&dest_addr, sizeof(dest_addr));
    if (err < 0) {
        ESP_LOGE(TAG, "Failed to send WOL packet");
    } else {
        ESP_LOGI(TAG, "WOL packet sent successfully");
    }
    
    close(sock);
    return (err < 0) ? ESP_FAIL : ESP_OK;
}

esp_err_t pc_execute_command(bool want_on)
{
    if (want_on) {
        ESP_LOGI(TAG, "Power ON command received: Sending WOL");
        return send_wol_packet();
    } else {
        ESP_LOGI(TAG, "Power OFF command received: Spawning shutdown task");
        // CHIP タスクのブロッキングを防ぐため専用タスクに委譲する
        BaseType_t ret = xTaskCreate(shutdown_task, "shutdown_trig",
                                     SHUTDOWN_TASK_STACK_SIZE, NULL,
                                     SHUTDOWN_TASK_PRIORITY, NULL);
        if (ret != pdPASS) {
            ESP_LOGE(TAG, "Failed to create shutdown task (ret=%d)", ret);
            return ESP_FAIL;
        }
        return ESP_OK;
    }
}

esp_err_t pc_execute_force_shutdown(void)
{
    ESP_LOGW(TAG, "Force shutdown requested: Ignored (Unsupported in WOL mode)");
    return ESP_OK;
}
