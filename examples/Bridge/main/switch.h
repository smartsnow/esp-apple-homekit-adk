/**
 * @file switch.h
 * @author Snow Yang (snowyang.iot@outlook.com)
 * @brief 
 * @version 0.1
 * @date 2021-02-04
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#ifndef _SWITCH_H_
#define _SWITCH_H_

#include <stdbool.h>
#include <stdint.h>

#define DEMO_SWITCH_MESH_ADDR (0x0005)

void switch_init(void);

void switch_set_on(uint16_t dst, bool on);

#endif