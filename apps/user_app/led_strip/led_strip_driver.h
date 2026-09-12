
#ifndef __LED_STRIP_DRIVE_H__
#define __LED_STRIP_DRIVE_H__

// #include "board_ac632n_demo_cfg.h" 

// RGB 幻彩灯珠的长度，单位：颗灯珠
#define LED_STRIP_RGB_NUMS 6
// 纯白色灯珠的长度，单位：颗灯珠
#define LED_STRIP_WHITE_NUMS 12

// RGB 幻彩灯珠在 ws2812fx 库调用的 起始位置
#define LED_STRIP_RGB_STAR_INDEX 0

// 纯白色灯珠在 ws2812fx 库调用的 起始位置
#define LED_STRIP_WHITE_STAR_INDEX                                             \
    (LED_STRIP_RGB_STAR_INDEX + LED_STRIP_RGB_NUMS)

#define LED_STRIP_RGB_SEG_INDEX   0
#define LED_STRIP_WHITE_SEG_INDEX 1

#define LEDC_RGB_PIN   IO_PORTA_07
#define LEDC_WHITE_PIN IO_PORTA_02

void led_strip_driver_init(void);
 

#endif
