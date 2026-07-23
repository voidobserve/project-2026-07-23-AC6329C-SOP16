// 白色流星灯 调度用 C文件

#include "led_strip_white_schedule.h"
#include "led_strand_effect.h"
#include "ws2812fx_effect.h"
#include "led_strip_white_anim.h"

volatile led_strip_white_t led_strip_white;

void led_strip_white_schedule_init(void)
{
	led_strip_white.is_dev_open = DEVICE_ON;
	led_strip_white.mode_index = 1;
	led_strip_white.app_speed = 80;
	led_strip_white.speed = (u32)330 * led_strip_white.app_speed / 100;
	led_strip_white.period = 2;									// 默认 2 秒  周期值
	led_strip_white.period_cnt = led_strip_white.period * 1000; // 周期值计数值，单位 ms
	// led_strip_white.is_one_cycle_comleted = 0;
	led_strip_white.sensitivity = 85;
}

void led_strip_white_schedule(void)
{
	mode_ptr meteor_light_mode_ptr = NULL;
	u8 opt = NO_OPTIONS;

	if (led_strip_white.is_dev_open == DEVICE_OFF)
	{
		// 流星灯当前是关闭的，跑关机对应的模式，不根据模式索引进行调节  
		meteor_light_mode_ptr = &led_strip_white_anim_pwr_off; 
		WS2812FX_setSegment_colorOptions(
			LED_STRIP_WHITE_SEG_INDEX,	// 第 x 段
			LED_STRIP_WHITE_STAR_INDEX, // 起始位置
			LED_STRIP_WHITE_LEN - 1,	// 结束位置
			meteor_light_mode_ptr,		// 效果
			BLACK,						// 颜色
			led_strip_white.speed,		// 速度
			opt);						// 选项

		WS2812FX_resetSegmentRuntime(LED_STRIP_WHITE_SEG_INDEX); // 重置流星灯所在的段运行时参数
		WS2812FX_running_flag_set();
		os_taskq_post("msg_task", 1, MSG_USER_SAVE_INFO); 
		return;
	}

	// printf("led_strip_white.mode_index == %u\n", (u16)led_strip_white.mode_index);

	// 流星效果
	if (led_strip_white.mode_index == 1)
	{
		// 单流星
		meteor_light_mode_ptr = &led_strip_white_anim_comet_1_whit_max_brightness;
		opt = FADE_FAST; // 选项
	}
	else if (led_strip_white.mode_index == 2)
	{
		// 单流星
		meteor_light_mode_ptr = &led_strip_white_anim_comet_1_whit_max_brightness;
		opt = FADE_SLOW; // 选项
	}
	else if (led_strip_white.mode_index == 3)
	{ // 单流星（反向）
		meteor_light_mode_ptr = &led_strip_white_anim_comet_1_whit_max_brightness;
		opt = FADE_FAST | REVERSE; // 选项
	}
	else if (led_strip_white.mode_index == 4)
	{
		meteor_light_mode_ptr = &led_strip_white_anim_comet_1_whit_max_brightness;
		opt = FADE_SLOW | REVERSE; // 选项
	}
	else if (led_strip_white.mode_index == 5)
	{
		meteor_light_mode_ptr = &led_strip_white_anim_comet_4_with_max_brightness;
		opt = NO_OPTIONS; // 选项
	}
	else if (led_strip_white.mode_index == 6)
	{
		// 两段流星灯追逐
		meteor_light_mode_ptr = &led_strip_white_anim_chase_with_max_brightness;
		opt = NO_OPTIONS; // 选项
	}
	else if (led_strip_white.mode_index == 7)
	{
		// 两段流星灯追逐
		meteor_light_mode_ptr = &led_strip_white_anim_chase_with_max_brightness;
		opt = REVERSE; // 选项
	}
	else if (led_strip_white.mode_index == 8)
	{
		// 改成先流星上半，时间间隔结束后，流星下半
		meteor_light_mode_ptr = &led_strip_white_anim_half_flow_with_max_brightness;
		opt = FADE_MEDIUM; // 选项
	}
	else if (led_strip_white.mode_index == 9)
	{
		// 改成先流星上半，时间间隔结束后，流星下半
		meteor_light_mode_ptr = &led_strip_white_anim_half_flow_with_max_brightness;
		opt = REVERSE | FADE_MEDIUM; // 选项
	}
	else if (led_strip_white.mode_index == 10)
	{
		// 单点流水，最后四个灯堆积
		meteor_light_mode_ptr = &led_strip_white_anim_single_flow_and_stack_with_max_brightness;
		opt = NO_OPTIONS; // 选项
	}
	else if (led_strip_white.mode_index == 11)
	{
		// 单点流水，最后四个灯堆积（反向）
		meteor_light_mode_ptr = &led_strip_white_anim_single_flow_and_stack_with_max_brightness;
		opt = REVERSE; // 选项
	}
	else if (led_strip_white.mode_index == 12)
	{
		// 堆积流水
		meteor_light_mode_ptr = &led_strip_white_anim_stack_flow_with_max_brightness;
		opt = NO_OPTIONS; // 选项
	}
	else if (led_strip_white.mode_index == 13)
	{
		// 堆积流水（反向）
		meteor_light_mode_ptr = &led_strip_white_anim_stack_flow_with_max_brightness;
		opt = REVERSE; // 选项
	}
	else if (led_strip_white.mode_index == 14)
	{
		// 堆积流水(正向) + 堆积流水(反向)
		meteor_light_mode_ptr = &led_strip_white_anim_stack_flow_plus_reverse_with_max_brightness;
		opt = NO_OPTIONS; // 选项
	}
	else if (led_strip_white.mode_index == 15)
	{
		// 音乐律动1
		meteor_light_mode_ptr = &led_strip_white_anim_sound_control_meteor_with_max_brightness;
		opt = NO_OPTIONS; // 选项
	}
	else if (led_strip_white.mode_index == 16)
	{
		// 音乐律动2
		meteor_light_mode_ptr = &led_strip_white_anim_sound_control_meteor3_with_max_brightness;
		opt = NO_OPTIONS; // 选项
	}
	else
	{
		// index 不在 索引范围内，直接返回
		return;
	}

	WS2812FX_stop();
	WS2812FX_setSegment_colorOptions(
		LED_STRIP_WHITE_SEG_INDEX,	// 第 x 段
		LED_STRIP_WHITE_STAR_INDEX, // 起始位置
		LED_STRIP_WHITE_LEN - 1,	// 结束位置
		meteor_light_mode_ptr,		// 效果
		WHITE,						// 颜色
		led_strip_white.speed,		// 速度
		opt);						// 选项

	WS2812FX_resetSegmentRuntime(LED_STRIP_WHITE_SEG_INDEX); // 重置流星灯所在的段运行时参数
	WS2812FX_running_flag_set();
	os_taskq_post("msg_task", 1, MSG_USER_SAVE_INFO);
}
