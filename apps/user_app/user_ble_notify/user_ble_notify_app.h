#ifndef __REPORT_H__
#define __REPORT_H__

#include "typedef.h"
#include "user_rtc.h"


void user_ble_notify_brightness(u8 brightness);
void user_ble_notify_speed(u8 speed);

void user_ble_notify_meteor_pwr_sta(u8 pwr_sta);
void user_ble_notify_meteor_period(u8 period);
void user_ble_notify_meteor_speed(u8 speed);

void user_ble_notify_led_strip_rgb_len(u16 nums);

void user_ble_notify_dev_type(u8 dev_type);
void user_ble_notify_dev_pwr_sta(u8 pwr_sta); // 设备总开关状态
void user_ble_notify_rgb_sequence(u8 sequence); 
void user_ble_notify_alarm_info(u8 alarm_index, user_alarm_t alarm_info); 

void user_ble_notify_sound_control_type(u8 type);
void user_ble_notify_sound_control_mode(u8 mode);
void user_ble_notify_sound_control_sensitivity(u8 sensitivity);


#endif