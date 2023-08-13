//#include "stdlib.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/apps/sntp.h"
#include "lwip/netdb.h"
#include "driver/uart.h"
#include "esp_timer.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"

#define DEFAULT_SCAN_LIST_SIZE 20
#define ESP_MAXIMUM_RETRY 5
#define ESP_WIFI_SSID "3-Ogorodnaya-55"
#define ESP_WIFI_PASS "@REN@-$0b@k@"
//#define ESP_WIFI_SSID "RADIUS-3"
//#define ESP_WIFI_PASS "temp-s0jdvbrjrv"

#define HOST_IP_ADDR "192.168.1.47"
#define PORT 3333

#define KEEPALIVE_IDLE 60
#define KEEPALIVE_INTERVAL 10
#define KEEPALIVE_COUNT 10

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

char rx_buffer[255];
time_t now;
struct tm timeinfo;

void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
void initialize_sntp(void);
void obtain_time(void);
void sntp_task();
void print_auth_mode(int authmode);
void print_cipher_type(int pairwise_cipher, int group_cipher);
void wifi_scan(void);
void do_retransmit(const int sock);
//void tcp_client_task(void *pvParameters);


