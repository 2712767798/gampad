file(GLOB_RECURSE SOURCES 	./*.c 
							)


idf_component_register(SRCS	${SOURCES}
                   INCLUDE_DIRS 
								.
                    REQUIRES esp_wifi nvs_flash bt driver
                    INCLUDE_DIRS "E:/app/espidf/Espressif/frameworks/esp-idf-v5.0.1/components/bt/include/esp32s3/include/."
                    INCLUDE_DIRS "E:/app/espidf/Espressif/frameworks/esp-idf-v5.0.1/components/bt/host/bluedroid/api/include/api/."
                   
                      )
