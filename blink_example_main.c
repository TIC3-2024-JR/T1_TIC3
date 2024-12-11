#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <fcntl.h>
#include <errno.h>

#include "esp_event.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "lwip/err.h"
#include "lwip/sys.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "lwip/sockets.h"

#define WIFI_SSID "LAB.SISTEMAS DE COMUNICACIONES"
#define WIFI_PASSWORD "Comunicaciones"
#define SERVER_IP "192.168.0.208"
#define SERVER_PORT 1234

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1
#define LED_PIN GPIO_NUM_19

static const char *TAG = "ESP32";
static int s_retry_num = 0;
static EventGroupHandle_t s_wifi_event_group;
static int sending_data = 0;

void init_led() {
    gpio_set_direction(LED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(LED_PIN, 0);
}

void event_handler(void *arg, esp_event_base_t event_base,
                   int32_t event_id, void *event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < 10) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "Reintentando conectar al AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG, "No se pudo conectar al AP");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Dirección IP obtenida: " IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(const char *ssid, const char *password) {
    s_wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL, &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL, &instance_got_ip));

    wifi_config_t wifi_config;
    memset(&wifi_config, 0, sizeof(wifi_config_t));
    strcpy((char *)wifi_config.sta.ssid, ssid);
    strcpy((char *)wifi_config.sta.password, password);
    wifi_config.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wifi_config.sta.pmf_cfg.capable = true;
    wifi_config.sta.pmf_cfg.required = false;

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "wifi_init_sta finalizado.");

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
                                           WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                                           pdFALSE, pdFALSE, portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "Conectado al AP SSID:%s password:%s", ssid, password);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "No se pudo conectar al SSID:%s, password:%s", ssid, password);
    } else {
        ESP_LOGE(TAG, "Evento inesperado");
    }

    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, instance_got_ip));
    ESP_ERROR_CHECK(esp_event_handler_instance_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, instance_any_id));
    vEventGroupDelete(s_wifi_event_group);
}

// Inicializa NVS
void nvs_init() {
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

// Genera datos simulados según las especificaciones
void simulate_sensors(char *buffer, int buffer_size) {
    static int n = 0;
    if (n >= 2000) n = 0; // Reinicia n después de 2000

    float acc_x = 2 * sinf(2 * M_PI * 0.001 * n);
    float acc_y = 3 * cosf(2 * M_PI * 0.001 * n);
    float acc_z = 10 * sinf(2 * M_PI * 0.001 * n);

    float temp = 5.0 + ((float)rand() / RAND_MAX) * (30.0 - 5.0);
    float hum = 30.0 + ((float)rand() / RAND_MAX) * (80.0 - 30.0);
    float pres = 1000.0 + ((float)rand() / RAND_MAX) * (1200.0 - 1000.0);
    float co = 30.0 + ((float)rand() / RAND_MAX) * (200.0 - 30.0);
    int batt = 1 + rand() % 100;

    float amp_x = 0.0059 + ((float)rand() / RAND_MAX) * (0.12 - 0.0059);
    float frec_x = 29.0 + ((float)rand() / RAND_MAX) * (31.0 - 29.0);
    float amp_y = 0.0041 + ((float)rand() / RAND_MAX) * (0.11 - 0.0041);
    float frec_y = 59.0 + ((float)rand() / RAND_MAX) * (61.0 - 59.0);
    float amp_z = 0.008 + ((float)rand() / RAND_MAX) * (0.15 - 0.008);
    float frec_z = 89.0 + ((float)rand() / RAND_MAX) * (91.0 - 89.0);

    n++;

    snprintf(buffer, buffer_size,
             "{\"acc_x\":%.2f,\"acc_y\":%.2f,\"acc_z\":%.2f,\"temp\":%.2f,\"hum\":%.2f,\"pres\":%.2f,\"co\":%.2f,\"batt\":%d,"
             "\"amp_x\":%.4f,\"frec_x\":%.2f,\"amp_y\":%.4f,\"frec_y\":%.2f,\"amp_z\":%.4f,\"frec_z\":%.2f}",
             acc_x, acc_y, acc_z, temp, hum, pres, co, batt,
             amp_x, frec_x, amp_y, frec_y, amp_z, frec_z);
}

void socket_tcp() {
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &server_addr.sin_addr.s_addr);

    while (1) { // Bucle para reconectar en caso de desconexión
        int sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (sock < 0) {
            ESP_LOGE(TAG, "Error al crear el socket");
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
        }

        ESP_LOGI(TAG, "Intentando conectar al servidor...");
        if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
            ESP_LOGE(TAG, "Error al conectar");
            close(sock);
            vTaskDelay(3000 / portTICK_PERIOD_MS);
            continue;
        }

        // Encender el LED al establecer la conexión TCP
        ESP_LOGI(TAG, "Conexión TCP establecida con el servidor");
        gpio_set_level(LED_PIN, 1);
        int led_state = 1;

        // Establece timeout para recv
        struct timeval timeout;
        timeout.tv_sec = 0;
        timeout.tv_usec = 100000;
        setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        char rx_buffer[128];
        uint32_t led_last_toggle_time = 0;

        while (1) {
            if (!sending_data) {

                // Espera comando de inicio
                int rx_len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
                if (rx_len > 0) {
                    rx_buffer[rx_len] = '\0';
                    ESP_LOGI(TAG, "Recibido: %s", rx_buffer);

                    if (strcmp(rx_buffer, "start") == 0) {
                        sending_data = 1;           // Activa el envío de datos
                        led_last_toggle_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
                    }
                } else if (rx_len == 0) {
                    // Conexión cerrada
                    ESP_LOGI(TAG, "Conexión cerrada por el servidor");
                    break;
                } else if (errno != EWOULDBLOCK && errno != EAGAIN) {
                    // Error en recv
                    ESP_LOGE(TAG, "Error en recv: errno %d", errno);
                    break;
                }

                vTaskDelay(10 / portTICK_PERIOD_MS); // Pequeño retraso para evitar uso alto de CPU
            } else {
                // Envía datos
                char tx_buffer[512];
                simulate_sensors(tx_buffer, sizeof(tx_buffer));
                int err = send(sock, tx_buffer, strlen(tx_buffer), 0);
                if (err < 0) {
                    ESP_LOGE(TAG, "Error al enviar datos: errno %d", errno);
                    break;
                }
                ESP_LOGI(TAG, "Enviando: %s", tx_buffer);

                // Parpadeo del LED sin bloqueo
                uint32_t current_time = xTaskGetTickCount() * portTICK_PERIOD_MS;
                if (current_time - led_last_toggle_time >= 200) {
                    led_state = !led_state;
                    gpio_set_level(LED_PIN, led_state);
                    led_last_toggle_time = current_time;
                }

                // Revisa si hay una señal de `stop`
                int rx_len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
                if (rx_len > 0) {
                    rx_buffer[rx_len] = '\0';
                    if (strcmp(rx_buffer, "stop") == 0) {
                        ESP_LOGI(TAG, "Detener envío de datos");
                        sending_data = 0;
                        // Asegura que el LED permanezca encendido
                        gpio_set_level(LED_PIN, 1);
                        gpio_set_level(LED_PIN, 0);
                        led_state = 0;
                    }
                } else if (rx_len == 0) {
                    // Conexión cerrada
                    ESP_LOGI(TAG, "Conexión cerrada por el servidor");
                    break;
                } else if (errno != EWOULDBLOCK && errno != EAGAIN) {
                    // Error en recv
                    ESP_LOGE(TAG, "Error en recv: errno %d", errno);
                    break;
                }

                vTaskDelay(10 / portTICK_PERIOD_MS); // Controla la frecuencia de transmisión
            }
        }

        // Cierra el socket y apaga el LED
        close(sock);
        gpio_set_level(LED_PIN, 0);
        sending_data = 0;
        ESP_LOGI(TAG, "Desconectado del servidor. Reintentando en 3 segundo...");
        vTaskDelay(3000 / portTICK_PERIOD_MS);
    }
}

void app_main(void) {
    nvs_init();
    init_led(); // Inicializa el LED antes de iniciar el WiFi
    wifi_init_sta(WIFI_SSID, WIFI_PASSWORD);
    ESP_LOGI(TAG, "Conectado a WiFi!");
    socket_tcp();
}
