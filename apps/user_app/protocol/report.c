#include "report.h"
#include "user_ble_notify.h"

#include "led_strand_effect.h" // ALARM_CLOCK 类型定义

/*
	USER_TO_DO 给app反馈数据的接口，可以优化成以下形式：
	可以额外参考 app_msg_typedef.c 和 app_msg_typedef.h 中的程序
*/
void report_msg(u8 msg_type, u8 msg_data)
{
	// 根据数据类型，查表
}

// 向app反馈声控类型：手机麦或者设备麦
void report_sound_control_type(u8 type)
{
	uint8_t buf[10];
	u8 len = 0;

	buf[len++] = 0x06;
	buf[len++] = 0x07;
	buf[len++] = type;

	user_ble_notify_obj.param_put(buf, len);
}

void report_brightness(u8 brightness)
{
	uint8_t buf[10];
	u8 len = 0;

	buf[len++] = 0x04;
	buf[len++] = 0x03;
	buf[len++] = brightness;

	user_ble_notify_obj.param_put(buf, len);
}

void report_speed(u8 speed)
{
	uint8_t buf[10];
	u8 len = 0;

	buf[len++] = 0x04;
	buf[len++] = 0x04;
	buf[len++] = speed;

	user_ble_notify_obj.param_put(buf, len);
}

void report_meteor_on_off_status(u8 on_off_status)
{
	uint8_t buf[10];
	u8 len = 0;

	buf[len++] = 0x2F;
	buf[len++] = 0x02;
	buf[len++] = on_off_status;

	user_ble_notify_obj.param_put(buf, len);
}

void report_meteor_period(u8 period)
{
	uint8_t buf[10];
	u8 len = 0;

	buf[len++] = 0x2F;
	buf[len++] = 0x03;
	buf[len++] = period;

	user_ble_notify_obj.param_put(buf, len);
}

void report_meteor_speed(u8 speed)
{
	uint8_t buf[10];
	u8 len = 0;

	buf[len++] = 0x2F;
	buf[len++] = 0x01;
	buf[len++] = speed;

	user_ble_notify_obj.param_put(buf, len);
}

void report_sound_control_sensitivity(u8 sensitivity)
{
	uint8_t buf[10];
	u8 len = 0;

	buf[len++] = 0x2F;
	buf[len++] = 0x05;
	buf[len++] = sensitivity;

	user_ble_notify_obj.param_put(buf, len);
}

// 反馈 RGB 灯带的长度(灯珠数量)
void report_led_strip_rgb_len(u16 nums)
{
	uint8_t buf[10];
	u8 len = 0;

	buf[len++] = 0x04;
	buf[len++] = 0x08;
	buf[len++] = (nums >> 8) & 0xFF;
	buf[len++] = (nums >> 0) & 0xFF;

	user_ble_notify_obj.param_put(buf, len);
}

// 反馈设备类型
void report_dev_type(u8 dev_type)
{
	uint8_t buf[10];
	u8 len = 0;

	buf[len++] = 0x07;
	buf[len++] = 0x01;
	buf[len++] = 0x01;
	buf[len++] = dev_type;

	user_ble_notify_obj.param_put(buf, len);
}

void report_dev_on_off_state(u8 on_off_state)
{
	uint8_t buf[10];
	u8 len = 0;

	// 使用旧版的APP，要加上6个字节的数据
	// buf[len++] = 0x01;
	// buf[len++] = 0x01;
	// buf[len++] = 0x01;
	// buf[len++] = 0x01;
	// buf[len++] = 0x01;
	// buf[len++] = 0x01;

	buf[len++] = 0x01;
	buf[len++] = 0x01;
	buf[len++] = on_off_state;

	user_ble_notify_obj.param_put(buf, len);
}

void report_rgb_sequence(u8 sequence)
{
	uint8_t buf[10];
	u8 len = 0;

	buf[len++] = 0x04;
	buf[len++] = 0x05;
	buf[len++] = sequence;

	user_ble_notify_obj.param_put(buf, len);
}

void report_alarm_data(u8 alarm_index, alarm_t alarm_data)
{
	uint8_t buf[10];
	u8 len = 0;

	buf[len++] = 0x05;
	switch (alarm_index)
	{
	case 0:
		// 闹钟 0
		buf[len++] = 0x00;
		break;
	case 1:
		// 闹钟 1
		buf[len++] = 0x01;
		break;
	case 2:
		// 闹钟 2
		buf[len++] = 0x02;
		break;
	default:
		return;
		break;
	}

	buf[len++] = alarm_data.hour;
	buf[len++] = alarm_data.minute;
	buf[len++] = alarm_data.enable;
	buf[len++] = alarm_data.mode;

	user_ble_notify_obj.param_put(buf, len);
}

// 反馈声控模式对应的子模式
void report_sound_control_mode(u8 mode)
{
	uint8_t buf[10];
	u8 len = 0;

	buf[len++] = 0x06;
	buf[len++] = 0x06;
	buf[len++] = mode;

	user_ble_notify_obj.param_put(buf, len);
}
