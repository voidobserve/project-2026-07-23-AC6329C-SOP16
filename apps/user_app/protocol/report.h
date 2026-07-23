#ifndef __REPORT_H__
#define __REPORT_H__

#include "typedef.h"
#include "alarm.h"


void report_brightness(u8 brightness);
void report_speed(u8 speed);

void report_meteor_on_off_status(u8 on_off_status);
void report_meteor_period(u8 period);
void report_meteor_speed(u8 speed);

void report_led_strip_rgb_len(u16 nums);

void report_dev_type(u8 dev_type);
void report_dev_on_off_state(u8 on_off_state); // 设备总开关状态
void report_rgb_sequence(u8 sequence);
void report_alarm_data(u8 alarm_index, alarm_t alarm_data);

void report_sound_control_type(u8 type);
void report_sound_control_mode(u8 mode);
void report_sound_control_sensitivity(u8 sensitivity);


#endif