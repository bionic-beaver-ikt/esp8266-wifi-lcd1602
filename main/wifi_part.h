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

#define DEFAULT_SCAN_LIST_SIZE 20
#define EXAMPLE_ESP_MAXIMUM_RETRY 5
#define EXAMPLE_ESP_WIFI_SSID "3-Ogorodnaya-55"
#define EXAMPLE_ESP_WIFI_PASS "@REN@-$0b@k@"
//#define EXAMPLE_ESP_WIFI_SSID "RADIUS-3"
//#define EXAMPLE_ESP_WIFI_PASS "temp-s0jdvbrjrv"

#define HOST_IP_ADDR "192.168.1.47"
#define PORT 3333

#define KEEPALIVE_IDLE 60
#define KEEPALIVE_INTERVAL 10
#define KEEPALIVE_COUNT 10

static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

char rx_buffer[255];
int len = 0;

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data);
static void initialize_sntp(void);
static void obtain_time(void);
static void sntp_example_task();
static void print_auth_mode(int authmode);
static void print_cipher_type(int pairwise_cipher, int group_cipher);
static void wifi_scan(void);
static void do_retransmit(const int sock);
static void tcp_client_task(void *pvParameters);