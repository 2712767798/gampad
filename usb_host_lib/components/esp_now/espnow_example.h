/* ESPNOW Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

#ifndef ESPNOW_EXAMPLE_H
#define ESPNOW_EXAMPLE_H

#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <assert.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_now.h"
#include "esp_crc.h"
#include "xbox.h"
/* ESPNOW can work in both station and softap mode. It is configured in menuconfig. */
#if CONFIG_ESPNOW_WIFI_MODE_STATION
#define ESPNOW_WIFI_MODE WIFI_MODE_STA
#define ESPNOW_WIFI_IF ESP_IF_WIFI_STA
#else
#define ESPNOW_WIFI_MODE WIFI_MODE_AP
#define ESPNOW_WIFI_IF ESP_IF_WIFI_AP
#endif

#define ESPNOW_QUEUE_SIZE 6

#define IS_BROADCAST_ADDR(addr) (memcmp(addr, s_example_broadcast_mac, ESP_NOW_ETH_ALEN) == 0)

typedef enum
{
    EXAMPLE_ESPNOW_SEND_CB,
    EXAMPLE_ESPNOW_RECV_CB,
    EXAMPLE_ESPNOW_SEND_ED,//我自己写的
} example_espnow_event_id_t;

typedef struct
{
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    esp_now_send_status_t status;
} example_espnow_event_send_cb_t;

typedef struct
{
    uint8_t mac_addr[ESP_NOW_ETH_ALEN];
    uint8_t *data;
    int data_len;
} example_espnow_event_recv_cb_t;

typedef union
{
    example_espnow_event_send_cb_t send_cb;//发送的数据
    example_espnow_event_recv_cb_t recv_cb;//接收的数据
} example_espnow_event_info_t;

/* When ESPNOW sending or receiving callback function is called, post event to ESPNOW task.
当EspNow发送或接收回调函数被调用时，post事件到EspNow任务。＊*/
typedef struct
{
    example_espnow_event_id_t id;
    example_espnow_event_info_t info;
    uint8_t payload[15];
} example_espnow_event_t;

enum
{
    EXAMPLE_ESPNOW_DATA_BROADCAST,
    EXAMPLE_ESPNOW_DATA_UNICAST,
    EXAMPLE_ESPNOW_DATA_MAX,
};

/* User defined field of ESPNOW data in this example. */
typedef struct
{
    uint8_t flag_who;                             //设备的归属
    uint8_t type;                                // Broadcast or unicast ESPNOW data.
    uint8_t state;                               // Indicate that if has received broadcast ESPNOW data or not.
    uint16_t seq_num;                            // Sequence number of ESPNOW data.// ESPNOW数据的序列号。
    uint16_t crc;                                // CRC16 value of ESPNOW data.ESPNOW数据的CRC16值。
    uint32_t magic;                              // Magic number which is used to determine which device to send unicast ESPNOW data.
    uint8_t payload[15];                          // Real payload of ESPNOW data.
} __attribute__((packed)) example_espnow_data_t; // 发送的真正的数据

/* Parameters of sending ESPNOW data. */
typedef struct
{
    bool unicast;   // Send unicast ESPNOW data.
    bool broadcast; // Send broadcast ESPNOW data.
    uint8_t state;  // Indicate that if has received broadcast ESPNOW data or not.//是否已经接收的广播的数据，用于广播时候识别用
    uint32_t magic; // Magic number which is used to determine which device to send unicast ESPNOW data.
    ////用于确定哪个设备发送单播ESPNOW数据的魔法数字。
    uint16_t count;                     // Total count of unicast ESPNOW data to be sent.
    uint16_t delay;                     // Delay between sending two ESPNOW data, unit: ms.两个设备发送的延迟
    int len;                            // Length of ESPNOW data to be sent, unit: byte.
    uint8_t *buffer;                    // Buffer pointing to ESPNOW data.
    uint8_t dest_mac[ESP_NOW_ETH_ALEN]; // MAC address of destination device.目的设备MAC地址。
} example_espnow_send_param_t;          // 发送前的数据

void espnow_init(void);
void esp_send(uint8_t *dat);
void esp_now_all_deinit(void);
void wifi_end(void);
bool get_esp_now(void);
bool get_status_esp_now(void);

#endif
