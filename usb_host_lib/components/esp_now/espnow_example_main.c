/* ESPNOW Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/

/*
   This example shows how to use ESPNOW.
   Prepare two device, one for sending ESPNOW data and another for receiving
   ESPNOW data.
*/

#include "espnow_example.h"

#define ESPNOW_MAXDELAY 512

static const char *TAG = "espnow_example";

static QueueHandle_t s_example_espnow_queue;

static uint8_t s_example_broadcast_mac[ESP_NOW_ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // 广播发送的地址
static uint8_t example_macs[ESP_NOW_ETH_ALEN] = {0,0,0,0,0,0};
static uint16_t espnow_num=0; // 这个是随着我增加的设备回自动调整发送给哪个设备的发送次数
static bool wifi_open = false;
static bool flag_connet = false;//判断是否连接到其他设备了
static bool flag_sendding = false;//判断是否连接到其他设备了

static void example_espnow_deinit(example_espnow_send_param_t *send_param);

/* WiFi should start before using ESPNOW */
static void example_wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK( esp_wifi_init(&cfg) );
    ESP_ERROR_CHECK( esp_wifi_set_storage(WIFI_STORAGE_RAM) );
    ESP_ERROR_CHECK( esp_wifi_set_mode(ESPNOW_WIFI_MODE) );
    ESP_ERROR_CHECK( esp_wifi_start());
     wifi_open = true;

#if CONFIG_ESPNOW_ENABLE_LONG_RANGE
    ESP_ERROR_CHECK( esp_wifi_set_protocol(ESPNOW_WIFI_IF, WIFI_PROTOCOL_11B|WIFI_PROTOCOL_11G|WIFI_PROTOCOL_11N|WIFI_PROTOCOL_LR) );
#endif
}

/* ESPNOW sending or receiving callback function is called in WiFi task.
 * Users should not do lengthy operations from this task. Instead, post
 * necessary data to a queue and handle it from a lower priority task. */
static void example_espnow_send_cb(const uint8_t *mac_addr, esp_now_send_status_t status)
{
    example_espnow_event_t evt;
    example_espnow_event_send_cb_t *send_cb = &evt.info.send_cb;

    if (mac_addr == NULL) {
        ESP_LOGE(TAG, "Send cb arg error");
        return;
    }

    evt.id = EXAMPLE_ESPNOW_SEND_CB;
    memcpy(send_cb->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    send_cb->status = status;
    if (xQueueSend(s_example_espnow_queue, &evt, ESPNOW_MAXDELAY) != pdTRUE) {
        ESP_LOGW(TAG, "Send send queue fail");
    }
}

static void example_espnow_recv_cb(const esp_now_recv_info_t *recv_info, const uint8_t *data, int len)
{
    example_espnow_event_t evt;
    example_espnow_event_recv_cb_t *recv_cb = &evt.info.recv_cb;
    uint8_t * mac_addr = recv_info->src_addr;

    if (mac_addr == NULL || data == NULL || len <= 0) {
        ESP_LOGE(TAG, "Receive cb arg error");
        return;
    }

    evt.id = EXAMPLE_ESPNOW_RECV_CB;
    memcpy(recv_cb->mac_addr, mac_addr, ESP_NOW_ETH_ALEN);
    recv_cb->data = malloc(len);
    if (recv_cb->data == NULL) {
        ESP_LOGE(TAG, "Malloc receive data fail");
        return;
    }
    memcpy(recv_cb->data, data, len);
    recv_cb->data_len = len;
    if (xQueueSend(s_example_espnow_queue, &evt, ESPNOW_MAXDELAY) != pdTRUE) {
        ESP_LOGW(TAG, "Send receive queue fail");
        free(recv_cb->data);
    }
}


void esp_send(uint8_t *dat)
{
    example_espnow_event_t evt;

    evt.id = EXAMPLE_ESPNOW_SEND_ED;
    memcpy(evt.payload, dat, 15);
    if (xQueueSend(s_example_espnow_queue, &evt, ESPNOW_MAXDELAY) != pdTRUE)
    {
        ESP_LOGW(TAG, "Send send queue fail");
    }
}


/* Parse received ESPNOW data. */
int example_espnow_data_parse(uint8_t *data, uint16_t data_len, uint8_t *state, uint16_t *seq, int *magic, uint8_t *dat)
{
    example_espnow_data_t *buf = (example_espnow_data_t *)data;
    uint16_t crc, crc_cal = 0;

    if (data_len < sizeof(example_espnow_data_t)) {
        ESP_LOGE(TAG, "Receive ESPNOW data too short, len:%d", data_len);
        return -1;
    }

    *state = buf->state;
    *seq = buf->seq_num;
    *magic = buf->magic;
    memcpy(dat, buf->payload, sizeof(buf->payload));
    crc = buf->crc;
    buf->crc = 0;
    crc_cal = esp_crc16_le(UINT16_MAX, (uint8_t const *)buf, data_len);

    if (crc_cal == crc) {
        return buf->type;
    }

    return -1;
}

/* Prepare ESPNOW data to be sent. */
void example_espnow_data_prepare(example_espnow_send_param_t *send_param, uint8_t *dat)
{
    example_espnow_data_t *buf = (example_espnow_data_t *)send_param->buffer;

    assert(send_param->len >= sizeof(example_espnow_data_t));
    buf->flag_who=0x68;    
    buf->type = IS_BROADCAST_ADDR(send_param->dest_mac) ? EXAMPLE_ESPNOW_DATA_BROADCAST : EXAMPLE_ESPNOW_DATA_UNICAST;
    buf->state = send_param->state;
    buf->seq_num = espnow_num++;
    buf->crc = 0;
    buf->magic = send_param->magic;
    memcpy(buf->payload,dat, 15);
    /* Fill all remaining bytes after the data with random values */
    //esp_fill_random(buf->payload, send_param->len - sizeof(example_espnow_data_t));
    buf->crc = esp_crc16_le(UINT16_MAX, (uint8_t const *)buf, send_param->len);
}

static void example_espnow_task(void *pvParameter)
{
    example_espnow_event_t evt;
    uint8_t dat[15];         // 第一位为0的时候发送的时候结束进程
    uint8_t recv_state = 0;  // 判断接收数据的广播还是单独的,判断接收到的是第几个
    uint16_t recv_seq = 0;   //收到的个数
    int recv_magic = 0;      // 判断接收数据的优先级
    bool is_broadcast = false;
    bool is_link = false;
    int ret;

    vTaskDelay(2000 / portTICK_PERIOD_MS);
    ESP_LOGI(TAG, "Start sending broadcast data");

    /* Start sending broadcast ESPNOW data. */
    example_espnow_send_param_t *send_param = (example_espnow_send_param_t *)pvParameter;
    if (esp_now_send(send_param->dest_mac, send_param->buffer, send_param->len) != ESP_OK) {
        ESP_LOGE(TAG, "Send error");
        example_espnow_deinit(send_param);
        vTaskDelete(NULL);
    }

    while (xQueueReceive(s_example_espnow_queue, &evt, portMAX_DELAY) == pdTRUE) {
        switch (evt.id) {
                    {
            case EXAMPLE_ESPNOW_SEND_ED:
            {
                if(is_link == true&&flag_connet==true)
                {
                    memcpy(dat, evt.payload, 15);   // 传入数据
                    memcpy(send_param->dest_mac,example_macs,ESP_NOW_ETH_ALEN);
                    example_espnow_data_prepare(send_param, evt.payload); // 传入数据
                    //ESP_LOGI(TAG, "send data to " MACSTR "", MAC2STR(send_param->dest_mac));
                    //ESP_LOGI("SEND","%x,len%d",esp_now_send(send_param->dest_mac, send_param->buffer, send_param->len),send_param->len);
                    flag_sendding=true;
                    // if (esp_now_send(send_param->dest_mac, send_param->buffer, send_param->len) != ESP_OK)
                    // {
                    //     ESP_LOGE(TAG, "Send error1");
                    //     example_espnow_deinit(send_param);
                    //     vTaskDelete(NULL);
                    // }
                }
                break;
            }
            case EXAMPLE_ESPNOW_SEND_CB:
            {
            flag_sendding=false;
            if (is_link == false)
            {
                example_espnow_event_send_cb_t *send_cb = &evt.info.send_cb;
                is_broadcast = IS_BROADCAST_ADDR(send_cb->mac_addr); // 比较广播地址，要是不一样则为斯波

                //ESP_LOGD(TAG, "Send data to "MACSTR", status1: %d", MAC2STR(send_cb->mac_addr), send_cb->status);

                if (is_broadcast) {
                    send_param->count--;
                    if (send_param->count == 0) {
                        ESP_LOGI(TAG, "Send done");
                        example_espnow_deinit(send_param);
                        send_xbox_move(0x50,0x00,2000);//震动2秒
                        vTaskDelete(NULL);
                    }
                }

                if (!is_broadcast) {
                    ESP_LOGI(TAG, "link success");
                    send_xbox_move(0x50,0x50,2000);//震动2秒
                    is_link = true;
                    break;
                    // example_espnow_deinit(send_param);
                    // vTaskDelete(NULL);
                }



                /* Delay a while before sending the next data. 在发送下一个数据之前延迟一段时间。
                之前的哪个数据帧里面的延迟时间是在这里用的*/
                if (send_param->delay > 0) {
                    vTaskDelay(send_param->delay/portTICK_PERIOD_MS);
                }

                ESP_LOGI(TAG, "send data to "MACSTR"", MAC2STR(send_cb->mac_addr));

                memcpy(send_param->dest_mac, send_cb->mac_addr, ESP_NOW_ETH_ALEN);
                example_espnow_data_prepare(send_param,&recv_seq);

                /* Send the next data after the previous data is sent. */
                if (esp_now_send(send_param->dest_mac, send_param->buffer, send_param->len) != ESP_OK) {
                    ESP_LOGE(TAG, "Send error2");
                    example_espnow_deinit(send_param);
                    vTaskDelete(NULL);
                }
                break;
            }else{
                // ESP_LOGI("espnow", "send success");
                break;
            }
            }
            case EXAMPLE_ESPNOW_RECV_CB:
            {
                example_espnow_event_recv_cb_t *recv_cb = &evt.info.recv_cb;

                ret = example_espnow_data_parse(recv_cb->data, recv_cb->data_len, &recv_state, &recv_seq, &recv_magic,dat);
                free(recv_cb->data);
                //ESP_LOGI("data", "%s", dat); // 此为收到的数据
                if (ret == EXAMPLE_ESPNOW_DATA_BROADCAST&&recv_magic==0) {
                    //ESP_LOGI(TAG, "Receive %dth broadcast data from: "MACSTR", len: %d", recv_seq, MAC2STR(recv_cb->mac_addr), recv_cb->data_len);

                    /* If MAC address does not exist in peer list, add it to peer list. */
                    /*如果对等体列表中没有MAC地址，则将MAC地址添加到对等体列表中，必须是未连接状态并且magic为0*/
                    if (esp_now_is_peer_exist(recv_cb->mac_addr) == false&&flag_connet==false) {
                        // esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
                        // if (peer == NULL) {
                        //     ESP_LOGE(TAG, "Malloc peer information fail");
                        //     example_espnow_deinit(send_param);
                        //     vTaskDelete(NULL);
                        // }
                        // memset(peer, 0, sizeof(esp_now_peer_info_t));
                        // peer->channel = CONFIG_ESPNOW_CHANNEL;
                        // peer->ifidx = ESPNOW_WIFI_IF;
                        // peer->encrypt = true;
                        // memcpy(peer->lmk, CONFIG_ESPNOW_LMK, ESP_NOW_KEY_LEN);
                        // memcpy(peer->peer_addr, recv_cb->mac_addr, ESP_NOW_ETH_ALEN);
                        // ESP_ERROR_CHECK( esp_now_add_peer(peer) );
                        // free(peer);
                        esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
                        if (peer == NULL)
                        {
                            ESP_LOGE(TAG, "Malloc peer information fail");
                            example_espnow_deinit(send_param);
                            vTaskDelete(NULL);
                        }
                        memset(peer, 0, sizeof(esp_now_peer_info_t));
                        peer->channel = CONFIG_ESPNOW_CHANNEL;
                        peer->ifidx = ESPNOW_WIFI_IF;
                        peer->encrypt = true;
                        memcpy(peer->lmk, CONFIG_ESPNOW_LMK, ESP_NOW_KEY_LEN);
                        memcpy(peer->peer_addr, recv_cb->mac_addr, ESP_NOW_ETH_ALEN);
                        ESP_ERROR_CHECK(esp_now_add_peer(peer));
                        free(peer);
                        memcpy(example_macs,recv_cb->mac_addr,ESP_NOW_ETH_ALEN);
                        ESP_LOGI("SAVE","SAVING MAC");
                        flag_connet=true;//表示已经对接设备了
                        }

                    /* Indicates that the device has received broadcast ESPNOW data. */
                    /*设备收到广播ESPNOW数据。*/
                    ESP_LOGI("STAT","STATE IS %d",recv_state);
                    if (send_param->state == 0) {
                        send_param->state = 1;
                    }

                    /* If receive broadcast ESPNOW data which indicates that the other device has received
                    * broadcast ESPNOW data and the local magic number is bigger than that in the received
                    * broadcast ESPNOW data, stop sending broadcast ESPNOW data and start sending unicast
                    * ESPNOW data.
                    * *如果接收到广播ESPNOW数据，表示另一台设备已经接收到*广播ESPNOW数据，
                    * 且本地魔术数大于接收到的*广播ESPNOW数据，则停止发送广播ESPNOW数据，开始发送单播数据。
                    */
                    if (recv_state == 1) {//发送回给对面让他确认是否已经存了
                        /* The device which has the bigger magic number sends ESPNOW data, the other one
                         * receives ESPNOW data.
                         */
                        if (send_param->unicast == false && send_param->magic >= recv_magic) {
                    	    ESP_LOGI(TAG, "Start sending unicast data");
                    	    ESP_LOGI(TAG, "send data to "MACSTR"", MAC2STR(recv_cb->mac_addr));

                    	    /* Start sending unicast ESPNOW data. */
                            memcpy(send_param->dest_mac, recv_cb->mac_addr, ESP_NOW_ETH_ALEN);
                            example_espnow_data_prepare(send_param,&recv_seq);
                            if (esp_now_send(send_param->dest_mac, send_param->buffer, send_param->len) != ESP_OK) {
                                ESP_LOGE(TAG, "Send error3");
                                example_espnow_deinit(send_param);
                                vTaskDelete(NULL);
                            }
                            else {
                                send_param->broadcast = false;
                                send_param->unicast = true;
                            }
                        }
                        else if(send_param->unicast == false && send_param->magic <= recv_magic)
                        {
                                send_param->broadcast = false;
                                send_param->unicast = true;
                        }
                    }
                }
                else if (ret == EXAMPLE_ESPNOW_DATA_UNICAST) {
                    //ESP_LOGI(TAG, "Receive %dth unicast data from: "MACSTR", len: %d", recv_seq, MAC2STR(recv_cb->mac_addr), recv_cb->data_len);

                    /* If receive unicast ESPNOW data, also stop sending broadcast ESPNOW data. */
                    send_param->broadcast = false;
                }
                else {
                    ESP_LOGE(TAG, "Receive error data from: "MACSTR"", MAC2STR(recv_cb->mac_addr));
                }
                break;
            }
            default:
                ESP_LOGE(TAG, "Callback type error: %d", evt.id);
                break;
        }
    }
}
}

static esp_err_t example_espnow_init(void)
{
    example_espnow_send_param_t *send_param;
    uint8_t pad=0;

    s_example_espnow_queue = xQueueCreate(ESPNOW_QUEUE_SIZE, sizeof(example_espnow_event_t));
    if (s_example_espnow_queue == NULL) {
        ESP_LOGE(TAG, "Create mutex fail");
        return ESP_FAIL;
    }

    /* Initialize ESPNOW and register sending and receiving callback function. */
    ESP_ERROR_CHECK( esp_now_init() );
    ESP_ERROR_CHECK( esp_now_register_send_cb(example_espnow_send_cb) );
    ESP_ERROR_CHECK( esp_now_register_recv_cb(example_espnow_recv_cb) );
#if CONFIG_ESP_WIFI_STA_DISCONNECTED_PM_ENABLE
    ESP_ERROR_CHECK( esp_now_set_wake_window(65535) );
#endif
    /* Set primary master key. */
    ESP_ERROR_CHECK( esp_now_set_pmk((uint8_t *)CONFIG_ESPNOW_PMK) );

    /* Add broadcast peer information to peer list. */
    esp_now_peer_info_t *peer = malloc(sizeof(esp_now_peer_info_t));
    if (peer == NULL) {
        ESP_LOGE(TAG, "Malloc peer information fail");
        vSemaphoreDelete(s_example_espnow_queue);
        esp_now_deinit();
        return ESP_FAIL;
    }
    memset(peer, 0, sizeof(esp_now_peer_info_t));
    peer->channel = CONFIG_ESPNOW_CHANNEL;
    peer->ifidx = ESPNOW_WIFI_IF;
    peer->encrypt = false;
    memcpy(peer->peer_addr, s_example_broadcast_mac, ESP_NOW_ETH_ALEN);
    ESP_ERROR_CHECK(esp_now_add_peer(peer)); // 广播数据
    // memcpy(peer->peer_addr, s_example_broadcast_macs[0], ESP_NOW_ETH_ALEN);
    // ESP_ERROR_CHECK(esp_now_add_peer(peer)); // 第一个设备地址
    // memcpy(peer->peer_addr, s_example_broadcast_macs[1], ESP_NOW_ETH_ALEN);
    // ESP_ERROR_CHECK(esp_now_add_peer(peer)); // 第二个
    free(peer);                              // 用完就扔

    /* Initialize sending parameters. */
    send_param = malloc(sizeof(example_espnow_send_param_t));
    if (send_param == NULL) {
        ESP_LOGE(TAG, "Malloc send parameter fail");
        vSemaphoreDelete(s_example_espnow_queue);
        esp_now_deinit();
        return ESP_FAIL;
    }
    memset(send_param, 0, sizeof(example_espnow_send_param_t));
    send_param->unicast = false;
    send_param->broadcast = true;                                       // 广播开启
    send_param->state = 0;                                              // 默认没收到回值
    send_param->magic = 1 /* esp_random() */;                           // 这里看谁是主机
    send_param->count = CONFIG_ESPNOW_SEND_COUNT;                       // 发送次数
    send_param->delay = CONFIG_ESPNOW_SEND_DELAY;                       // 最低延迟，若延迟内不回当。。
    send_param->len = CONFIG_ESPNOW_SEND_LEN;                           // 数据的长度
    send_param->buffer = /* '10011'  */ malloc(CONFIG_ESPNOW_SEND_LEN); // 开辟13个字节的空间
    if (send_param->buffer == NULL) {
        ESP_LOGE(TAG, "Malloc send buffer fail");
        free(send_param);
        vSemaphoreDelete(s_example_espnow_queue);
        esp_now_deinit();
        return ESP_FAIL;
    }
    memcpy(send_param->dest_mac, s_example_broadcast_mac, ESP_NOW_ETH_ALEN);
    example_espnow_data_prepare(send_param,&pad);

    xTaskCreate(example_espnow_task, "example_espnow_task", 1024*3, send_param, 4, NULL);

    return ESP_OK;
}

static void example_espnow_deinit(example_espnow_send_param_t *send_param)
{
    free(send_param->buffer);
    free(send_param);
    vSemaphoreDelete(s_example_espnow_queue);
    esp_now_deinit();
}

static void wifi_deinit(void)
{
    ESP_ERROR_CHECK(esp_wifi_stop());
    ESP_ERROR_CHECK(esp_wifi_deinit());               // wifi的任务
    ESP_ERROR_CHECK(esp_event_loop_delete_default()); // wi_evt的任务
    esp_wifi_restore();
    wifi_open = false;
    // ESP_ERROR_CHECK(esp_netif_deinit());              // ti目前并不能去初始化
}
void esp_now_all_deinit(void)
{
    uint8_t dat[2] = {0, 0};
    esp_send(dat); // 结束进程
    wifi_deinit();
}

bool get_esp_now(void)
{
    return flag_connet;
}

bool get_status_esp_now(void)
{
    return flag_sendding;
}

void espnow_init(void)
{
    // Initialize NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK( nvs_flash_erase() );
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK( ret );

    example_wifi_init();
    example_espnow_init();
    vTaskDelete(NULL);
}

