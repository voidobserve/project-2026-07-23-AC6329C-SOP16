#ifndef __LED_STRIP_RGB_SCENE_SCHEDULE_H__
#define __LED_STRIP_RGB_SCENE_SCHEDULE_H__

#include "typedef.h"

/*
	幻彩灯「场景」调度
	================================================================================
	本文件是场景与 WS2812FX 之间的唯一桥梁，负责：
		1. 维护「app 模式索引 -> 动画实现 + 段选项」的映射表（模式表驱动，新增模式只加一行）；
		2. 把场景参数写进 WS2812FX 的段（效果函数、速度、方向、段选项）和全局亮度；
		3. 上电时用 flash 恢复出来的 app 动画参数启动场景；
		4. 10ms 心跳里同步旧通道（遥控器、DP 协议）对 fc_effect 亮度/速度/开关的修改。

	新增一个动画模式只需要两步：
		1. 在 led_strip_rgb_scene_anim.c 里写一个 u16 (*)(void) 的动画函数；
		2. 在 led_strip_rgb_scene_schedule.c 的 led_strip_rgb_scene_mode_table[] 里加一行。
	================================================================================
*/

// 上电启动：初始化场景 -> 用 flash 恢复的 app 动画参数填充 -> 开始跑动画
void led_strip_rgb_scene_start(void);

// 按当前场景刷新灯带（app 下发动画指令、开关灯、静态色之后调用）
void led_strip_rgb_scene_apply(void);

/*
	声控模式（app 的声控模式索引 fc_effect.music.m）
	================================================================================
	声控动画已经按「能量 / 节奏 / 频谱 / 滚动」四个主题重新设计，
	都由声控模块的声音信息驱动，并按灯带长度计算，不会超过灯串数量：
		0：能量    1：节奏    2：频谱    3：滚动
	帧间隔固定（见 ANIM_SOUND_FRAME_MS），不跟 app 下发的动画速度走 ——
	声控要跟得上声音，等几百毫秒才动一下就看不出声控效果了。
	================================================================================
*/
void led_strip_rgb_scene_sound_apply(u8 sound_mode);

// 10ms 心跳（在 WS2812FX_service 之后调用）
void led_strip_rgb_scene_tick_10ms(void);

#endif
