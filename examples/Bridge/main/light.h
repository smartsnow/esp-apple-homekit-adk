/**
 * @file light.h
 * @author Snow Yang (snowyang.iot@outlook.com)
 * @brief 
 * @version 0.1
 * @date 2021-02-04
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#ifndef _LIGHT_H_
#define _LIGHT_H_

#include <stdbool.h>
#include <stdint.h>

#define DEMO_LIGHT_MESH_ADDR (0x0004)

void light_init(void);

void light_color_set_on(uint16_t dst, bool on);
void light_set_hue(uint16_t dst, float hue);
void light_set_saturation(uint16_t dst, float saturation);
void light_set_value(uint16_t dst, uint8_t value);

void light_white_set_on(uint16_t dst, bool on);
void light_set_brightness(uint16_t dst, uint8_t brightness);
void light_set_temperature(uint16_t dst, uint32_t temperature);

#endif