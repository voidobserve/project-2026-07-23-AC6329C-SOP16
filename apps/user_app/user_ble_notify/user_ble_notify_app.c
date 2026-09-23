#include "user_ble_notify_app.h"
#include "user_ble_notify.h"

#include "led_strand_effect.h" // ALARM_CLOCK 类型定义
#include "app_msg_handle.h"

#include "user_config.h"

// 向app反馈声控类型：手机麦或者设备麦
void user_ble_notify_sound_control_type(u8 type)
{
    uint8_t buf[10];
    u8 len = 0;

    buf[len++] = 0x06;
    buf[len++] = 0x07;
    buf[len++] = type;

    user_ble_notify_param_put(buf, len);
}

void user_ble_notify_brightness(u8 brightness)
{
    uint8_t buf[10];
    u8 len = 0;

    buf[len++] = 0x04;
    buf[len++] = 0x03;
    buf[len++] = brightness;

    user_ble_notify_param_put(buf, len);
}

void user_ble_notify_speed(u8 speed)
{
    uint8_t buf[10];
    u8 len = 0;

    buf[len++] = 0x04;
    buf[len++] = 0x04;
    buf[len++] = speed;

    user_ble_notify_param_put(buf, len);
}

void user_ble_notify_meteor_pwr_sta(u8 pwr_sta)
{
    uint8_t buf[10];
    u8 len = 0;

    buf[len++] = 0x2F;
    buf[len++] = 0x02;
    buf[len++] = pwr_sta;

    user_ble_notify_param_put(buf, len);
}

void user_ble_notify_meteor_period(u8 period)
{
    uint8_t buf[10];
    u8 len = 0;

    buf[len++] = 0x2F;
    buf[len++] = 0x03;
    buf[len++] = period;

    user_ble_notify_param_put(buf, len);
}

void user_ble_notify_meteor_speed(u8 speed)
{
    uint8_t buf[10];
    u8 len = 0;

    buf[len++] = 0x2F;
    buf[len++] = 0x01;
    buf[len++] = speed;

    user_ble_notify_param_put(buf, len);
}

void user_ble_notify_sound_control_sensitivity(u8 sensitivity)
{
    uint8_t buf[10];
    u8 len = 0;

    buf[len++] = 0x2F;
    buf[len++] = 0x05;
    buf[len++] = sensitivity;

    user_ble_notify_param_put(buf, len);
}

// 反馈 RGB 灯带的长度(灯珠数量)
void user_ble_notify_led_strip_rgb_len(u16 nums)
{
    uint8_t buf[10];
    u8 len = 0;

    buf[len++] = 0x04;
    buf[len++] = 0x08;
    buf[len++] = (nums >> 8) & 0xFF;
    buf[len++] = (nums >> 0) & 0xFF;

    user_ble_notify_param_put(buf, len);
}

// 反馈设备类型
void user_ble_notify_dev_type(u8 dev_type)
{
    uint8_t buf[10];
    u8 len = 0;

    buf[len++] = 0x07;
    buf[len++] = 0x01;
    buf[len++] = 0x01;
    buf[len++] = dev_type;

    user_ble_notify_param_put(buf, len);
}

void user_ble_notify_dev_pwr_sta(u8 pwr_sta)
{
    uint8_t buf[10];
    u8 len = 0;

    buf[len++] = 0x01;
    buf[len++] = 0x01;
    buf[len++] = pwr_sta;

    user_ble_notify_param_put(buf, len);

#if USER_DEBUG_ENABLE
    printf("pwr_sta == %u\n", pwr_sta);
#endif
}

void user_ble_notify_rgb_sequence(u8 sequence)
{
    uint8_t buf[10];
    u8 len = 0;

    buf[len++] = 0x04;
    buf[len++] = 0x05;
    buf[len++] = sequence;

    user_ble_notify_param_put(buf, len);
}

void user_ble_notify_alarm_info(u8 alarm_idx, user_alarm_t alarm_info)
{
    uint8_t buf[10];
    u8 len = 0;
    u8 tx_byte;
    u8 i;

    buf[len++] = 0x05;
    buf[len++] = alarm_idx;
    buf[len++] = alarm_info.hour;
    buf[len++] = alarm_info.min;
    buf[len++] = alarm_info.enable << 7;
    buf[len++] = (alarm_info.power_on << 7) | (alarm_info.weekday);

    user_ble_notify_param_put(buf, len);
}

// 反馈声控模式对应的子模式
void user_ble_notify_sound_control_mode(u8 mode)
{
    uint8_t buf[10];
    u8 len = 0;

    buf[len++] = 0x06;
    buf[len++] = 0x06;
    buf[len++] = mode;

    user_ble_notify_param_put(buf, len);
}

void user_ble_notify_anim_info(app_msg_anim_info_t *info)
{
    u16 len;

    if (info == NULL || info->color_num > APP_MSG_COLOR_NUM_MAX) {
        return;
    }

    len = APP_MSG_ANIM_INFO_FIXED_LEN + info->color_num * 3;
    user_ble_notify_param_put((u8 *)info, len);
}
