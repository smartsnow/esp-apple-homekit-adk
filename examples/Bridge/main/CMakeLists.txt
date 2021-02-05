idf_component_register(SRCS ./app_wifi.c ./app_main.c ./DB.c ./App.c ./ble_mesh_demo_main.c ./ble_mesh_demo_init.c ./board.c ./light.c 
                       INCLUDE_DIRS ".")
add_definitions(-DHAP_LOG_LEVEL=${CONFIG_HAP_LOG_LEVEL})
