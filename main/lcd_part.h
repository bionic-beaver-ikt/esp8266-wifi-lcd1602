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

#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<RS) | (1ULL<<E))
#define GPIO_OUTPUT_PIN_SEL2 ((1ULL<<D4) | (1ULL<<D5) | (1ULL<<D6) | (1ULL<<D7))

void command (unsigned char data);
void lcd_send (unsigned char data);
void lcd_sendstr (char* data);
void lcd_init ();
void set_position (unsigned char stroka, unsigned char symb);
void lcd_intro();
