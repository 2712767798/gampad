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

#ifndef I2C_H
#define I2C_H


#ifdef __cplusplus
extern "C" {
#endif

void IIC_MAIN(void);
bool CHECK_VBUS_EXIT(void);
void open_ldo3_3(void);
void close_ldo3_3(void);


#ifdef __cplusplus
}
#endif

#endif /* __ESP_HIDD_API_H__ */
