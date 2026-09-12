#ifndef __LED_STRIP_RGB_ANIM_H__
#define __LED_STRIP_RGB_ANIM_H__

// RBG幻彩灯带，头文件
#include "typedef.h"

// 检查是否触发了声控：
#define __LED_STRIP_RGB_GET_SOUND_TRIGGER__()                                  \
    get_sound_triggered_by_led_strip_rgb()

typedef enum
{
    E_RISE,
    E_TOP
} FS_ACT;

typedef struct
{
    FS_ACT act;        // 当前动作，0：上升：1：顶部效果
    uint16_t rise_tag; // 上升的目标值
    uint16_t c_pos;    // 当前位置
    uint8_t fall_sp;   // 下降速度
    short int top_pos;
    uint8_t top_sp; // 顶端速度
    uint32_t bgc;   // 背景颜色
} music_fs_t;       // 频谱声控参数结构体

typedef struct
{
    uint8_t phase;          // 当前动作阶段，E_RISE=上升/张开，E_TOP=到顶/收合
    uint16_t forward_pos;   // 正向位置计数器
    uint16_t reverse_pos;   // 反向位置计数器
    uint16_t target_len;    // 触发/目标长度（声控上升的高度）
} music_open_close_state_t; // music open/close 开合模式状态

uint16_t led_strip_rgb_anim_single_color_meteor(void);
u16 led_strip_rgb_anim_mutil_fade(void);
u16 led_strip_rgb_anim_single_block_scan(void);
uint16_t led_strip_rgb_anim_mutil_breath(void);
u16 led_strip_rgb_anim_breath(void);
u16 led_strip_rgb_anim_breathing(void);
uint16_t led_strip_rgb_anim_mutil_twihkle(void);
uint16_t led_strip_rgb_anim_multi_block_scan(void);
u16 led_strip_rgb_anim_multi_colors_gradual(void);
// u16 led_strip_rgb_anim_starry_sky(void);
uint16_t led_strip_rgb_anim_fire_flicker_intense(void);
u16 led_strip_rgb_anim_meteor_signle_color(void);
u16 led_strip_rgb_anim_background_meteor(void);
u16 led_strip_rgb_anim_open(void);
u16 led_strip_rgb_anim_close(void);
u16 led_strip_rgb_anim_multi_dot_running(void);
u16 led_strip_rgb_anim_meteor_jump(void);
u16 led_strip_rgb_anim_meteor_gradual(void);
u16 led_strip_rgb_anim_rainbow_flow(void);
u16 led_strip_rgb_anim_when_adjust_sequence(void);

u16 led_strip_rgb_anim_single_superposition(void);
u16 led_strip_rgb_anim_single_superposition_with_background(void);
u16 led_strip_rgb_anim_multi_superposition(void);

u16 led_strip_rgb_anim_fireworks(void);

// ==================================================================
// ==================================================================
// 声控模式：

void music_open_close_set_trigger_len(u8 percent); // 设置声控开合模式的目标长度
u16 led_strip_rgb_anim_sound_control_star_random(void);
u16 led_strip_rgb_anim_sound_control_star_white(void);
u16 led_strip_rgb_anim_sound_control_star_blue(void);
uint16_t led_strip_rgb_anim_sound_control_open_and_close_both_sides(void);
uint16_t led_strip_rgb_anim_sound_control_open_and_close_slide_to_center(void);
uint16_t led_strip_rgb_anim_sound_control_feq_rise(void);
uint16_t led_strip_rgb_anim_sound_control_feq_rise_bround_color(void);
uint16_t led_strip_rgb_anim_sound_control_feq_rise_green_and_blue(void);
uint16_t led_strip_rgb_anim_sound_control_multi_color_flow(void);

uint16_t led_strip_rgb_anim_sound_control_rainbow_flash(void);
void __led_strip_rgb_anim_sound_control_feq_rise_set__(u8 percent); 
uint16_t led_strip_rgb_anim_sound_control_energy(void);

u16 led_strip_rgb_anim_pwr_off(void);

#endif