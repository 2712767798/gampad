/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "usb/usb_host.h"
#include <string.h>
#include "esp_timer.h"
#include "driver/gpio.h"
#include "i2c.h"
#include "xbox.h"

uint8_t flag_move=3;
uint8_t move_code[]={0x00,0x08,0x00,0x40,0x40,0x00,0x00,0x01};
static esp_timer_handle_t lv_timer_ti = NULL; // //先建立一个例子句柄

#define CLIENT_NUM_EVENT_MSG        5

// #define CLASS_DRIVER_ACTION_OPEN_DEV    0x01
// #define CLASS_DRIVER_ACTION_TRANSFER    0x02
// #define CLASS_DRIVER_ACTION_CLOSE_DEV   0x03

#define ACTION_OPEN_DEV             0x01
#define ACTION_GET_DEV_INFO         0x02
#define ACTION_GET_DEV_DESC         0x04
#define ACTION_GET_CONFIG_DESC      0x08
#define ACTION_GET_STR_DESC         0x10
#define ACTION_CLOSE_DEV            0x20
#define ACTION_EXIT                 0x40
#define ACTION_TRANSFER             0x80


typedef struct {
    usb_host_client_handle_t client_hdl;
    uint8_t dev_addr;
    usb_device_handle_t dev_hdl;
    uint32_t actions;
} class_driver_t;

struct class_driver_control {
    uint32_t actions;
    uint8_t dev_addr;
    usb_host_client_handle_t client_hdl;
    usb_device_handle_t dev_hdl;
};


static const char *TAG = "CLASS";

static void lv_timer_back(void *arg)
{
    flag_move=4;
}

//创建注册timer
void lv_ti_init(void)
{
    esp_timer_create_args_t lv_timer_arg = {
        //建立初始化句柄的实例
        .callback = lv_timer_back,
        .name = "lv_timer_arg",
    };
    esp_err_t err = esp_timer_create(&lv_timer_arg, &lv_timer_ti);
    //err = esp_timer_start_periodic(lv_timer_ti, 1000); // 1000us调用一次回调函数
    if (err == ESP_OK)
    {
        ESP_LOGI("main", "lvgl定时器创建成功\r\n");
    }
}

void send_xbox_move(uint8_t r,uint8_t l,uint32_t time)
{
    flag_move=0;
    move_code[3]=l;
    move_code[4]=r;
    esp_timer_start_once(lv_timer_ti,time*1000);//ms级别
}

static void client_event_cb(const usb_host_client_event_msg_t *event_msg, void *arg)
{
    class_driver_t *driver_obj = (class_driver_t *)arg;
    switch (event_msg->event) {
        case USB_HOST_CLIENT_EVENT_NEW_DEV:
            if (driver_obj->dev_addr == 0) {
                driver_obj->dev_addr = event_msg->new_dev.address;
                //Open the device next
                driver_obj->actions |= ACTION_OPEN_DEV;
            }
            break;
        case USB_HOST_CLIENT_EVENT_DEV_GONE:
            if (driver_obj->dev_hdl != NULL) {
                //Cancel any other actions and close the device next
                driver_obj->actions = ACTION_CLOSE_DEV;
            }
            break;
        default:
            //Should never occur
            abort();
    }
}



void transfer_cb(usb_transfer_t *transfer)
{
    //This is function is called from within usb_host_client_handle_events(). Don't block and try to keep it short
    class_driver_t *class_driver_obj = (class_driver_t *)transfer->context;
    //ESP_LOG_BUFFER_HEX("DATA",transfer->data_buffer,transfer->actual_num_bytes);
    xbox_pross((void *)transfer->data_buffer,transfer->actual_num_bytes);
    assert(transfer);
    if(!flag_move)
    {
        memcpy(transfer->data_buffer,move_code,8);
        transfer->bEndpointAddress = 0x02;
        transfer->num_bytes = 8;
        flag_move=1;
    }
    else if(flag_move==4)
    {
        memcpy(transfer->data_buffer,move_code,8);
        transfer->data_buffer[3]=0x00;
        transfer->data_buffer[4]=0x00;
        transfer->bEndpointAddress = 0x02;
        transfer->num_bytes = 8;
        flag_move=1;
    }
    else if(flag_move==1)
    {
        transfer->bEndpointAddress = 0x81;
        transfer->num_bytes = 32;
        flag_move=3;
    }
    usb_host_transfer_submit(transfer);

    //printf("Transfer status %d, actual number of bytes transferred %d\n", transfer->status, transfer->actual_num_bytes);
    //class_driver_obj->actions |= ACTION_TRANSFER;
    //class_driver_obj->actions &= ~ACTION_OPEN_DEV;
}

static void action_open_dev(class_driver_t *driver_obj)
{
    assert(driver_obj->dev_addr != 0);
    ESP_LOGI(TAG, "Opening device at address %d", driver_obj->dev_addr);
    ESP_ERROR_CHECK(usb_host_device_open(driver_obj->client_hdl, driver_obj->dev_addr, &driver_obj->dev_hdl));
    //Get the device's information next
    driver_obj->actions &= ~ACTION_OPEN_DEV;
    driver_obj->actions |= ACTION_GET_DEV_INFO;
}

static void action_get_info(class_driver_t *driver_obj)
{
    assert(driver_obj->dev_hdl != NULL);
    ESP_LOGI(TAG, "Getting device information");
    usb_device_info_t dev_info;
    ESP_ERROR_CHECK(usb_host_device_info(driver_obj->dev_hdl, &dev_info));
    ESP_LOGI(TAG, "\t%s speed", (dev_info.speed == USB_SPEED_LOW) ? "Low" : "Full");
    ESP_LOGI(TAG, "\tbConfigurationValue %d", dev_info.bConfigurationValue);
    //Todo: Print string descriptors

    //Get the device descriptor next
    driver_obj->actions &= ~ACTION_GET_DEV_INFO;
    driver_obj->actions |= ACTION_GET_DEV_DESC;
}

static void action_get_dev_desc(class_driver_t *driver_obj)
{
    assert(driver_obj->dev_hdl != NULL);
    ESP_LOGI(TAG, "Getting device descriptor");
    const usb_device_desc_t *dev_desc;
    ESP_ERROR_CHECK(usb_host_get_device_descriptor(driver_obj->dev_hdl, &dev_desc));
    usb_print_device_descriptor(dev_desc);
    //Get the device's config descriptor next
    driver_obj->actions &= ~ACTION_GET_DEV_DESC;
    driver_obj->actions |= ACTION_GET_CONFIG_DESC;
}

static void action_get_config_desc(class_driver_t *driver_obj)
{
    assert(driver_obj->dev_hdl != NULL);
    ESP_LOGI(TAG, "Getting config descriptor");
    const usb_config_desc_t *config_desc;
    ESP_ERROR_CHECK(usb_host_get_active_config_descriptor(driver_obj->dev_hdl, &config_desc));
    usb_print_config_descriptor(config_desc, NULL);
    //Get the device's string descriptors next
    driver_obj->actions &= ~ACTION_GET_CONFIG_DESC;
    driver_obj->actions |= ACTION_GET_STR_DESC;
}

static void action_get_str_desc(class_driver_t *driver_obj)
{
    assert(driver_obj->dev_hdl != NULL);
    usb_device_info_t dev_info;
    ESP_ERROR_CHECK(usb_host_device_info(driver_obj->dev_hdl, &dev_info));
    if (dev_info.str_desc_manufacturer) {
        ESP_LOGI(TAG, "Getting Manufacturer string descriptor");
        usb_print_string_descriptor(dev_info.str_desc_manufacturer);
    }
    if (dev_info.str_desc_product) {
        ESP_LOGI(TAG, "Getting Product string descriptor");
        usb_print_string_descriptor(dev_info.str_desc_product);
    }
    if (dev_info.str_desc_serial_num) {
        ESP_LOGI(TAG, "Getting Serial Number string descriptor");
        usb_print_string_descriptor(dev_info.str_desc_serial_num);
    }
    //Nothing to do until the device disconnects
    driver_obj->actions &= ~ACTION_GET_STR_DESC;
    driver_obj->actions |= ACTION_TRANSFER;
}

static void aciton_close_dev(class_driver_t *driver_obj)
{
    ESP_ERROR_CHECK(usb_host_device_close(driver_obj->client_hdl, driver_obj->dev_hdl));
    driver_obj->dev_hdl = NULL;
    driver_obj->dev_addr = 0;
    //We need to exit the event handler loop
    driver_obj->actions &= ~ACTION_CLOSE_DEV;
    driver_obj->actions |= ACTION_EXIT;
}



void class_driver_task(void *arg)
{

    SemaphoreHandle_t signaling_sem = (SemaphoreHandle_t)arg;
    class_driver_t driver_obj = {0};
    uint8_t tt=0;

    //Wait until daemon task has installed USB Host Library
    xSemaphoreTake(signaling_sem, portMAX_DELAY);

    ESP_LOGI(TAG, "Registering Client");
    usb_host_client_config_t client_config = {
        .is_synchronous = false,    //Synchronous clients currently not supported. Set this to false
        .max_num_event_msg = CLIENT_NUM_EVENT_MSG,
        .async = {
            .client_event_callback = client_event_cb,
            .callback_arg = (void *)&driver_obj,
        },
    };
    ESP_ERROR_CHECK(usb_host_client_register(&client_config, &driver_obj.client_hdl));


    usb_transfer_t *transfer;
    esp_err_t a;
    uint8_t aa[]={0xc1,0x01,0x00,0x00,0x00,0x00,0x14,0x00};
    usb_host_transfer_alloc(32, 0, &transfer);
    lv_ti_init();
    while (1) {
         if (driver_obj.actions == 0) {
            //ESP_LOGI("CLASS","TESTING");
            usb_host_client_handle_events(driver_obj.client_hdl, 0xffffff);
         } else {
            if (driver_obj.actions & ACTION_OPEN_DEV) {
                action_open_dev(&driver_obj);
                a=usb_host_interface_claim(driver_obj.client_hdl, driver_obj.dev_hdl, 0, 0);
                if(a==ESP_OK)
                ESP_LOGI("CLAIM","ok");
                else
                ESP_LOGI("CLAIM","no");
                //usb_host_interface_claim(driver_obj.client_hdl, driver_obj.dev_hdl, 0X82, 0);
            }
            if (driver_obj.actions & ACTION_GET_DEV_INFO) {
                action_get_info(&driver_obj);
            }
            if (driver_obj.actions & ACTION_GET_DEV_DESC) {
                action_get_dev_desc(&driver_obj);
            }
            if (driver_obj.actions & ACTION_GET_CONFIG_DESC) {
                action_get_config_desc(&driver_obj);
            }
            if (driver_obj.actions & ACTION_GET_STR_DESC) {
                action_get_str_desc(&driver_obj);
            }
            if (driver_obj.actions & ACTION_CLOSE_DEV) {
                usb_host_interface_release(driver_obj.client_hdl, driver_obj.dev_hdl, 0);
                aciton_close_dev(&driver_obj);
    
               //usb_host_interface_release(driver_obj.client_hdl, driver_obj.dev_hdl, 0X81);
            }
            if (driver_obj.actions & ACTION_EXIT) {
                driver_obj.actions&=~ACTION_EXIT;
            }
            if(driver_obj.actions&ACTION_TRANSFER){
                //Send an OUT transfer to EP1
                //memset(transfer->data_buffer, 0xAA, 32);
                transfer->num_bytes = 32;
                transfer->device_handle = driver_obj.dev_hdl;
                transfer->bEndpointAddress = 0x81;
                transfer->callback = transfer_cb;
                transfer->context = &driver_obj;
                transfer->timeout_ms=5000;
                usb_host_transfer_submit(transfer);
                driver_obj.actions&=~ACTION_TRANSFER;
                ESP_LOGI("TRANS","TRANSFER");
                vTaskDelay(1);
                

                //xTaskCreate(USB_TES,"usb_task",1024*3,(void *)transfer,3,NULL);
                //recive an in transfer to ep1
                // memset(transfer->data_buffer, 0x00, 32);
                // transfer->num_bytes = 32;
                // transfer->device_handle = driver_obj.dev_hdl;
                // transfer->bEndpointAddress = 0x81;
                // transfer->callback = transfer_cb;
                // transfer->context = (void *)&driver_obj;
                // usb_host_transfer_submit(transfer);
                // ESP_LOG_BUFFER_HEX("USB",transfer->data_buffer,transfer->num_bytes);
                //driver_obj.actions&=~ACTION_TRANSFER;
                // for(tt=0;tt<transfer->num_bytes;tt++)
                // {
                //     ESP_LOG_BUFFER_HEX
                // }
             }
        }
        //        USB_TES(transfer);
        // memset(transfer->data_buffer, 0xAA, 5);
        // transfer->num_bytes = 5;
        // transfer->device_handle = driver_obj.dev_hdl;
        // transfer->bEndpointAddress = 0x81;
        // transfer->callback = transfer_cb;
        // transfer->context = (void *)&driver_obj;
        // usb_host_transfer_submit(transfer);
    }

    ESP_LOGI(TAG, "Deregistering Client");
    usb_host_transfer_free(transfer);
    ESP_ERROR_CHECK(usb_host_client_deregister(driver_obj.client_hdl));

    //Wait to be deleted
    xSemaphoreGive(signaling_sem);
    vTaskDelete(NULL);
    //vTaskSuspend(NULL);
}




