/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "esp_intr_alloc.h"
#include "usb/usb_host.h"
#include "driver/gpio.h"
#include "xbox.h"
#include "ble_main.h"
#include "i2c.h"
#include "espnow_example.h"

#define DAEMON_TASK_PRIORITY    4
#define CLASS_TASK_PRIORITY     5
#define XBOX_5V_EN              37

extern void class_driver_task(void *arg);

static const char *TAG = "DAEMON";

static void host_lib_daemon_task(void *arg)
{
    SemaphoreHandle_t signaling_sem = (SemaphoreHandle_t)arg;

    ESP_LOGI(TAG, "Installing USB Host Library");
    usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags = ESP_INTR_FLAG_LEVEL1,
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));

    //Signal to the class driver task that the host library is installed
    xSemaphoreGive(signaling_sem);
    vTaskDelay(10); //Short delay to let client task spin up

    bool has_clients = true;
    bool has_devices = true;
    while (1/*has_clients || has_devices */) {
        uint32_t event_flags=0;
        ESP_ERROR_CHECK(usb_host_lib_handle_events(portMAX_DELAY, &event_flags));
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            has_clients = false;
        }
        if (event_flags & USB_HOST_LIB_EVENT_FLAGS_ALL_FREE) {
            has_devices = false;
        }
    }
    ESP_LOGI(TAG, "No more clients and devices");

    //Uninstall the USB Host Library
    ESP_ERROR_CHECK(usb_host_uninstall());
    //Wait to be deleted
    xSemaphoreGive(signaling_sem);
    vTaskDelete(NULL);
    //vTaskSuspend(NULL);
}

void gpio_init(void)
{
    gpio_config_t conf = {
    .mode = GPIO_MODE_OUTPUT,
    .pin_bit_mask = XBOX_5V_EN,
    .pull_up_en = GPIO_PULLUP_DISABLE,
    .pull_down_en = GPIO_PULLDOWN_DISABLE,
    .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&conf);
}


void app_main(void)
{
    xTaskCreatePinnedToCore(IIC_MAIN,"iic_main",1024*3,NULL,1,NULL,1);
    gpio_reset_pin(XBOX_5V_EN);
    gpio_set_direction(XBOX_5V_EN,GPIO_MODE_OUTPUT);
    gpio_set_pull_mode(XBOX_5V_EN,GPIO_FLOATING);
    vTaskDelay(900/portTICK_PERIOD_MS);
    if(!CHECK_VBUS_EXIT()){
        SemaphoreHandle_t signaling_sem = xSemaphoreCreateBinary();

        TaskHandle_t daemon_task_hdl;
        TaskHandle_t class_driver_task_hdl;
        //Create daemon task
        xTaskCreatePinnedToCore(host_lib_daemon_task,
                                "daemon",
                                4096,
                                (void *)signaling_sem,
                                DAEMON_TASK_PRIORITY,
                                &daemon_task_hdl,
                                1);
        //Create the class driver task
        xTaskCreatePinnedToCore(class_driver_task,
                                "class",
                                4096,
                                (void *)signaling_sem,
                                CLASS_TASK_PRIORITY,
                                &class_driver_task_hdl,
                                1);
        //gpio_init();

        //ble_main();

        //ESP_LOGE("TESTING","PACK IS %d",sizeof(xbox_massage_t));
        // vTaskDelay(10000/portTICK_PERIOD_MS);     //Add a short delay to let the tasks run
        // while (1)
        // {
        //     vTaskDelay(100/portTICK_PERIOD_MS);
        //     esp_rom_gpio_connect_in_signal(GPIO_MATRIX_CONST_ONE_INPUT, 67, false);  //receiving a valid Vbus from host
        // }
        

        //Wait for the tasks to complete
        // for (int i = 0; i < 2; i++) {
        //     xSemaphoreTake(signaling_sem, portMAX_DELAY);
        // }

        //Delete the tasks
        // vTaskDelete(class_driver_task_hdl);
        //espnow_init();
        open_ldo3_3();
        gpio_set_level(37,1);
        vTaskDelete(NULL);
    }
    else
    {
        open_ldo3_3();
        gpio_set_level(XBOX_5V_EN,1);

    }

}
