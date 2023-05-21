// Copyright 2017-2018 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef XBOX_H
#define XBOX_H



#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>
#include <stdlib.h>
#include "esp_log.h"
#include "esp_err.h"
#include "esp_hidd_prf_api.h"
#include "ble_main.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "espnow_example.h"


struct xbox_massage_s{
    uint16_t flage_xbox;              /**< xbox标志位*/
    struct{    
        uint8_t bttk;                         /**< 头盔和四个按键*/
        uint8_t btabxy;                        /**< abxy和lbrb和logo*/
        //uint16_t buttun;                   /**< 十个按钮位*/
        int16_t unknown;                    /**< 猜测是z轴位 */
        int16_t xfang;                    /**< x轴，前两位符号位，-32768~32767*/
        int16_t yfang;                   /**< y轴，前两位符号位，-32768~32767*/
        int16_t xxuan;                    /**< x轴旋，前两位符号位，-32768~32767*/
        int16_t yxuan;                   /**< y轴旋，前两位符号位，-32768~32767*/
    }data;
    uint16_t zero;                    /**< 应该是恒为零*/
    uint16_t zer02;
    uint16_t zer03;
}__attribute__((packed));

typedef struct xbox_massage_s xbox_massage_t;


#define LB           0x0001
#define RB           0x0002
#define LOGO         0x0004


//按钮
#define BTA          0x0010
#define BTB          0x0020
#define BTX          0x0040
#define BTY          0x0080


//头盔
#define headup       0x0100
#define headdown     0x0200
#define headleft     0x0400
#define headright    0x0800

//
#define BTSTART      0x1000
#define BTBACK       0x2000
#define leftjy       0x4000
#define rightjy      0x8000


// value循环左移bits位
#define rol(value, bits) ((value << bits) | (value >> (sizeof(value)*8 - bits)))

// value循环右移bits位
#define ror(value, bits) ((value >> bits) | (value << (sizeof(value)*8 - bits)))


esp_err_t xbox_pross(void *xbox_dat,uint8_t len);
void send_xbox_move(uint8_t r,uint8_t l,uint32_t time);

#ifdef __cplusplus
} // extern "C"
#endif

#endif /* HID_DEV_H__ */
