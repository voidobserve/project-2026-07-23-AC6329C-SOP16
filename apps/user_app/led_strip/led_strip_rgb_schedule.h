#ifndef __LED_STRIP_RGB_SCHEDULE_H__
#define __LED_STRIP_RGB_SCHEDULE_H__
// RBG幻彩灯带，调度 头文件

#include "typedef.h"
#include "WS2812FX.H"
#include "led_strand_effect.h"
 

typedef u16 (*led_strip_rgb_handler_t)(void);

// RGB幻彩灯的情景->处理函数映射表
typedef struct
{
	u8 light_scene; // 灯光情景
	led_strip_rgb_handler_t handler; // 灯光情景对应的处理函数
} led_strip_rgb_handler_map_t;

void led_strip_rgb_schedule_set_mode(mode_ptr mode, u16 speed, u8 option);

void led_strip_rgb_schedule_init(void);
void led_strip_rgb_schedule(void);

void dot_runningh_handle(void);

#endif
