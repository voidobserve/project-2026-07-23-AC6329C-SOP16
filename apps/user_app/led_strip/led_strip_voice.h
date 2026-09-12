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


void sound_ctl_init(void);






#endif
