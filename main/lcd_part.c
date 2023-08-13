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
#include "lcd_part.h"

static const char *TAG = "LCD1602A PART";

static void _delay(uint32_t time_ms)
{
//    ets_delay_us(time_us*1000);
    //    os_delay_us(time_us);
	vTaskDelay(time_ms / portTICK_PERIOD_MS);
	//vTaskDelay(time_us);
}


void lcd_init()
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

void command (unsigned char data)
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

void lcd_sendstr (char* data)
{
	for(int i = 0; i < 16; i++)
	{
		if (!data[i]) break;
		lcd_send(data[i]);
	}
}

void lcd_send (unsigned char data)
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

void set_position (unsigned char stroka, unsigned char symb)
{
	command((0x40*stroka+symb)|0b10000000);
	_delay(1);_delay(1);_delay(1);_delay(1);
}

void lcd_intro()
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

}
