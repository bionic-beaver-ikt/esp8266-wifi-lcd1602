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
#include "wifi_part.h"
#include "lcd_part.h"

#define LED 2

static const char *TAG = "TEMP VIEWER";


SemaphoreHandle_t xSemaphore;

void _us_delay(uint32_t time_us)
{
//    ets_delay_us(time_us*1000);
    //    os_delay_us(time_us);
	vTaskDelay(time_us / portTICK_RATE_MS);
//	vTaskDelay(time_us);
}

static void led_blink()
{
	gpio_set_level(LED, 0);
        ESP_LOGI(TAG, "Off");
	vTaskDelay(300 / portTICK_PERIOD_MS);
	gpio_set_level(LED, 1);
        ESP_LOGI(TAG, "ON");
	vTaskDelay(300 / portTICK_PERIOD_MS);
	gpio_set_level(LED, 0);
        ESP_LOGI(TAG, "OFF");
	vTaskDelay(300 / portTICK_PERIOD_MS);
	gpio_set_level(LED, 1);
        ESP_LOGI(TAG, "ON");
	vTaskDelay(300 / portTICK_PERIOD_MS);
	gpio_set_level(LED, 0);
        ESP_LOGI(TAG, "OFF");
	vTaskDelay(300 / portTICK_PERIOD_MS);
	gpio_set_level(LED, 1);
        ESP_LOGI(TAG, "ON");
	vTaskDelay(300 / portTICK_PERIOD_MS);
	gpio_set_level(LED, 0);
        ESP_LOGI(TAG, "Off");
        vTaskDelay(300 / portTICK_PERIOD_MS);
	gpio_set_level(LED, 1);
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

void calculate_data(int len)
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
        vTaskDelay(25 / portTICK_PERIOD_MS);
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

void tcp_client_task(void *pvParameters)
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
	struct timeval to;
	to.tv_sec = 8;
        to.tv_usec = 0;
        int result_recv_timeout = setsockopt(sock,SOL_SOCKET,SO_RCVTIMEO,&to,sizeof(to));
	if (result_recv_timeout < 0)     
        {
        
            ESP_LOGI(TAG, "Unable to set read timeout on socket! %d", result_recv_timeout);
        }
	else
	ESP_LOGE(TAG, "SO_RCVTIMEO: ON");

        int err = connect(sock, (struct sockaddr *)&destAddr, sizeof(destAddr));
        if (err != 0) {
            ESP_LOGE(TAG, "Socket unable to connect: errno %d", errno);
            close(sock);
            continue;
        }
        ESP_LOGI(TAG, "Successfully connected");

        while (1) {
  	    /*int err = send(sock, payload, strlen(payload), 0);
            if (err < 0) {
                ESP_LOGE(TAG, "Error occured during sending: errno %d", errno);
                break;
            }*/

            int len = recv(sock, rx_buffer, sizeof(rx_buffer) - 1, 0);
            // Error occured during receiving
            if (len < 0) {
                ESP_LOGE(TAG, "recv failed: errno %d", errno);
                break;
            }
	    //else if ((len < 64) | (len > 250)) {
	    else if ((len < 120) | (len > 135)){
                ESP_LOGW(TAG, "incorrect packet size: %d", len);
                //vTaskDelay(2000 / portTICK_PERIOD_MS);
		continue;
		}
            // Data received
            else {
                rx_buffer[len] = 0; // Null-terminate whatever we received and treat like a string
                //ESP_LOGI(TAG, "Received %d bytes from %s:", len, addr_str);
                ESP_LOGI(TAG, "(%d) %s", len, (char *) rx_buffer);
		calculate_data(len);
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
gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL<<LED);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);
    
//    ESP_ERROR_CHECK(nvs_flash_init());
//    ESP_ERROR_CHECK(esp_netif_init());
//    ESP_ERROR_CHECK(esp_event_loop_create_default());

   // ESP_ERROR_CHECK(connect());

	lcd_init();
	led_blink();
	lcd_intro();

	command(0b00000001);
	vTaskDelay(20 / portTICK_PERIOD_MS);

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
	sntp_task();
	lcd_send('O');
	lcd_send('K');
	vTaskDelay(500 / portTICK_PERIOD_MS);
	command(0b00000001);
	vTaskDelay(20 / portTICK_PERIOD_MS);
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
