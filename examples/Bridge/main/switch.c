/**
 * @file switch.c
 * @author Snow Yang (snowyang.iot@outlook.com)
 * @brief 
 * @version 0.1
 * @date 2021-02-04
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include <stdio.h>
#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_bt.h"

#include "esp_ble_mesh_defs.h"
#include "esp_ble_mesh_common_api.h"
#include "esp_ble_mesh_networking_api.h"
#include "esp_ble_mesh_provisioning_api.h"
#include "esp_ble_mesh_config_model_api.h"
#include "esp_ble_mesh_local_data_operation_api.h"

#include "switch.h"

#define TAG "Switch"

#define SHADOW_CHECK_INTERVAL_MS (10)

typedef struct
{
    bool on;
} switch_shadow_t;

static switch_shadow_t switch_shadow;
static switch_shadow_t switch_shadow_last;

void mble_mesh_send_data(uint16_t dst, uint8_t *data, uint32_t len);

static void switch_set_on_internal(uint16_t dst, bool on)
{
    ESP_LOGI(TAG, "ON = %d", on);

    uint8_t data[4];

    /* TID */
    data[0] = 0x00;

    /* ONOFF type = 0x0100 */
    data[1] = 0x00;
    data[2] = 0x01;

    /* Value: uint8_t */
    data[3] = on;

    mble_mesh_send_data(dst, data, sizeof(data));
}

static void switch_task(void *arg)
{
    while (1)
    {
        vTaskDelay(SHADOW_CHECK_INTERVAL_MS);

        bool on = switch_shadow.on;
        if (on != switch_shadow_last.on)
        {
            switch_set_on_internal(DEMO_SWITCH_MESH_ADDR, on);
            switch_shadow_last.on = on;
        }
    }
}

void switch_init(void)
{
    xTaskCreate(switch_task, "Switch task", 4096, NULL, 6, NULL);
}

void switch_set_on(uint16_t dst, bool on)
{
    switch_shadow.on = on;
}
