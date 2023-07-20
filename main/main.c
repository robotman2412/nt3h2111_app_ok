/*
 * This example code is in the Public Domain (or CC0 licensed, at your option.)
 * Unless required by applicable law or agreed to in writing, this
 * software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
 * CONDITIONS OF ANY KIND, either express or implied.
 */

// This file contains a simple Hello World app which you can base you own
// native Badge apps on.

#include "main.h"
#include "nt3h2111.h"
#include "ndef.h"
#include "ndef_record_types.h"
#include "ui.h"

pax_buf_t buf;
NT3H2111 tag;
xQueueHandle buttonQueue;

#include <esp_log.h>
static const char *TAG = "mch2022-demo-app";

// Updates the screen with the latest buffer.
void disp_flush() {
	ili9341_write(get_ili9341(), buf.buf);
}

// Exits the app, returning to the launcher.
void exit_to_launcher() {
	REG_WRITE(RTC_CNTL_STORE0_REG, 0);
	esp_restart();
}

// Prints a simple hexdump.
static void hexdump(const void *datas, size_t size) {
	const size_t cols = 16;
	const uint8_t *arr = datas;
	for (size_t y = 0; y * cols < size; y++) {
		for (uint_fast8_t x = 0; x < cols && x+y*cols < size; x++) {
			if (x) putc(' ', stdout);
			printf("%02x", arr[y*cols+x]);
		}
		putc('\n', stdout);
	}
}

void app_main() {
	ESP_LOGI(TAG, "Welcome to the template app!");
	
	// Initialize the screen, the I2C and the SPI busses.
	bsp_init();
	
	// Initialize the RP2040 (responsible for buttons, etc).
	bsp_rp2040_init();
	
	// This queue is used to receive button presses.
	buttonQueue = get_rp2040()->queue;
	
	// Initialize graphics for the screen.
	pax_buf_init(&buf, NULL, 320, 240, PAX_BUF_16_565RGB);
	pax_enable_multicore(1);
	
	// Initialize NVS.
	nvs_flash_init();
	
	// Initialize WiFi. This doesn't connect to Wifi yet.
	wifi_init();
	
	// Init some NFC TAG.
	nt3h2111_init(&tag, 0, 0x55);
	
	while (1) {
		menu_show_summary();
		rp2040_input_message_t msg;
		if (xQueueReceive(buttonQueue, &msg, portMAX_DELAY) && msg.state && msg.input == RP2040_INPUT_BUTTON_HOME) {
			exit_to_launcher();
		}
	}
}
