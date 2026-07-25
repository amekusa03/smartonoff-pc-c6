#include "shutdown_client.h"
#include "wifi_creds.h"
#include <esp_log.h>
#include <string.h>
#include <lwip/sockets.h>
#include <lwip/netdb.h>
#include <lwip/inet.h>
#include <errno.h>

static const char *TAG = "shutdown_client";

esp_err_t shutdown_client_init(void)
{
    ESP_LOGI(TAG, "Shutdown client initialized");
    return ESP_OK;
}

esp_err_t shutdown_client_execute(void)
{
    char ip_str[16] = {0};

#ifdef PC_STATIC_IP
    strncpy(ip_str, PC_STATIC_IP, sizeof(ip_str) - 1);
    ESP_LOGI(TAG, "Using static IP: %s", ip_str);
#else
    char hostname_buf[64];
    snprintf(hostname_buf, sizeof(hostname_buf), "%s.local", PC_HOSTNAME);
    ESP_LOGI(TAG, "Resolving %s via getaddrinfo...", hostname_buf);

    struct addrinfo hints = {
        .ai_family   = AF_INET,
        .ai_socktype = SOCK_STREAM,
    };
    struct addrinfo *res = NULL;
    int gai_err = getaddrinfo(hostname_buf, NULL, &hints, &res);
    if (gai_err != 0 || res == NULL) {
        ESP_LOGE(TAG, "getaddrinfo failed for %s: %d", hostname_buf, gai_err);
        if (res) freeaddrinfo(res);
        return ESP_FAIL;
    }
    struct in_addr resolved_addr = ((struct sockaddr_in *)res->ai_addr)->sin_addr;
    snprintf(ip_str, sizeof(ip_str), "%s", inet_ntoa(resolved_addr));
    freeaddrinfo(res);
    ESP_LOGI(TAG, "Resolved %s -> %s", hostname_buf, ip_str);
#endif

    int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock < 0) {
        ESP_LOGE(TAG, "Failed to create socket: errno %d", errno);
        return ESP_FAIL;
    }

    // Set timeout to 5 seconds
    struct timeval timeout = {
        .tv_sec = 5,
        .tv_usec = 0
    };
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

    struct sockaddr_in dest_addr;
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_addr.s_addr = inet_addr(ip_str);
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(PC_SHUTDOWN_PORT);

    ESP_LOGI(TAG, "Connecting to %s:%d...", ip_str, PC_SHUTDOWN_PORT);
    if (connect(sock, (struct sockaddr *)&dest_addr, sizeof(dest_addr)) != 0) {
        ESP_LOGE(TAG, "Connection failed: errno %d", errno);
        close(sock);
        return ESP_FAIL;
    }

    ESP_LOGI(TAG, "Sending shutdown token...");
    int err = send(sock, PC_SHUTDOWN_TOKEN, strlen(PC_SHUTDOWN_TOKEN), 0);
    if (err < 0) {
        ESP_LOGE(TAG, "Send failed: errno %d", errno);
        close(sock);
        return ESP_FAIL;
    }

    // Wait for peer response or close
    char rx_buf[32];
    int len = recv(sock, rx_buf, sizeof(rx_buf) - 1, 0);
    if (len > 0) {
        rx_buf[len] = '\0';
        ESP_LOGI(TAG, "Received response: %s", rx_buf);
    }

    close(sock);
    ESP_LOGI(TAG, "Shutdown trigger sent successfully");
    return ESP_OK;
}
