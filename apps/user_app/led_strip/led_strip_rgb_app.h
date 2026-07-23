#ifndef __LED_STRIP_RGB_APP_H__
#define __LED_STRIP_RGB_APP_H__

#include "typedef.h"
#include "led_strand_effect.h"

void led_strip_rgb_set_brightness(u8 brightness_percent);
void led_strip_rgb_set_speed(u8 speed_percent);

void led_strip_rgb_set_static_color(u32 color);
void led_strip_rgb_set_static_color_by_structure(color_t color_structure);

#endif
