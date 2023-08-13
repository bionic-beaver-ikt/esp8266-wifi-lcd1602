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
#include "wifi_part.h"

#define RS 13
#define E 12
#define D4 14
#define D5 16
#define D6 4
#define D7 5
#define LED 2

#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<RS) | (1ULL<<E) | (1ULL<<LED))
#define GPIO_OUTPUT_PIN_SEL2 ((1ULL<<D4) | (1ULL<<D5) | (1ULL<<D6) | (1ULL<<D7))

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
