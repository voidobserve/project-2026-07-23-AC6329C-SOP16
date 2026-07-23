#ifndef __LED_STRIP_WHITE_SCHEDULE_H__
#define __LED_STRIP_WHITE_SCHEDULE_H__
// 白色流星灯 调度用 头文件

#include "typedef.h"

typedef struct
{
	u8 is_dev_open;			  // 流星灯开关
	u8 mode_index;			  // 模式索引值
	u8 app_speed;			  // 与app通信时，用到的速度值
	u16 speed;				  // 实际驱动时，使用到的速度值
	u8 period;				  // 流星灯周期值
	u16 period_cnt;			  // 周期值计数
	// u8 is_one_cycle_comleted; // 一轮动画循环是否结束
	u8 sensitivity;			  // 灵敏度
} led_strip_white_t;
extern volatile led_strip_white_t led_strip_white;

void led_strip_white_schedule_init(void);
void led_strip_white_schedule(void);

#endif