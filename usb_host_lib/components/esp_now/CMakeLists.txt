file(GLOB_RECURSE SOURCES 	./*.c 
							)


idf_component_register(SRCS	${SOURCES}
                   INCLUDE_DIRS 
								.
                    REQUIRES nvs_flash esp_event esp_wifi xbox
                    INCLUDE_DIRS 
                   
                      )
