#include "xbox.h"

uint8_t flag_connet=0;
uint8_t data_pre[15]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};

uint8_t xbox_hat(uint8_t hatdat)
{
    uint8_t ret=0;
    switch (hatdat)
    {
    case 1:
    ret = 1;
    break;
    case 2:
    ret = 5;
    break;
    case 4:
    ret = 7;
    break;
    case 8:
    ret = 3;
    break;
    case 9:
    ret = 2;
    break;
    case 10:
    ret = 4;
    break;
    case 6:
    ret = 6;
    break;
    case 5:
    ret = 8;
    break;
    default:
        break;
    }
    return ret;
}

esp_err_t xbox_pross(void *xbox_dat,uint8_t len)
{
    uint8_t tk=0;
    
    if(len!=20)
    {
        return ESP_FAIL;
    }
    xbox_massage_t* massgae_dat = (xbox_massage_t*)xbox_dat;
    if(memcmp(data_pre,(uint8_t*)&massgae_dat->data,15))
    {
        memcpy(data_pre,(uint8_t*)&massgae_dat->data,15);
        tk=massgae_dat->data.bttk&0x0f;
        massgae_dat->data.bttk&=0xf0;
        massgae_dat->data.bttk|=xbox_hat(tk);
        massgae_dat->data.xfang+=32768;
        massgae_dat->data.yfang=(~(massgae_dat->data.yfang))+32768;
        massgae_dat->data.xxuan+=32768;
        massgae_dat->data.yxuan+=32768;
        massgae_dat->data.unknown+=32768;
        massgae_dat->data.bttk=rol(massgae_dat->data.bttk,4);

        if(!ble_status_get()&&!flag_connet&&massgae_dat->data.bttk!=0x02)
        {
            if(massgae_dat->flage_xbox==0x1400);
            xTaskCreatePinnedToCore(ble_main,"ble task",1024*4,NULL,2,NULL,0);
            flag_connet=1;
            //ble_main();
        }
        else if(!get_esp_now()&&!flag_connet&&massgae_dat->data.bttk==0x02)
        {
            if(massgae_dat->flage_xbox==0x1400);
            xTaskCreatePinnedToCore(espnow_init,"esp_now task",1024*4,NULL,2,NULL,0);
            flag_connet=2;
            send_xbox_move(0x7f,0x7f,1000);
            // vTaskDelay(1000/portTICK_PERIOD_MS);
            // send_xbox_move(0x04);
        }
        switch(flag_connet)
        {
        case 1:
        if(ble_status_get())
        {esp_hidd_send_mouse_value(0,true,(void*)&massgae_dat->data);}
        break;
        case 2:
        if(get_esp_now()&&!get_status_esp_now())
        {esp_send((uint8_t*)&massgae_dat->data);}
        break;
        default:break;
        }
    }
    return ESP_OK;

}


//xbox_massage_t;







