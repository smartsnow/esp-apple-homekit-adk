/**
 * @file light.c
 * @author your name (you@domain.com)
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

#include "light.h"

#define SHADOW_CHECK_INTERVAL_MS (10)

typedef enum
{
    LIGHT_MODE_WHITE,
    LIGHT_MODE_COLOR,
} light_mode_t;

typedef struct
{
    bool onoff;
    uint16_t hue;
    uint8_t saturation;
    uint8_t value;
    uint8_t brightness;
    uint8_t temperature;
    light_mode_t mode;
} light_shadow_t;

static light_shadow_t light_shadow;
static light_shadow_t light_shadow_last;

void mble_mesh_send_data(uint16_t dst, uint8_t *data, uint32_t len);

static void light_set_onoff_internal(uint16_t dst, bool onff)
{
    uint8_t data[4];

    /* TID */
    data[0] = 0x00;

    /* ONOFF type = 0x0100 */
    data[1] = 0x00;
    data[2] = 0x01;

    /* Value: uint8_t */
    data[3] = onff;

    mble_mesh_send_data(dst, data, sizeof(data));
}

static void light_set_hsv_internal(uint16_t dst, float hue, float saturation, uint8_t value)
{
    uint8_t data[7];

    /* TID */
    data[0] = 0x00;

    /* HSV type = 0x0123 */
    data[1] = 0x23;
    data[2] = 0x01;

    /* Hue: uint16_t */
    data[3] = (uint16_t)hue & 0xFF;
    data[4] = (uint16_t)hue >> 8;

    /* Saturation: uint8_t */
    data[5] = (uint8_t)saturation;

    /* Brightness: uint8_t */
    data[6] = value;

    mble_mesh_send_data(dst, data, sizeof(data));
}

static void light_set_brightness_internal(uint16_t dst, uint8_t brightness)
{
    uint8_t data[5];

    /* TID */
    data[0] = 0x00;

    /* Brightness type = 0x0121 */
    data[1] = 0x21;
    data[2] = 0x01;

    /* Brightness: uint16_t */
    data[3] = brightness & 0xFF;
    data[4] = brightness >> 8;

    mble_mesh_send_data(dst, data, sizeof(data));
}

static void light_set_temperature_internal(uint16_t dst, uint32_t temperature)
{
    uint8_t data[4];

    /* TID */
    data[0] = 0x00;

    /* ColorTemperature Percent type = 0x01F1 */
    data[1] = 0xF1;
    data[2] = 0x01;

    /* ColorTemperature Percent: uint8_t */

    data[3] = temperature;

    mble_mesh_send_data(dst, data, sizeof(data));
}

static void light_set_by_mode(void)
{
    switch (light_shadow.mode)
    {
    case LIGHT_MODE_WHITE:
    {
        uint8_t brightness = light_shadow.brightness;
        uint8_t temperature = light_shadow.temperature;
        if (brightness != light_shadow_last.brightness)
        {
            light_set_brightness_internal(DEMO_LIGHT_MESH_ADDR, brightness);
        }
        if (temperature != light_shadow_last.temperature)
        {
            light_set_temperature_internal(DEMO_LIGHT_MESH_ADDR, temperature);
        }
    }
    break;

    case LIGHT_MODE_COLOR:
    {
        uint16_t hue = light_shadow.hue;
        uint8_t saturation = light_shadow.saturation;
        uint8_t value = light_shadow.value;
        if (hue != light_shadow_last.hue || saturation != light_shadow_last.saturation || value != light_shadow_last.value)
        {
            light_set_hsv_internal(DEMO_LIGHT_MESH_ADDR, hue, saturation, value);
        }
    }
    break;
    }
}

static void light_task(void *arg)
{
    while (1)
    {
        vTaskDelay(SHADOW_CHECK_INTERVAL_MS);

        bool onoff = light_shadow.onoff;
        if (onoff != light_shadow_last.onoff)
        {
            if (onoff)
            {
                light_set_by_mode();
            }
            else
            {
                light_set_onoff_internal(DEMO_LIGHT_MESH_ADDR, onoff);
            }
        }
        else
        {
            light_set_by_mode();
        }

        memcpy(&light_shadow_last, &light_shadow, sizeof(light_shadow_t));
    }
}

void light_init(void)
{
    xTaskCreate(light_task, "Light task", 4096, NULL, 6, NULL);
}

void light_set_onoff(uint16_t dst, bool onoff)
{
    light_shadow.onoff = onoff;
}

void light_set_hue(uint16_t dst, float hue)
{
    light_shadow.hue = hue;
    light_shadow.mode = LIGHT_MODE_COLOR;
}

void light_set_saturation(uint16_t dst, float saturation)
{
    light_shadow.saturation = saturation;
    light_shadow.mode = LIGHT_MODE_COLOR;
}

void light_set_value(uint16_t dst, uint8_t value)
{
    light_shadow.value = value;
    light_shadow.mode = LIGHT_MODE_COLOR;
}

void light_set_brightness(uint16_t dst, uint8_t brightness)
{
    light_shadow.brightness = brightness;
    light_shadow.mode = LIGHT_MODE_WHITE;
}

void light_set_temperature(uint16_t dst, uint32_t temperature)
{
    light_shadow.temperature = (300 - temperature) * 100 / 350;
    light_shadow.mode = LIGHT_MODE_WHITE;
}
