#include <stdio.h>
#include <string.h>
#include <sdkconfig.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <esp_system.h>
#include <esp_err.h>
#include <esp_log.h>
#include <nvs_flash.h>
#include <nvs.h>
#include "hardware.h"
#include "managed_i2c.h"
#include "pax_gfx.h"
#include "sdcard.h"
#include "appfs.h"
#include "driver_framebuffer.h"

#include "rp2040.h"

#include "fpga_test.h"

#include "menu.h"
#include "button_wrapper.h"
#include "system_wrapper.h"
#include "graphics_wrapper.h"
#include "appfs_wrapper.h"
#include "settings.h"
#include "pax_keyboard.h"

static const char *TAG = "main";

typedef enum action {
    ACTION_NONE,
    ACTION_APPFS,
    ACTION_INSTALLER,
    ACTION_SETTINGS,
    ACTION_OTA,
    ACTION_FPGA,
    ACTION_WIFI_SCAN,
    ACTION_WIFI_MANUAL,
    ACTION_WIFI_LIST,
    ACTION_BACK
} menu_action_t;

typedef struct _menu_args {
    appfs_handle_t fd;
    menu_action_t action;
} menu_args_t;

void menu_launcher(xQueueHandle buttonQueue, pax_buf_t* pax_buffer, ILI9341* ili9341, uint8_t* framebuffer, menu_action_t* menu_action, appfs_handle_t* appfs_fd) {
    menu_t* menu = menu_alloc("Main menu");
    *appfs_fd = APPFS_INVALID_FD;
    *menu_action = ACTION_NONE;
    
    while (1) {
        *appfs_fd = appfsNextEntry(*appfs_fd);
        if (*appfs_fd == APPFS_INVALID_FD) break;
        const char* name = NULL;
        appfsEntryInfo(*appfs_fd, &name, NULL);
        menu_args_t* args = malloc(sizeof(menu_args_t));
        args->fd = *appfs_fd;
        args->action = ACTION_APPFS;
        menu_insert_item(menu, name, NULL, (void*) args, -1);
    }
    *appfs_fd = APPFS_INVALID_FD;

    menu_args_t* install_args = malloc(sizeof(menu_args_t));
    install_args->action = ACTION_INSTALLER;
    menu_insert_item(menu, "Hatchery", NULL, install_args, -1);
    
    menu_args_t* settings_args = malloc(sizeof(menu_args_t));
    settings_args->action = ACTION_SETTINGS;
    menu_insert_item(menu, "WiFi settings", NULL, settings_args, -1);
    
    menu_args_t* ota_args = malloc(sizeof(menu_args_t));
    ota_args->action = ACTION_OTA;
    menu_insert_item(menu, "Firmware update", NULL, ota_args, -1);

    menu_args_t* fpga_args = malloc(sizeof(menu_args_t));
    fpga_args->action = ACTION_FPGA;
    menu_insert_item(menu, "FPGA test", NULL, fpga_args, -1);

    bool render = true;
    menu_args_t* menuArgs = NULL;

    while (1) {
        button_message_t buttonMessage = {0};
        if (xQueueReceive(buttonQueue, &buttonMessage, 16 / portTICK_PERIOD_MS) == pdTRUE) {
            uint8_t pin = buttonMessage.button;
            bool value = buttonMessage.state;
            switch(pin) {
                case PCA9555_PIN_BTN_JOY_DOWN:
                    if (value) {
                        menu_navigate_next(menu);
                        render = true;
                    }
                    break;
                case PCA9555_PIN_BTN_JOY_UP:
                    if (value) {
                        menu_navigate_previous(menu);
                        render = true;
                    }
                    break;
                case PCA9555_PIN_BTN_ACCEPT:
                    if (value) {
                        menuArgs = menu_get_callback_args(menu, menu_get_position(menu));
                    }
                    break;
                default:
                    break;
            }
        }

        if (render) {
            graphics_task(pax_buffer, ili9341, framebuffer, menu, NULL);
            render = false;
        }
        
        if (menuArgs != NULL) {
            *appfs_fd = menuArgs->fd;
            *menu_action = menuArgs->action;
            break;
        }
    }
    
    for (size_t index = 0; index < menu_get_length(menu); index++) {
        free(menu_get_callback_args(menu, index));
    }
    
    menu_free(menu);
}

void menu_wifi_settings(xQueueHandle buttonQueue, pax_buf_t* pax_buffer, ILI9341* ili9341, uint8_t* framebuffer, menu_action_t* menu_action) {
    menu_t* menu = menu_alloc("WiFi settings");
    *menu_action = ACTION_NONE;

    menu_args_t* wifi_scan_args = malloc(sizeof(menu_args_t));
    wifi_scan_args->action = ACTION_WIFI_SCAN;
    menu_insert_item(menu, "Add by scan...", NULL, wifi_scan_args, -1);
    
    menu_args_t* wifi_manual_args = malloc(sizeof(menu_args_t));
    wifi_manual_args->action = ACTION_WIFI_MANUAL;
    menu_insert_item(menu, "Add manually...", NULL, wifi_manual_args, -1);
    
    menu_args_t* wifi_list_args = malloc(sizeof(menu_args_t));
    wifi_list_args->action = ACTION_WIFI_LIST;
    menu_insert_item(menu, "List known networks", NULL, wifi_list_args, -1);
    
    menu_args_t* back_args = malloc(sizeof(menu_args_t));
    back_args->action = ACTION_BACK;
    menu_insert_item(menu, "< Back", NULL, back_args, -1);

    bool render = true;
    menu_args_t* menuArgs = NULL;

    while (1) {
        button_message_t buttonMessage = {0};
        if (xQueueReceive(buttonQueue, &buttonMessage, 16 / portTICK_PERIOD_MS) == pdTRUE) {
            uint8_t pin = buttonMessage.button;
            bool value = buttonMessage.state;
            switch(pin) {
                case PCA9555_PIN_BTN_JOY_DOWN:
                    if (value) {
                        menu_navigate_next(menu);
                        render = true;
                    }
                    break;
                case PCA9555_PIN_BTN_JOY_UP:
                    if (value) {
                        menu_navigate_previous(menu);
                        render = true;
                    }
                    break;
                case PCA9555_PIN_BTN_ACCEPT:
                    if (value) {
                        menuArgs = menu_get_callback_args(menu, menu_get_position(menu));
                    }
                    break;
                default:
                    break;
            }
        }

        if (render) {
            graphics_task(pax_buffer, ili9341, framebuffer, menu, NULL);
            render = false;
        }
        
        if (menuArgs != NULL) {
            *menu_action = menuArgs->action;
            break;
        }
    }
    
    for (size_t index = 0; index < menu_get_length(menu); index++) {
        free(menu_get_callback_args(menu, index));
    }
    
    menu_free(menu);
}

bool keyboard(xQueueHandle buttonQueue, pax_buf_t* aBuffer, ILI9341* ili9341, uint8_t* framebuffer, float aPosX, float aPosY, float aWidth, float aHeight, const char* aTitle, const char* aHint, char* aOutput, size_t aOutputSize) {
    bool accepted = false;
    pkb_ctx_t kb_ctx;
    pkb_init(aBuffer, &kb_ctx, aOutput);
    
    pax_col_t fgColor = 0xFF000000;
    pax_col_t bgColor = 0xFFFFFFFF;
    pax_col_t shadowColor = 0xFFC0C3C8;
    pax_col_t borderColor = 0xFF0000AA;
    pax_col_t titleBgColor = 0xFF080764;
    pax_col_t titleColor = 0xFFFFFFFF;
    pax_col_t selColor = 0xff007fff;
    
    kb_ctx.text_col       = borderColor;
    kb_ctx.sel_text_col   = selColor;
    kb_ctx.sel_col        = selColor;
    kb_ctx.bg_col         = bgColor;
    
    kb_ctx.kb_font_size = 18;
    
    float titleHeight = 20;
    float hintHeight = 14;
    
    pax_noclip(aBuffer);
    pax_simple_rect(aBuffer, shadowColor, aPosX+5, aPosY+5, aWidth, aHeight);
    pax_simple_rect(aBuffer, bgColor, aPosX, aPosY, aWidth, aHeight);
    pax_outline_rect(aBuffer, borderColor, aPosX, aPosY, aWidth, aHeight);
    pax_simple_rect(aBuffer, titleBgColor, aPosX, aPosY, aWidth, titleHeight);
    pax_simple_line(aBuffer, titleColor, aPosX + 1, aPosY + titleHeight, aPosX + aWidth - 2, aPosY + titleHeight - 1);
    pax_clip(aBuffer, aPosX + 1, aPosY + 1, aWidth - 2, titleHeight - 2);
    pax_draw_text(aBuffer, titleColor, NULL, titleHeight - 2, aPosX + 1, aPosY + 1, aTitle);
    pax_clip(aBuffer, aPosX + 1, aPosY + aHeight - hintHeight, aWidth - 2, hintHeight);
    pax_draw_text(aBuffer, borderColor, NULL, hintHeight - 2, aPosX + 1, aPosY + aHeight - hintHeight, aHint);
    pax_noclip(aBuffer);

    kb_ctx.x = aPosX + 1;
    kb_ctx.y = aPosY + titleHeight + 1 ;
    kb_ctx.width = aWidth - 2;
    kb_ctx.height = aHeight - 3 - titleHeight - hintHeight;

    bool running = true;
    while (running) {
        button_message_t buttonMessage = {0};
        if (xQueueReceive(buttonQueue, &buttonMessage, 16 / portTICK_PERIOD_MS) == pdTRUE) {
            uint8_t pin = buttonMessage.button;
            bool value = buttonMessage.state;
            switch(pin) {
                case PCA9555_PIN_BTN_JOY_DOWN:
                    if (value) {
                        pkb_press(&kb_ctx, PKB_DOWN);
                    } else {
                        pkb_release(&kb_ctx, PKB_DOWN);
                    }
                    break;
                case PCA9555_PIN_BTN_JOY_UP:
                    if (value) {
                        pkb_press(&kb_ctx, PKB_UP);
                    } else {
                        pkb_release(&kb_ctx, PKB_UP);
                    }
                    break;
                case PCA9555_PIN_BTN_JOY_LEFT:
                    if (value) {
                        pkb_press(&kb_ctx, PKB_LEFT);
                    } else {
                        pkb_release(&kb_ctx, PKB_LEFT);
                    }
                    break;
                case PCA9555_PIN_BTN_JOY_RIGHT:
                    if (value) {
                        pkb_press(&kb_ctx, PKB_RIGHT);
                    } else {
                        pkb_release(&kb_ctx, PKB_RIGHT);
                    }
                    break;
                case PCA9555_PIN_BTN_JOY_PRESS:
                    if (value) {
                        pkb_press(&kb_ctx, PKB_SHIFT);
                    } else {
                        pkb_release(&kb_ctx, PKB_SHIFT);
                    }
                    break;
                case PCA9555_PIN_BTN_ACCEPT:
                    if (value) {
                        pkb_press(&kb_ctx, PKB_CHARSELECT);
                    } else {
                        pkb_release(&kb_ctx, PKB_CHARSELECT);
                    }
                    break;
                case PCA9555_PIN_BTN_BACK:
                    if (value) {
                        pkb_press(&kb_ctx, PKB_DELETE_BEFORE);
                    } else {
                        pkb_release(&kb_ctx, PKB_DELETE_BEFORE);
                    }
                    break;
                case PCA9555_PIN_BTN_SELECT:
                    if (value) {
                        pkb_press(&kb_ctx, PKB_MODESELECT);
                    } else {
                        pkb_release(&kb_ctx, PKB_MODESELECT);
                    }
                    break;
                case PCA9555_PIN_BTN_HOME:
                    if (value) {
                        running = false;
                    }
                    break;
                default:
                    break;
            }
        }
        pkb_loop(&kb_ctx);
        if (kb_ctx.dirty) {
            pkb_redraw(aBuffer, &kb_ctx);
            ili9341_write(ili9341, framebuffer);
        }
        if (kb_ctx.input_accepted) {
            memset(aOutput, 0, aOutputSize);
            strncpy(aOutput, kb_ctx.content, aOutputSize - 1);
            running = false;
            accepted = true;
        }
    }
    pkb_destroy(&kb_ctx);
    return accepted;
}

void app_main(void) {
    esp_err_t res;
    
    /* Initialize memory */
    
    xQueueHandle buttonQueue = xQueueCreate(10, sizeof(button_message_t));
    
    if (buttonQueue == NULL) {
        ESP_LOGE(TAG, "Failed to allocate queue");
        restart();
    }
    
    
    uint8_t* framebuffer = heap_caps_malloc(ILI9341_BUFFER_SIZE, MALLOC_CAP_8BIT);
    if (framebuffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate framebuffer");
        restart();
    }
    memset(framebuffer, 0, ILI9341_BUFFER_SIZE);
    
    pax_buf_t* pax_buffer = malloc(sizeof(pax_buf_t));
    if (framebuffer == NULL) {
        ESP_LOGE(TAG, "Failed to allocate pax buffer");
        restart();
    }
    memset(pax_buffer, 0, sizeof(pax_buf_t));
    
    pax_buf_init(pax_buffer, framebuffer, ILI9341_WIDTH, ILI9341_HEIGHT, PAX_BUF_16_565RGB);
    driver_framebuffer_init(framebuffer);
    
    /* Initialize hardware */
    
    res = board_init();
    
    if (res != ESP_OK) {
        printf("Failed to initialize hardware!\n");
        restart();
    }
    
    ILI9341* ili9341 = get_ili9341();
    ICE40* ice40 = get_ice40();
    BNO055* bno055 = get_bno055();
    RP2040* rp2040 = get_rp2040();
    PCA9555* pca9555 = get_pca9555();

    graphics_task(pax_buffer, ili9341, framebuffer, NULL, "Button init...");
    button_init(pca9555, buttonQueue);
    
    rp2040_set_led_mode(rp2040, true, true);
    
    graphics_task(pax_buffer, ili9341, framebuffer, NULL, "AppFS init...");
    res = appfs_init();
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "AppFS init failed: %d", res);
        graphics_task(pax_buffer, ili9341, framebuffer, NULL, "AppFS init failed!");
        return;
    }
    ESP_LOGI(TAG, "AppFS initialized");
    
    graphics_task(pax_buffer, ili9341, framebuffer, NULL, "NVS init...");
    res = nvs_init();
    if (res != ESP_OK) {
        ESP_LOGE(TAG, "NVS init failed: %d", res);
        graphics_task(pax_buffer, ili9341, framebuffer, NULL, "NVS init failed!");
        return;
    }
    ESP_LOGI(TAG, "NVS initialized");
    
    graphics_task(pax_buffer, ili9341, framebuffer, NULL, "Mount SD card...");
    res = mount_sd(SD_CMD, SD_CLK, SD_D0, SD_PWR, "/sd", false, 5);
    bool sdcard_ready = (res == ESP_OK);
  
    if (sdcard_ready) {
        graphics_task(pax_buffer, ili9341, framebuffer, NULL, "SD card mounted");
    }

    while (true) {
        menu_action_t menu_action;
        appfs_handle_t appfs_fd;
        menu_launcher(buttonQueue, pax_buffer, ili9341, framebuffer, &menu_action, &appfs_fd);
        if (menu_action == ACTION_APPFS) {
            appfs_boot_app(appfs_fd);
        } else if (menu_action == ACTION_FPGA) {
            graphics_task(pax_buffer, ili9341, framebuffer, NULL, "FPGA TEST");
            fpga_test(ili9341, ice40, buttonQueue);
        } else if (menu_action == ACTION_INSTALLER) {
            graphics_task(pax_buffer, ili9341, framebuffer, NULL, "INSTALLER");
            //appfs_store_app();
        } else if (menu_action == ACTION_OTA) {
            graphics_task(pax_buffer, ili9341, framebuffer, NULL, "Firmware update...");
        } else if (menu_action == ACTION_SETTINGS) {
            while (true) {
                menu_wifi_settings(buttonQueue, pax_buffer, ili9341, framebuffer, &menu_action);
                if (menu_action == ACTION_WIFI_MANUAL) {
                    nvs_handle_t handle;
                    nvs_open("system", NVS_READWRITE, &handle);
                    char ssid[33];
                    char password[33];
                    size_t requiredSize;
                    esp_err_t res = nvs_get_str(handle, "wifi.ssid", NULL, &requiredSize);
                    if (res != ESP_OK) {
                        strcpy(ssid, "");
                        strcpy(password, "");
                    } else if (requiredSize < sizeof(ssid)) {
                        res = nvs_get_str(handle, "wifi.ssid", ssid, &requiredSize);
                        if (res != ESP_OK) strcpy(ssid, "");
                        res = nvs_get_str(handle, "wifi.password", NULL, &requiredSize);
                        if (res != ESP_OK) {
                            strcpy(password, "");
                        } else if (requiredSize < sizeof(password)) {
                            res = nvs_get_str(handle, "wifi.password", password, &requiredSize);
                            if (res != ESP_OK) strcpy(password, "");
                        }
                    }
                    bool accepted = keyboard(buttonQueue, pax_buffer, ili9341, framebuffer, 30, 30, pax_buffer->width - 60, pax_buffer->height - 60, "WiFi SSID", "Press HOME to exit", ssid, sizeof(ssid));
                    if (accepted) {
                        accepted = keyboard(buttonQueue, pax_buffer, ili9341, framebuffer, 30, 30, pax_buffer->width - 60, pax_buffer->height - 60, "WiFi password", "Press HOME to exit", password, sizeof(password));
                    }
                    if (accepted) {
                        nvs_set_str(handle, "wifi.ssid", ssid);
                        nvs_set_str(handle, "wifi.password", password);
                        graphics_task(pax_buffer, ili9341, framebuffer, NULL, "WiFi settings stored");
                    } else {
                        graphics_task(pax_buffer, ili9341, framebuffer, NULL, "Canceled");
                    }
                    nvs_close(&handle);
                } else if (menu_action == ACTION_WIFI_LIST) {
                    nvs_handle_t handle;
                    nvs_open("system", NVS_READWRITE, &handle);
                    char ssid[33];
                    char password[33];
                    size_t requiredSize;
                    esp_err_t res = nvs_get_str(handle, "wifi.ssid", NULL, &requiredSize);
                    if (res != ESP_OK) {
                        strcpy(ssid, "");
                    } else if (requiredSize < sizeof(ssid)) {
                        res = nvs_get_str(handle, "wifi.ssid", ssid, &requiredSize);
                        if (res != ESP_OK) strcpy(ssid, "");
                        res = nvs_get_str(handle, "wifi.password", NULL, &requiredSize);
                        if (res != ESP_OK) {
                            strcpy(password, "");
                        } else if (requiredSize < sizeof(password)) {
                            res = nvs_get_str(handle, "wifi.password", password, &requiredSize);
                            if (res != ESP_OK) strcpy(password, "");
                        }
                    }
                    nvs_close(&handle);
                    char buffer[300];
                    snprintf(buffer, sizeof(buffer), "SSID is %s\nPassword is %s", ssid, password);
                    graphics_task(pax_buffer, ili9341, framebuffer, NULL, buffer);
                } else {
                    break;
                }
            }
        }
    }

    
    free(framebuffer);
}