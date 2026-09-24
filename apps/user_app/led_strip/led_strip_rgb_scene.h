#ifndef __LED_STRIP_RGB_SCENE_H__
#define __LED_STRIP_RGB_SCENE_H__

#include "typedef.h"
#include "app_msg_typedef.h" // app_msg_anim_info_t、APP_MSG_MOD_IDX_xxx
#include "led_strip_sys.h"   // color_t、DEVICE_ON / DEVICE_OFF

/*
	幻彩灯「场景」(scene)
	================================================================================
	重构说明：
		旧的 fc_effect 把「灯带参数、灯光情景、音乐、电机、闹钟、倒计时」等
		全部揉在一个结构体里，字段名也不直观（b / app_b / speed / app_speed /
		dream_scene.seg_size ...），app 下发一条动画指令需要绕好几个弯。

		新的做法是用一个「场景」结构体，专门描述 app 下发的整条灯带显示要求：
			- 当前模式索引
			- 动画方向
			- 动画亮度（百分比）
			- 动画速度（百分比）
			- 保留字节（来自 app 协议 byte6，目前未使用）
			- 多少个灯为一组（来自 app 协议 byte7，协议里叫「段数量」）
			- 动画底色（默认黑色）
			- 动画使用的颜色数量（不包含底色）
			- 动画使用的颜色数据（R、G、B）

		新的动画实现（led_strip_rgb_scene_anim.c）只读取本结构体，
		不再读取 fc_effect.dream_scene，参数含义一目了然，扩展新动画时
		也不需要再去猜某个字段的单位和取值范围。

	数据流：
		app 指令
			-> app_msg_handle.c 解析成 app_msg_anim_info_t
			-> led_strip_rgb_scene_set_from_app_info()    [本文件]
			-> led_strip_rgb_scene_apply()                [led_strip_rgb_scene_schedule.c]
			-> 新的动画实现                                [led_strip_rgb_scene_anim.c]

	兼容说明：
		旧的 fc_effect 保留不删（遥控器、DP 协议、白光流星灯等旧通道还在用），
		场景在更新时会把自己的关键字段同步回 fc_effect，
		旧通道修改 fc_effect 的亮度/速度/开关时，也会在 10ms 心跳里同步回场景。
	================================================================================
*/

// 场景中最多保存的颜色数量（与 app 协议保持一致）
#define LED_STRIP_RGB_SCENE_COLOR_MAX APP_MSG_COLOR_NUM_MAX

// 动画方向
#define LED_STRIP_RGB_SCENE_DIR_FORWARD 0 // 正向
#define LED_STRIP_RGB_SCENE_DIR_REVERSE 1 // 反向

typedef struct
{
    u8 pwr_sta;  // 开关：DEVICE_ON(1) / DEVICE_OFF(0)
    u8 mode_idx; // 当前模式索引，取值见 app_msg_typedef.h 的 APP_MSG_MOD_IDX_xxx
    u8 dir;      // 动画方向：0 正向，1 反向
    u8 brightness;    // 动画亮度，单位：百分比，0 ~ 100
    u8 speed;         // 动画速度，单位：百分比，0 ~ 100（100 最快）
    u8 byte_reserved; // 保留，未使用
    u8 leds_per_seg; // 多少个灯为一组（协议 byte7，协议里的字段名叫「段数量」，0 视为 1）
    color_t background; // 动画底色，默认黑色
    u8 color_num;       // 动画使用的颜色数量（不包含底色）
    color_t colors[LED_STRIP_RGB_SCENE_COLOR_MAX]; // 动画使用的颜色数据
} led_strip_rgb_scene_t;

extern volatile led_strip_rgb_scene_t led_strip_rgb_scene; // 当前幻彩灯场景

/* ---------------------------------- 写入接口 ---------------------------------- */

// 复位为默认场景（静态红、亮度 100%、速度 80%、一段一个灯、底色黑色）
void led_strip_rgb_scene_init(void);

// 用 app 下发的动画参数更新场景（内部会做范围校验）
void led_strip_rgb_scene_set_from_app_info(const app_msg_anim_info_t *info);

// 把当前场景转换成 app 需要的动画参数（用于 app 同步指令的状态上报）
void led_strip_rgb_scene_to_app_info(app_msg_anim_info_t *info);

// 开关灯
void led_strip_rgb_scene_set_power(u8 on_off);

// 设置动画亮度（百分比），会同步给 WS2812FX 和 fc_effect
void led_strip_rgb_scene_set_brightness(u8 percent);

// 设置动画速度（百分比），会同步给 WS2812FX 和 fc_effect
void led_strip_rgb_scene_set_speed(u8 percent);

// 设置静态颜色（会把模式切成静态色）
void led_strip_rgb_scene_set_static_color(u8 r, u8 g, u8 b);
 

// 设置动画方向
void led_strip_rgb_scene_set_direction(u8 dir);

// 设置颜色池与底色
void led_strip_rgb_scene_set_colors(const color_t *colors, u8 color_num,
                                    color_t background);

/* -------------------------------- 有效值/换算接口 ------------------------------- */

// 获取「多少个灯为一组」（app 协议 byte7，协议里叫「段数量」）：至少为 1
// 上限交给动画层按当前真实的段长度(_seg_len)去夹，这里不夹，
// 否则 app 下发 5 会被夹成 1，动画就变成「每个灯一种颜色」
u8 led_strip_rgb_scene_get_leds_per_seg(void);

// 颜色数量有效值：至少为 1
u8 led_strip_rgb_scene_get_color_num(void);

// 取第 idx 个动画颜色（自动对颜色数量取模，越界不会崩）
color_t led_strip_rgb_scene_get_color(u8 idx);

// 取动画底色
color_t led_strip_rgb_scene_get_background(void);

// 是否反向
u8 led_strip_rgb_scene_is_reverse(void);

// 速度百分比 -> WS2812FX 的帧间隔（ms，值越大越慢）
u16 led_strip_rgb_scene_speed_to_ms(u8 percent);

// 亮度百分比 -> WS2812FX 的亮度值（0 ~ 255）
u8 led_strip_rgb_scene_brightness_to_255(u8 percent);

#endif
