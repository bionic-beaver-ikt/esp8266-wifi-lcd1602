#include "stdlib.h"
#include <time.h>
#include "esp_system.h"
#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "nvs.h"
#include "nvs_flash.h"
#include "driver/gpio.h"
#include "string.h"
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "freertos/task.h"
#include "driver/adc.h"
#include "esp_wifi.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/apps/sntp.h"
#include "lwip/netdb.h"
#include "driver/uart.h"
#include "esp_timer.h"

#define RS 13
#define E 12
#define D4 14
#define D5 16
#define D6 4
#define D7 5
#define LED 2

#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<RS) | (1ULL<<E) | (1ULL<<LED))
#define GPIO_OUTPUT_PIN_SEL2 ((1ULL<<D4) | (1ULL<<D5) | (1ULL<<D6) | (1ULL<<D7))

#define DEFAULT_SCAN_LIST_SIZE 20
#define EXAMPLE_ESP_MAXIMUM_RETRY 5
#define EXAMPLE_ESP_WIFI_SSID "3-Ogorodnaya-55"
#define EXAMPLE_ESP_WIFI_PASS "@REN@-$0b@k@"
//#define EXAMPLE_ESP_WIFI_SSID "RADIUS-3"
//#define EXAMPLE_ESP_WIFI_PASS "temp-s0jdvbrjrv"

#define HOST_IP_ADDR "192.168.1.47"
#define PORT 3333

//static const char *payload = "Message from ESP32 ";

#define KEEPALIVE_IDLE 60
#define KEEPALIVE_INTERVAL 10
#define KEEPALIVE_COUNT 10
//.ssid = "3-Ogorodnaya-55",
//.password = "@REN@-$0b@k@",

static EventGroupHandle_t s_wifi_event_group;

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

static void command (unsigned char data);
static void lcd_send (unsigned char data);
static void lcd_sendstr (char* data);
static void lcd_init ();
static void set_position (unsigned char stroka, unsigned char symb);

static const char *TAG = "TEMP VIEWER";
static int s_retry_num = 0;

SemaphoreHandle_t xSemaphore;
time_t now;
    struct tm timeinfo;

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "ntp2.niiftri.irkutsk.ru");
    sntp_init();
}

static void obtain_time(void)
{
    initialize_sntp();
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;

    while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }
}

static void sntp_example_task()
{
	char strftime_buf[64];
	time(&now);
	localtime_r(&now, &timeinfo);
	ESP_LOGI(TAG, "timeinfo.tm_year = %d", timeinfo.tm_year);
    if (timeinfo.tm_year < (2016 - 1900)) {
        ESP_LOGI(TAG, "Time is not set yet. Connecting to WiFi and getting time over NTP.");
        obtain_time();
    }
    setenv("TZ", "<+08>-8", 1);
    tzset();
    time(&now);
    localtime_r(&now, &timeinfo);
    if (timeinfo.tm_year < (2016 - 1900)) ESP_LOGE(TAG, "The current date/time error");
    else {
        strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
	ESP_LOGI(TAG, "Date: %02d-%02d-%04d. Time: %02d:%02d:%02d", timeinfo.tm_mday, timeinfo.tm_mon+1, timeinfo.tm_year+1900, timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    	//ESP_LOGI(TAG, "The current date/time in Irkutsk is: %s", strftime_buf);
    }
}

static void print_auth_mode(int authmode)
{
    switch (authmode) {
    case WIFI_AUTH_OPEN:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_OPEN");
        break;
    case WIFI_AUTH_WEP:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WEP");
        break;
    case WIFI_AUTH_WPA_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA_PSK");
        break;
    case WIFI_AUTH_WPA2_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_PSK");
        break;
    case WIFI_AUTH_WPA_WPA2_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA_WPA2_PSK");
        break;
    case WIFI_AUTH_WPA2_ENTERPRISE:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_ENTERPRISE");
        break;
    case WIFI_AUTH_WPA3_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA3_PSK");
        break;
    case WIFI_AUTH_WPA2_WPA3_PSK:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_WPA2_WPA3_PSK");
        break;
    default:
        ESP_LOGI(TAG, "Authmode \tWIFI_AUTH_UNKNOWN");
        break;
    }
}

static void print_cipher_type(int pairwise_cipher, int group_cipher)
{
    switch (pairwise_cipher) {
    case WIFI_CIPHER_TYPE_NONE:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_NONE");
        break;
    case WIFI_CIPHER_TYPE_WEP40:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP40");
        break;
    case WIFI_CIPHER_TYPE_WEP104:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_WEP104");
        break;
    case WIFI_CIPHER_TYPE_TKIP:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP");
        break;
    case WIFI_CIPHER_TYPE_CCMP:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_CCMP");
        break;
    case WIFI_CIPHER_TYPE_TKIP_CCMP:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP");
        break;
    default:
        ESP_LOGI(TAG, "Pairwise Cipher \tWIFI_CIPHER_TYPE_UNKNOWN");
        break;
    }

    switch (group_cipher) {
    case WIFI_CIPHER_TYPE_NONE:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_NONE");
        break;
    case WIFI_CIPHER_TYPE_WEP40:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_WEP40");
        break;
    case WIFI_CIPHER_TYPE_WEP104:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_WEP104");
        break;
    case WIFI_CIPHER_TYPE_TKIP:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP");
        break;
    case WIFI_CIPHER_TYPE_CCMP:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_CCMP");
        break;
    case WIFI_CIPHER_TYPE_TKIP_CCMP:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_TKIP_CCMP");
        break;
    default:
        ESP_LOGI(TAG, "Group Cipher \tWIFI_CIPHER_TYPE_UNKNOWN");
        break;
    }
}

static void wifi_scan(void)
{
	s_wifi_event_group = xEventGroupCreate();
	tcpip_adapter_init();
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    uint16_t number = DEFAULT_SCAN_LIST_SIZE;
    wifi_ap_record_t ap_info[DEFAULT_SCAN_LIST_SIZE];
    uint16_t ap_count = 0;
    memset(ap_info, 0, sizeof(ap_info));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    esp_wifi_scan_start(NULL, true);
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_LOGI(TAG, "Total APs scanned = %u", ap_count);
    for (int i = 0; (i < DEFAULT_SCAN_LIST_SIZE) && (i < ap_count); i++) {
        ESP_LOGI(TAG, "SSID \t\t%s", ap_info[i].ssid);
        ESP_LOGI(TAG, "RSSI \t\t%d", ap_info[i].rssi);
        print_auth_mode(ap_info[i].authmode);
        if (ap_info[i].authmode != WIFI_AUTH_WEP) {
            print_cipher_type(ap_info[i].pairwise_cipher, ap_info[i].group_cipher);
        }
        ESP_LOGI(TAG, "Channel \t\t%d\n", ap_info[i].primary);
    }
	ESP_ERROR_CHECK(esp_wifi_stop() );
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,ESP_EVENT_ANY_ID,&event_handler,NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT,IP_EVENT_STA_GOT_IP,&event_handler,NULL));
    wifi_config_t wifi_config = {
        .sta = {
            .ssid = EXAMPLE_ESP_WIFI_SSID,
            .password = EXAMPLE_ESP_WIFI_PASS,
			.threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .pmf_cfg = {
                .capable = true,
                .required = false
            },
        },
    };
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config) );
    ESP_ERROR_CHECK(esp_wifi_start() );

    ESP_LOGI(TAG, "wifi_init_sta finished.");
	EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,pdFALSE,pdFALSE,portMAX_DELAY);
	if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 EXAMPLE_ESP_WIFI_SSID, EXAMPLE_ESP_WIFI_PASS);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }

    ESP_ERROR_CHECK(esp_event_handler_unregister(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler));
    ESP_ERROR_CHECK(esp_event_handler_unregister(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler));
    vEventGroupDelete(s_wifi_event_group);
}

/*static void do_retransmit(const int sock)
{
    int len;
    //char rx_buffer[128];
    do {
        len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
        if (len < 0) {
            ESP_LOGE(TAG, "Error occurred during receiving: errno %d", errno);
        } else if (len == 0) {
            ESP_LOGW(TAG, "Connection closed");
        } else {
            rx_buffer[len] = 0; // Null-terminate whatever is received and treat it like a string
            ESP_LOGI(TAG, "Received %d bytes: %s", len, rx_buffer);

            // send() can return less bytes than supplied length.
            // Walk-around for robust implementation.
            int to_write = len;
            while (to_write > 0) {
                int written = send(sock, rx_buffer + (len - to_write), to_write, 0);
                if (written < 0) {
                    ESP_LOGE(TAG, "Error occurred during sending: errno %d", errno);
                }
                to_write -= written;
            }
        }
    } while (len > 0);
}*/

static void _us_delay(uint32_t time_us)
{
//    ets_delay_us(time_us*1000);
    //    os_delay_us(time_us);
	vTaskDelay(time_us / portTICK_RATE_MS);
//	vTaskDelay(time_us);
}

static void _delay(uint32_t time_us)
{
//    ets_delay_us(time_us*1000);
    //    os_delay_us(time_us);
	vTaskDelay(time_us / portTICK_PERIOD_MS);
	//vTaskDelay(time_us);
}

long long int Timer1;
  //printf("Timer: %lld μs\n", Timer1);
  //vTaskDelay(1);
  long long int Timer2;
  //printf("Timer: %lld μs\n", Timer2);
  long long int diff;
  //printf("Difference: %lld mks\n", diff);

static void lcd_init()
{
    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL2;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    Timer1 = esp_timer_get_time();
	_delay(1); //14
	Timer2 = esp_timer_get_time();
	diff = Timer2 - Timer1;
	printf("Difference: %lld mks\n", diff);

	_delay(50); //50ms
	vTaskDelay(10 / portTICK_PERIOD_MS); //10ms
	vTaskDelay(50 / portTICK_PERIOD_MS); //50ms
	vTaskDelay(1); //10ms
	vTaskDelay(10); //100ms
	vTaskDelay(15); //150ms
	vTaskDelay(50); //500ms

//	gpio_set_level(VDD, 0);
	gpio_set_level(RS, 0);
	gpio_set_level(E, 0);
	gpio_set_level(D4, 0);
	gpio_set_level(D5, 0);
	gpio_set_level(D6, 0);
	gpio_set_level(D7, 0);
	//_delay(10);
	//gpio_set_level(VDD, 1);

    ESP_LOGI(TAG, "START");
	gpio_set_level(E, 1);
    ESP_LOGI(TAG, "-");
	_delay(10);
    ESP_LOGI(TAG, "10");
	gpio_set_level(D4, 1);
	gpio_set_level(D5, 1);
	gpio_set_level(E, 0);
	_delay(10);
    ESP_LOGI(TAG, "10");
	gpio_set_level(E, 1);
	_delay(10);
    ESP_LOGI(TAG, "10");
	gpio_set_level(E, 0);
	_delay(10);
    ESP_LOGI(TAG, "10");
	gpio_set_level(E, 1);
	_delay(10);
	ESP_LOGI(TAG, "10");
	gpio_set_level(E, 0);
	_delay(10);
    ESP_LOGI(TAG, "10");
	gpio_set_level(E, 1);
	_delay(10);
    ESP_LOGI(TAG, "10");
	gpio_set_level(D4, 0);
	gpio_set_level(E, 0);
	_delay(10);
    ESP_LOGI(TAG, "10");

    command(0b00101000); //4-bit, 2-lines
    command(0b00001100); //display_on, cursor_off, blink_off
    //command(0b00001111); //display_on, cursor_on, blink_on
    command(0b00000110); //cursor_right, no_display_shift

    command(0b01000000);
    lcd_send(0b00000100);
    lcd_send(0b00001110);
    lcd_send(0b00010101);
    lcd_send(0b00000100);
    lcd_send(0b00000100);
    lcd_send(0b00000100);
    lcd_send(0b00000100);
    lcd_send(0b00000100);

    lcd_send(0b00000100);
    lcd_send(0b00000100);
    lcd_send(0b00000100);
    lcd_send(0b00000100);
    lcd_send(0b00000100);
    lcd_send(0b00010101);
    lcd_send(0b00001110);
    lcd_send(0b00000100);

    lcd_send(0b00000110);
    lcd_send(0b00001001);
    lcd_send(0b00001001);
    lcd_send(0b00000110);
    lcd_send(0b00000000);
    lcd_send(0b00000000);
    lcd_send(0b00000000);
    lcd_send(0b00000000);

    lcd_send(0b00001111);
    lcd_send(0b00000111);
    lcd_send(0b00000111);
    lcd_send(0b00000011);
    lcd_send(0b00000011);
    lcd_send(0b00000001);
    lcd_send(0b00000001);
    lcd_send(0b00000000);

    lcd_send(0b00010001);
    lcd_send(0b00001110);
    lcd_send(0b00010001);
    lcd_send(0b00010101);
    lcd_send(0b00010001);
    lcd_send(0b00001110);
    lcd_send(0b00001010);
    lcd_send(0b00011011);

    command(0b00000001);
    _delay(20);
}

static void command (unsigned char data)
{
	gpio_set_level(RS, 0);
	gpio_set_level(E, 1);
	_delay(1);_delay(1);_delay(1);_delay(1);
//        ESP_LOGI(TAG, "10");
	gpio_set_level(D4, 0);
	gpio_set_level(D5, 0);
	gpio_set_level(D6, 0);
	gpio_set_level(D7, 0);

	gpio_set_level(D7, (data&(1<<7))>>7);
	gpio_set_level(D6, (data&(1<<6))>>6);
	gpio_set_level(D5, (data&(1<<5))>>5);
	gpio_set_level(D4, (data&(1<<4))>>4);

	gpio_set_level(E, 0);
	_delay(1);_delay(1);_delay(1);_delay(1);
//        ESP_LOGI(TAG, "5");
	gpio_set_level(E, 1);
	_delay(1);_delay(1);_delay(1);_delay(1);
//        ESP_LOGI(TAG, "5");
	gpio_set_level(D4, 0);
	gpio_set_level(D5, 0);
	gpio_set_level(D6, 0);
	gpio_set_level(D7, 0);

	gpio_set_level(D7, (data&(1<<3))>>3);
	gpio_set_level(D6, (data&(1<<2))>>2);
	gpio_set_level(D5, (data&(1<<1))>>1);
	gpio_set_level(D4, data&1);
	gpio_set_level(E, 0);
	_delay(1);_delay(1);_delay(1);_delay(1);
//        ESP_LOGI(TAG, "10");
}

static void lcd_sendstr (char* data)
{
	for(int i = 0; i < 16; i++)
	{
		if (!data[i]) break;
		lcd_send(data[i]);
	}
}

static void lcd_send (unsigned char data)
{
	gpio_set_level(RS, 1);
	gpio_set_level(E, 1);
	_delay(1);_delay(1);_delay(1);_delay(1);
//        ESP_LOGI(TAG, "5");
	gpio_set_level(D4, 0);
	gpio_set_level(D5, 0);
	gpio_set_level(D6, 0);
	gpio_set_level(D7, 0);

	gpio_set_level(D7, (data&(1<<7))>>7);
	gpio_set_level(D6, (data&(1<<6))>>6);
	gpio_set_level(D5, (data&(1<<5))>>5);
	gpio_set_level(D4, (data&(1<<4))>>4);

	gpio_set_level(E, 0);
	_delay(1);_delay(1);_delay(1);_delay(1);
//        ESP_LOGI(TAG, "5");
	gpio_set_level(E, 1);
	_delay(1);_delay(1);_delay(1);_delay(1);
//        ESP_LOGI(TAG, "5");
	gpio_set_level(D4, 0);
	gpio_set_level(D5, 0);
	gpio_set_level(D6, 0);
	gpio_set_level(D7, 0);

	gpio_set_level(D7, (data&(1<<3))>>3);
	gpio_set_level(D6, (data&(1<<2))>>2);
	gpio_set_level(D5, (data&(1<<1))>>1);
	gpio_set_level(D4, data&1);
	gpio_set_level(E, 0);
	_delay(1);_delay(1);_delay(1);_delay(1);
}

static void set_position (unsigned char stroka, unsigned char symb)
{
	command((0x40*stroka+symb)|0b10000000);
	_delay(1);_delay(1);_delay(1);_delay(1);
}

static void led_blink()
{
	gpio_set_level(LED, 0);
        ESP_LOGI(TAG, "Off");
	_delay(500);
	gpio_set_level(LED, 1);
        ESP_LOGI(TAG, "ON");
	_delay(500);
	gpio_set_level(LED, 0);
        ESP_LOGI(TAG, "OFF");
	_delay(500);
	gpio_set_level(LED, 1);
        ESP_LOGI(TAG, "ON");
	_delay(500);
	gpio_set_level(LED, 0);
        ESP_LOGI(TAG, "OFF");
	_delay(500);
	gpio_set_level(LED, 1);
        ESP_LOGI(TAG, "ON");
	_delay(500);
	gpio_set_level(LED, 0);
        ESP_LOGI(TAG, "Off");
}

static void lcd_intro()
{
	//set_position(0,0);
	lcd_send(' ');_delay(50);
	lcd_send('W');_delay(50);
	lcd_send('i');_delay(50);
	lcd_send('-');_delay(50);
	lcd_send('F');_delay(50);
	lcd_send('i');_delay(50);
	lcd_send(' ');_delay(50);
	lcd_send('s');_delay(50);
	lcd_send('e');_delay(50);
	lcd_send('n');_delay(50);
	lcd_send('s');_delay(50);
	lcd_send('o');_delay(50);
	lcd_send('r');_delay(50);
	lcd_send(' ');_delay(50);
	lcd_send(4);_delay(50);

	set_position(1,0);
	lcd_send('v');_delay(50);
	lcd_send('0');_delay(50);
	lcd_send('.');_delay(50);
	lcd_send('1');_delay(50);
	lcd_send(' ');_delay(50);
	lcd_send('(');_delay(50);
	lcd_send('1');_delay(50);
	lcd_send('1');_delay(50);
	lcd_send('.');_delay(50);
	lcd_send('0');_delay(50);
	lcd_send('5');_delay(50);
	lcd_send('.');_delay(50);
	lcd_send('2');_delay(50);
	lcd_send('3');_delay(50);
	lcd_send(')');_delay(1000);

//        ESP_LOGI(TAG, "ON-3");
//	vTaskDelay(3000);

	gpio_set_level(LED, 1);
//        ESP_LOGI(TAG, "ON");

}

uint8_t sign = 1;
uint8_t digit = 36;
uint16_t decimal = 60;
uint8_t sign_min = 1;
uint8_t min1 = 30;
uint8_t min2 = 50;
uint8_t sign_max = 1;
uint8_t max1 = 40;
uint8_t max2 = 6;

char array[17][15];
int array_max[17];
char rx_buffer[255];
int len = 0;


static void calculate_data()
{
//rx_buffer[]="Date: 2022-10-29 Time: 21:16:51 Voltage: 5100mV temp: +25.81C min: +25.06C max: +26.00C";
//len = 128;
//rx_buffer="Date: 2023-01-15 Time: 15:26:31 Voltage: 12860mV temp: +25.16C min: +24.88C max: +25.16C hum: 37.1% pres: 957.8hPa (718.0mmHg)";
//rx_buffer[127] = 0;
//	len = sizeof(rx_buffer);
	
	uint8_t second = rx_buffer[30]-48;
//	ESP_LOGI (TAG, "len: %d", len);
	int part_num = 0;
	int cut1 = 0;
	int cut2 = 0;
	for (int i = 0; i < len; i++)
	{
		if ((rx_buffer[i] != ' ')&&(i != len-1))
		{
			cut2++;
		}
		else
		{
			int samples = cut2-cut1+1;
			array_max[part_num] = samples - 1;
			int cut0 = 0;
			char temp_array[samples];
			while(cut0<samples-1)
			{
				temp_array[cut0] = rx_buffer[cut1];
				cut1++;
				cut0++;
			}
	//		char array[part_num][samples];

			for (int j=0; j<samples-1; j++)
			{
				array[part_num][j]=temp_array[j];
			}
			array[part_num][samples-1] = 0;
			cut2++;
			cut1 = cut2;
//			ESP_LOGI (TAG, "i: %d, part_num: %d, array: %s", i, part_num, (char*) array[part_num]);
//			ESP_LOGI (TAG, "i: %d, array2: %s", i, (char *) temp_char);
//			ESP_LOGI (TAG, "%d: %s (%d)", part_num, (char*) array[part_num], sizeof (array[part_num]) );
			part_num++;
		}
	}

	command(0b00000001);
        _delay(20);
	for (int i=0; i<17; i++)
	{
		switch (i)
		{
		case 3: //time
		        set_position(0,8);
			break;

		case 5: //voltage
		        set_position(1,0);
			break;

		case 7: //temp
		        set_position(0,0);
			break;

		case 9: //min
		        set_position(1,8);
			break;

		case 11: //max
		        set_position(0,8);
			break;

		case 13: //humidity
		        set_position(1,11);
			break;

		case 16: //pressure
	        	set_position(1,0);
			break;
		}

		if (((second < 5) && (i == 5 || i == 7)) || ((second >= 5) && (i == 3 || i == 7 || i == 13)))
		{
			for (int j=0; j<array_max[i]; j++)
			lcd_send((char) array[i][j]);
		}
		else if ((second >= 5) && (i == 16))
		{
			for (int j=1; j<10; j++)
			lcd_send((char) array[i][j]);
		}
		else if ((second < 5) && (i == 9))
		{
		        lcd_send(1);
			for (int j=0; j<array_max[i]; j++)
			lcd_send((char) array[i][j]);
		}
		else if ((second < 5) && (i == 11))
		{
		        lcd_send(0);
			for (int j=0; j<array_max[i]; j++)
			lcd_send((char) array[i][j]);
		}
	}
//	for (int j=0; j<i-1; j++)
//	{
//		ESP_LOGI (TAG, "%d: %s", j, (char*) array[j]);
//	}
}

static void tcp_client_task(void *pvParameters)
{
    char addr_str[128];
    int addr_family;
    int ip_protocol;

	while(1)
	{
        struct sockaddr_in destAddr;
        destAddr.sin_addr.s_addr = inet_addr(HOST_IP_ADDR);
        destAddr.sin_family = AF_INET;
        destAddr.sin_port = htons(PORT);
        addr_family = AF_INET;
        ip_protocol = IPPROTO_IP;
        inet_ntoa_r(destAddr.sin_addr, addr_str, sizeof(addr_str) - 1);

		int sock =  socket(addr_family, SOCK_STREAM, ip_protocol);
        if (sock < 0) {
            ESP_LOGE(TAG, "Unable to create socket: errno %d", errno);
            break;
        }
        ESP_LOGI(TAG, "Socket created");

        int err = connect(sock, (struct sockaddr *)&destAddr, sizeof(destAddr));
        if (err != 0) {
            ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
            close(sock);
            continue;
        }
        ESP_LOGI(TAG, "Successfully connected");

        while (1) {
  /*          int err = send(sock, payload, strlen(payload), 0);
            if (err < 0) {
                ESP_LOGE(TAG, "Error occured during sending: errno %d", errno);
                break;
            }*/

            len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
            // Error occured during receiving
            if (len < 0) {
                ESP_LOGE(TAG, "recv failed: errno %d", errno);
                break;
            }
	    else if ((len < 64) | (len > 250)) {
                ESP_LOGW(TAG, "incorrect packet size");
		continue;
		}
            // Data received
            else {
                rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
                ESP_LOGI(TAG, "Received %d bytes from %s:", len, addr_str);
                ESP_LOGI(TAG, "%s", (char *) rx_buffer);
		calculate_data();
            }

            vTaskDelay(100 / portTICK_PERIOD_MS);
        }

        if (sock != -1) {
            ESP_LOGE(TAG, "Shutting down socket and restarting...");
            shutdown(sock, 0);
            close(sock);
	}
    }
}

void app_main(void)
{
//    ESP_ERROR_CHECK(nvs_flash_init());
//    ESP_ERROR_CHECK(esp_netif_init());
//    ESP_ERROR_CHECK(esp_event_loop_create_default());

   // ESP_ERROR_CHECK(example_connect());

       lcd_init();
	led_blink();
    lcd_intro();

	command(0b00000001);
	_delay(10);

	set_position(0,0);
	char sendstr[17];
	strcpy(sendstr, "Wi-Fi scan... \0");
	lcd_sendstr(sendstr);
	wifi_scan();
	lcd_send('O');
	lcd_send('K');
	set_position(1,0);
	strcpy(sendstr, "Time sync...  \0");
	lcd_sendstr(sendstr);
	sntp_example_task();
	lcd_send('O');
	lcd_send('K');
	_delay(100);
	command(0b00000001);
	_delay(10);
	set_position(0,0);
	strcpy(sendstr, "Wi-Fi connect...");
	lcd_sendstr(sendstr);

	xSemaphore = xSemaphoreCreateBinary();
	if( xSemaphore == NULL ) ESP_LOGI(TAG, "Cannot create semaphore"); else ESP_LOGI(TAG, "Semaphore created"); 
	xTaskCreate(tcp_client_task, "tcp_client", 4096, NULL, 5, NULL);
//	xTaskCreate(tcp_server_task, "tcp_server", 4096, (void*)AF_INET, 5, NULL);
	lcd_send('O');
	lcd_send('K');
	set_position(1,0);
	strcpy(sendstr, "Sensor connect..");
	lcd_sendstr(sendstr);

	while(1)
	{
            vTaskDelay(100 / portTICK_PERIOD_MS);

//	vTaskDelay(10 / portTICK_RATE_MS);
//		vTaskDelay(10);
	}
	ESP_LOGI(TAG, "After xTask");
	//tcp_server_task((void*)AF_INET);
	//fclose(f);
}
