#ifndef led_strip_voice_h
#define led_strip_voice_h

#include "led_strand_effect.h"

// 声控模式对应的检测脚
#define SOUND_CTL_PIN IO_PORTA_08
// 声控模式检测脚对应的adc检测通道
#define SOUND_CTL_ADC_CHANNEL AD_CH_PA8

u8 get_sound_triggered_by_colorful_lights(void); // 获取七彩灯的声控结果
u8 get_sound_triggered_by_meteor_lights(void);   // 获取流星灯的声控结果
// u8 get_sound_triggered_by_motor(void);
u8 sound_triggered_by_motor_get(void);
void sound_triggered_by_motor_clear(void);

// 七彩灯声控模式下的灵敏度 增加
void colorful_lights_sound_sensitivity_add(void);
// 七彩灯声控模式下的灵敏度 减少
void colorful_lights_sound_sensitivity_sub(void);

void meteor_lights_sound_sensitivity_add(void); // 流星灯声控模式下的灵敏度 增加
void meteor_lights_sound_sensitivity_sub(void); // 流星灯声控模式下的灵敏度 减少

void motor_sound_sensitivity_add(void);
void motor_sound_sensitivity_sub(void);

u8 get_sound_triggered_by_led_strip_white(void);
u8 get_sound_triggered_by_led_strip_rgb(void);

/*
    声控电平（0 ~ 100）：
    检测到声音时直接跳到当前的声音强度（快起），没有声音时逐次衰减（慢落）。
    幻彩灯的声控动画（能量、频谱）用它来表现「声音有多大」；
    只判断「有没有声音」时用 get_sound_triggered_by_led_strip_rgb() 即可。
*/
u8 led_strip_voice_get_level(void);


void sound_ctl_init(void);






#endif
