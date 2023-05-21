/* i2c - Simple example

   Simple I2C example that shows how to initialize I2C
   as well as reading and writing from and to registers for a sensor connected over I2C.

   The sensor used in this example is a MPU9250 inertial measurement unit.

   For other examples please check:
   https://github.com/espressif/esp-idf/tree/master/examples

   See README.md file to get detailed usage of this example.

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
/* i2c—简单示例I2C的简单示例，演示如何初始化I2C以及对通过I2C连接的传感器的寄存器进行读写。
本例中使用的传感器是MPU9250惯性测量单元。其他例子请查看:
https://github.com/espressif/esp-idf/tree/master/examples参见README
。Md文件来获得这个例子的详细用法。此示例代码位于Public Domain(或根据您的选择获得cce许可)中。
除非适用法律要求或书面同意，否则软件是在“现状”的基础上发布的，没有任何形式的保证或条件，无论是明示的还是暗示的。*/
#include <stdio.h>
#include "esp_log.h"
#include "driver/i2c.h"
#include "driver/gpio.h"

static const char *TAG = "i2c-simple-example";

#define I2C_MASTER_SCL_IO           5      /*!< GPIO number used for I2C master clock */
#define I2C_MASTER_SDA_IO           8     /*!< GPIO number used for I2C master data  */
#define I2C_MASTER_NUM              0                          /*!< I2C master i2c port number, the number of i2c peripheral interfaces available will depend on the chip */
                                                               // I2C主I2C端口号，可用的I2C外设接口数量将取决于芯片
#define I2C_MASTER_FREQ_HZ          10000                     /*!< I2C master clock frequency */
#define I2C_MASTER_TX_BUF_DISABLE   0                          /*!< I2C master doesn't need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0                          /*!< I2C master doesn't need buffer */
#define I2C_MASTER_TIMEOUT_MS       2000

#define AXP_IRQ1_7                  0x44               /*主要有vubs插入事件 */
#define AXP_IRQ8_15                 0x45               /*主要有电池插入和正在充电事件*/
#define AXP_IRQ16_23                0x46               /*电流显示，过低等*/
//剩下没啥用

#define AXP_IRQ1                    0x40               /*IQ1使能 */
#define AXP_IRQ2                    0x41               /*IQ2使能*/
#define AXP_IRQ3                    0x42               /*IQ3使能*/
#define AXP_IRQ4                    0x43               /*IQ5使能 */

#define AXP_DCDC1OPEN_              0x12               /*电源输出控制*/


#define AXP_VBUS_IN_MASK               0X01<<3         //VBUS插入1-7
#define AXP_VBUS_OUT_MASK              0X01<<2         //VBUS拔除
#define AXP_BAT_CHARG_MASK             0X01<<3         //VBUS拔除8-15
#define AXP_BAT_FINISH_MASK            0X01<<2         //VBUS拔除

#define AXP_VBUS_EXIT_MASK             0X01<<5         //VBUS存在，在0x00reg中
#define AXP_VBUS_CAN_MASK              0X01<<4         //VBUS拔除

//open
#define AXP_DCDC1_MASK                 0x01<<0               //DCDC1位置0x12中
#define AXP_LDO4_MASK                  0x01<<1               //LDO4
#define AXP_LDO2_MASK                  0x01<<2               //LDO2
#define AXP_LDO3_MASK                  0x01<<3               //LDO3
#define AXP_DCDC2_MASK                 0x01<<4               //DCDC2

//set level
#define AXP_DCDC1_REG                  0x26               //DCDC1 3.5vmax 25mv/step
#define AXP_LDO4_REG                   0x27               //LDO4
#define AXP_LDO2_3_REG                 0x28               //LDO2_3 0-3ldo3 3.3vmax 100mv/step
#define AXP_DCDC2_REG                  0x23               //DCDC2
#define AXP_VBUS__REG                  0x00               //VBUS和ACIN寄存器

//bat level
#define AXP_BAT_LAVEL                  0x78               //bat

#define AXP173                      0x34            //写地址

/**
 * @brief Read a sequence of bytes from a MPU9250 sensor registers
 */
esp_err_t iic_read(uint8_t reg_addr, uint8_t *data, size_t len)
{
    return i2c_master_write_read_device(I2C_MASTER_NUM, AXP173, &reg_addr, 1, data, len, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
}

/**
 * @brief Write a byte to a iic sensor register
 */
esp_err_t iic_write_byte(uint8_t reg_addr, uint8_t data)
{
    int ret;
    uint8_t write_buf[2];
    write_buf[0]=reg_addr;
    write_buf[1]=data;
    ret = i2c_master_write_to_device(I2C_MASTER_NUM, AXP173,write_buf,sizeof(write_buf),I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    return ret;
}

/**
 * @brief i2c master initialization    i2c主初始化
 */
esp_err_t i2c_master_init(void)
{
    int i2c_master_port = I2C_MASTER_NUM;

    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };

    i2c_param_config(i2c_master_port, &conf);

    return i2c_driver_install(i2c_master_port, conf.mode, I2C_MASTER_RX_BUF_DISABLE, I2C_MASTER_TX_BUF_DISABLE, 0);
}



bool CHECK_VBUS_EXIT(void)
{
    esp_err_t er;
    uint8_t data=0;
    er=iic_read(AXP_VBUS__REG,&data,1);
    if(er==ESP_FAIL)
    {
        ESP_LOGI("IICTAG","no_ack,%d",er);
    }
    if((data&(AXP_VBUS_EXIT_MASK|AXP_VBUS_CAN_MASK)))
    return true;
    else
    return false;
}

void AXP_INIT()
{
    esp_err_t er;
    er=iic_write_byte(AXP_IRQ1,0x00|AXP_VBUS_OUT_MASK|AXP_VBUS_IN_MASK);
    if(er==ESP_FAIL)
    {
        ESP_LOGE("IICTAG","no_ack,%d",er);
    }
    er=iic_write_byte(AXP_IRQ1,0x00|AXP_BAT_CHARG_MASK|AXP_BAT_FINISH_MASK);
    if(er==ESP_FAIL)
    {
        ESP_LOGE("IICTAG","no_ack,%d",er);
    }
    er=iic_write_byte(AXP_IRQ1,0x00);
    if(er==ESP_FAIL)
    {
        ESP_LOGE("IICTAG","no_ack,%d",er);
    }
    er=iic_write_byte(AXP_IRQ1,0x00);
    if(er==ESP_FAIL)
    {
        ESP_LOGE("IICTAG","no_ack,%d",er);
    }

}

void open_ldo3_3(void)
{
    esp_err_t er;
    uint8_t data;
    iic_read(AXP_DCDC1OPEN_,&data,1);
    ESP_LOGI("IICTAG","AXP OPEN,%d",data);
    er=iic_write_byte(AXP_DCDC1OPEN_,/*0X00|AXP_LDO3_MASK|AXP_DCDC1_MASK|AXP_LDO4_MASK*/0x0b);
    iic_read(AXP_DCDC1OPEN_,&data,1);
    ESP_LOGI("IICTAG","AXP OPEN,%d",data);
    er=iic_write_byte(AXP_LDO2_3_REG,0XFF);
    if(er==ESP_FAIL)
    {
        ESP_LOGE("IICTAG","no_ack,%d",er);
    }
}


void close_ldo3_3(void)
{
    esp_err_t er;
    uint8_t data;
    iic_read(AXP_DCDC1OPEN_,&data,1);
    ESP_LOGI("IICTAG","AXP OPEN,%d",data);
    er=iic_write_byte(AXP_DCDC1OPEN_,/*0X00|AXP_LDO3_MASK|AXP_DCDC1_MASK|AXP_LDO4_MASK*/0x0b);
    iic_read(AXP_DCDC1OPEN_,&data,1);
    ESP_LOGI("IICTAG","AXP OPEN,%d",data);
    er=iic_write_byte(AXP_LDO2_3_REG,0X00);
    if(er==ESP_FAIL)
    {
        ESP_LOGE("IICTAG","no_ack,%d",er);
    }
}

float AXP_BAT_CHECK(void)
{
    float ADCLSB = 1.1 / 1000.0;
    uint8_t bat_data[2];
    //     uint8_t buff[2];
    // _I2C_readBuff(addr, 2, buff);
    // return (buff[0] << 4) + buff[1];
    if(iic_read(AXP_BAT_LAVEL,&bat_data,2)!=ESP_OK)
    {
        ESP_LOGE("IIC","NONE READ");
        return 0;
    }
    return ((bat_data[0]<<4)+bat_data[1]) * ADCLSB;
}

void IIC_MAIN(void)
{
    uint8_t val=0;
    ESP_ERROR_CHECK(i2c_master_init());
    ESP_LOGI(TAG, "I2C initialized successfully");
    AXP_INIT();
    //open_ldo3_3();
    //iic_read(0X26,&val,1);
    //i2c_master_write_read_device(I2C_MASTER_NUM, AXP173, &csxy, 1,&val, 1, I2C_MASTER_TIMEOUT_MS / portTICK_PERIOD_MS);
    while(1)
    {
    vTaskDelay(1000/portTICK_PERIOD_MS);
    //ESP_LOGI(TAG, "bat level is %x",AXP_BAT_CHECK());
    //ESP_LOGI(TAG, "vbus is %d",CHECK_VBUS_EXIT());
    }

}
