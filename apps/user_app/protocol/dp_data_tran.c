#include "cpu.h"
#include "led_strip_sys.h"
#include "paint_tool.h"
#include "dp_data_tran.h"
#include "led_strand_effect.h"
#include "ble_multi_profile.h"
#include "led_strand_effect.h"
#include "WS2812FX.H"
#include "btstack/le/att.h"
#include "ble_user.h"
#include "btstack/le/ble_api.h"
#include "led_strip_driver.h"
// #include "one_wire.h"

#include "user_include.h"
#include "led_strand_effect.h"
#include "user_ble_notify.h"
#include "report.h"
#include "app_msg_typedef.h"
#include "led_strip_white_schedule.h"
#include "Adafruit_NeoPixel.h"

#include "user_rtc.h"

// 和通信协议对应
const u8 rgb_sequence_map[6] = {
    NEO_RGB, NEO_RBG, NEO_GRB, NEO_GBR, NEO_BRG, NEO_BGR,
};

dp_data_header_t dp_data_header; // 涂鸦DP数据头
dp_switch_led_t dp_switch_led;   // DPID_SWITCH_LED开关
dp_work_mode_t dp_work_mode;     // DPID_WORK_MODE工作模式
dp_countdown_t dp_countdown;     // DPID_COUNTDOWN倒计时
dp_music_data_t dp_music_data;   // DPID_MUSIC_DATA音乐灯
// dp_secene_data_t  dp_secene_data;  //DPID_RGBIC_LINERLIGHT_SCENE炫彩情景
dp_lednum_set_t dp_lednum_set; // DPID_LED_NUMBER_SET led点数设置
dp_draw_tool_t dp_draw_tool;   // DPID_DRAW_TOOL 涂抹功能

/************************************************************************************
 *@  函数：string_hex_Byte
 *@  描述：把字符串转为HEX，支持小写abcdef,输出uint8、uint16、uint32类型的hex
 *@  形参1: str         <字符输入>
 *@  形参2: out_Byte    <输出的1、2、4字节>
 *@  返回：unsigned long
 ************************************************************************************/
unsigned long string_hex_Byte(char *str, unsigned char out_Byte)
{
    int i = 0;
    unsigned char temp = 0;
    unsigned long hex = 0;
    if ((out_Byte == 1) || (out_Byte == 2) || (out_Byte == 4)) {
        while (i < (out_Byte * 2)) {
            hex <<= 8;
            if (str[i] >= '0' && str[i] <= '9') {
                temp = (str[i] & 0x0f);
            } else if (str[i] >= 'a' && str[i] <= 'f') {
                temp = ((str[i] + 0x09) & 0x0f);
            }
            ++i;
            if (out_Byte != 1) {
                temp <<= 4;
                if (str[i] >= '0' && str[i] <= '9') {
                    temp |= (str[i] & 0x0f);
                } else if (str[i] >= 'a' && str[i] <= 'f') {
                    temp |= ((str[i] + 0x09) & 0x0f);
                }
            }
            ++i;
            hex |= temp;
        }
    } else
        return 0xffffffff; // 错误类型
    return hex;
}

/*****************************************
 *@  函数：dp_extract_data_handle
 *@  描述：dp数据提取
 *@  形参: buff
 *@  返回：DP数据长度
 ****************************************/
void dp_extract_data_handle(unsigned char *buff)
{
    unsigned char num = 0;
    unsigned char max;
    unsigned char temp;
    /*提取涂鸦DP数据头*/
    dp_data_header.id = buff[0];
    dp_data_header.type = buff[1];
    dp_data_header.len = buff[2];
    dp_data_header.len <<= 8;
    dp_data_header.len |= buff[3];

    /*提取DP数据*/
    switch (dp_data_header.id) {
        // case DPID_SWITCH_LED: // 开关(可下发可上报)
        //     printf("\r\n DPID_SWITCH_LED");

        //     fc_effect.on_off_flag = buff[4];
        //     if (fc_effect.on_off_flag == DEVICE_ON)
        //     {
        //         // soft_turn_on_the_light();
        //     }
        //     else
        //     {
        //         // soft_rurn_off_lights();
        //     }
        //     break;

    case DPID_WORK_MODE: // 工作模式(可下发可上报)
        dp_work_mode.mode = buff[4];

        printf("dp_work_mode.mode = %d\r\n", dp_work_mode.mode);
        printf("\r\n");

        break;

    case DPID_BRIGHT_VALUE: // 白光亮度(可下发可上报)
        break;

    case DPID_COLOUR_DATA: // 彩光(可下发可上报)
        break;

    case DPID_COUNTDOWN: // 倒计时(可下发可上报)
        // DP协议参数
        dp_countdown.time = buff[4];
        dp_countdown.time <<= 8;
        dp_countdown.time |= buff[5];
        dp_countdown.time <<= 8;
        dp_countdown.time |= buff[6];
        dp_countdown.time <<= 8;
        dp_countdown.time |= buff[7];

        printf("dp_countdown.time = %d\r\n", dp_countdown.time);
        printf("\r\n");

        // 效果参数
        fc_effect.countdown.time = dp_countdown.time;

        printf("fc_effect.countdown.time = %d\r\n", fc_effect.countdown.time);
        printf("\r\n");

        break;

    case DPID_MUSIC_DATA: // 音乐律动(只下发)
        // DP协议参数
        dp_music_data.change_type = string_hex_Byte(&buff[4], 1);
        dp_music_data.colour.h_val = string_hex_Byte(&buff[5], 2);
        dp_music_data.colour.s_val = string_hex_Byte(&buff[9], 2);
        dp_music_data.colour.v_val = string_hex_Byte(&buff[13], 2);
        dp_music_data.white_b = string_hex_Byte(&buff[17], 2);
        dp_music_data.ct = string_hex_Byte(&buff[21], 2);

        break;

    case DPID_RGBIC_LINERLIGHT_SCENE: // 炫彩情景(可下发可上报)
        fc_effect.Now_state = IS_light_scene;

        // 变化类型、模式
        fc_effect.dream_scene.change_type = buff[4];
        // 方向
        fc_effect.dream_scene.direction = buff[5];
        // 段大小
        fc_effect.dream_scene.seg_size = buff[6];
        // 颜色数量
        fc_effect.dream_scene.c_n = buff[7];
        if (fc_effect.dream_scene.c_n > MAX_NUM_COLORS) {
            fc_effect.dream_scene.c_n = MAX_NUM_COLORS;
        }
        // 清除rgb[0~n]数据
        memset(fc_effect.dream_scene.rgb, 0, sizeof(fc_effect.dream_scene.rgb));

        // 数据循环传输
        for (num = 0; num < fc_effect.dream_scene.c_n; num++) {
            fc_effect.dream_scene.rgb[num].r = buff[8 + num * 3];
            fc_effect.dream_scene.rgb[num].g = buff[9 + num * 3];
            fc_effect.dream_scene.rgb[num].b = buff[10 + num * 3];
        }

#if USER_DEBUG_ENABLE
        printf("fc_effect.dream_scene.c_n == %u\n",
               (u16)fc_effect.dream_scene.c_n);
        printf("fc_effect.dream_scene.seg_size == %u\n",
               (u16)fc_effect.dream_scene.seg_size);
#endif

        // 包含多段不同颜色的跳变模式和渐变模式，由于RGB灯串一共只有6个灯，这里将颜色段设置为1
        if ((fc_effect.dream_scene.change_type == 0x02 ||
             fc_effect.dream_scene.change_type == 0x0A) &&
            fc_effect.dream_scene.seg_size == 0x05) {
            fc_effect.dream_scene.seg_size = 1;
        }
        // else if (fc_effect.dream_scene.change_type == 0x0B && // 呼吸模式
        //          fc_effect.dream_scene.c_n == 0x07)           // 呼吸模式、7种颜色 -> 炫彩呼吸
        // {
        //     const u8 color_buf[] = {
        //         0xFF, 0x00, 0x00,  // R
        //         0x00, 0xFF, 0x00,  // G
        //         0x00, 0x00, 0xFF,  // B
        //         0xFF, 0xFF, 0x00,  // YELLOW
        //         0x00, 0xFF, 0xFF,  // CYAN
        //         0xFF, 0x00, 0xFF,  // MAGENTA
        //         0xFF, 0xFF, 0xFF}; // WHITE
        //     for (num = 0; num < 7; num++)
        //     {
        //         fc_effect.dream_scene.rgb[num].r = color_buf[num * 3];
        //         fc_effect.dream_scene.rgb[num].g = color_buf[num * 3 + 1];
        //         fc_effect.dream_scene.rgb[num].b = color_buf[num * 3 + 2];
        //     }
        // }

        led_strip_rgb_schedule();
        break;

    case DPID_LED_NUMBER_SET: // led点数设置(可下发可上报)
        // DP协议参数
        dp_lednum_set.lednum = buff[4];
        dp_lednum_set.lednum <<= 8;
        dp_lednum_set.lednum |= buff[5];
        dp_lednum_set.lednum <<= 8;
        dp_lednum_set.lednum |= buff[6];
        dp_lednum_set.lednum <<= 8;
        dp_lednum_set.lednum |= buff[7];

        printf("dp_lednum_set.lednum = %d\r\n", dp_lednum_set.lednum);
        printf("\r\n");

        // 效果参数
        fc_effect.led_num = dp_lednum_set.lednum;

        if (fc_effect.led_num >= 48)
            fc_effect.led_num = 48;

        break;

    case DPID_DRAW_TOOL: // 涂抹功能(可下发可上报)
        break;
    }
}

extern hci_con_handle_t ZD_HCI_handle;

extern ALARM_CLOCK alarm_clock[3];
extern TIME_CLOCK time_clock;

/**
 * @brief 向app反馈信息
 *
 * @param p   数据内容
 * @param len  数据长度
 */
void zd_fb_2_app(u8 *p, u8 len)
{
    uint8_t buffer[30];     // 发送缓存
    memcpy(buffer, p, len); //
    user_ble_notify_param_put(buffer, len);
}

/*********************************************************
 *
 *      APP反馈 API
 *
 *********************************************************/
/**
 * @brief 向app反馈灯开关状态
 *
 */
void fb_led_on_off_state(void)
{
    uint8_t tp_buffer[10];
    u8 len = 0;
    // 0x 01 E9 00，指令前缀
    tp_buffer[len++] = 0x01;
    tp_buffer[len++] = 0xE9;
    tp_buffer[len++] = 0x00;

    tp_buffer[len++] = 0x01;
    tp_buffer[len++] = 0x01;
    tp_buffer[len++] = fc_effect.on_off_flag; //

    zd_fb_2_app(tp_buffer, len);
}
/**
 * @brief 反馈音乐模式
 *
 */
// void fb_led_music_mode(void)
// {
//     uint8_t tp_buffer[6];
//     tp_buffer[0] = 0x06;
//     tp_buffer[1] = 0x06;
//     tp_buffer[2] = fc_effect.music.m; //
//     zd_fb_2_app(tp_buffer, 3);
// }

/**
 * @brief 反馈流星速度
 *
 */
void fd_meteor_speed(void)
{
    uint8_t tp_buffer[10];
    u8 len = 0;
    // 0x 01 E9 00，指令前缀
    tp_buffer[len++] = 0x01;
    tp_buffer[len++] = 0xE9;
    tp_buffer[len++] = 0x00;

    tp_buffer[len++] = 0x2F;
    tp_buffer[len++] = 0x01;
    tp_buffer[len++] = fc_effect.app_star_speed;
    zd_fb_2_app(tp_buffer, len);
}

/**
 * @brief 反馈流星周期
 *
 */
void fd_meteor_cycle(void)
{
    uint8_t tp_buffer[10];
    u8 len = 0;
    // 0x 01 E9 00，指令前缀
    tp_buffer[len++] = 0x01;
    tp_buffer[len++] = 0xE9;
    tp_buffer[len++] = 0x00;

    tp_buffer[len++] = 0x2F;
    tp_buffer[len++] = 0x03;
    tp_buffer[len++] = fc_effect.meteor_period;

    zd_fb_2_app(tp_buffer, len);
}

/**
 * @brief 反馈流星开关
 *
 */
void fd_meteor_on_off(void)
{
    uint8_t tp_buffer[10];
    u8 len = 0;
    u8 data = DEVICE_OFF;
    // 指令前缀 0x 01 E9 00
    tp_buffer[len++] = 0x01;
    tp_buffer[len++] = 0xE9;
    tp_buffer[len++] = 0x00;

    tp_buffer[len++] = 0x2F;
    tp_buffer[len++] = 0x02;

    // 目前在 app 中，1表示开启，2表示关闭
    if (DEVICE_ON == fc_effect.star_on_off) {
        data = 1;
    } else {
        data = 2;
    }

    tp_buffer[len++] = data;
    zd_fb_2_app(tp_buffer, len);
}

void fb_bright(void)
{
    uint8_t tp_buffer[10];
    u8 len = 0;

    tp_buffer[len++] = 0x04;
    tp_buffer[len++] = 0x03;
    tp_buffer[len++] = fc_effect.app_b;

    zd_fb_2_app(tp_buffer, len);
}

void fb_speed(void)
{
    uint8_t tp_buffer[10];
    u8 len = 0;

    tp_buffer[len++] = 0x04;
    tp_buffer[len++] = 0x04;
    tp_buffer[len++] = fc_effect.app_speed;

    zd_fb_2_app(tp_buffer, len);
}

void fb_sensitive(void)
{
    uint8_t tp_buffer[10];
    u8 len = 0;
    // 0x 01 E9 00，指令前缀
    tp_buffer[len++] = 0x01;
    tp_buffer[len++] = 0xE9;
    tp_buffer[len++] = 0x00;

    tp_buffer[len++] = 0x2F;
    tp_buffer[len++] = 0x05;
    tp_buffer[len++] = fc_effect.music.s;

    zd_fb_2_app(tp_buffer, len);
}

void fb_rgb_value(void)
{
    uint8_t tp_buffer[10];
    u8 len = 0;
    // 0x 01 E9 00，指令前缀
    tp_buffer[len++] = 0x01;
    tp_buffer[len++] = 0xE9;
    tp_buffer[len++] = 0x00;

    tp_buffer[len++] = 0x04;
    tp_buffer[len++] = 0x01;
    tp_buffer[len++] = 0x1e;
    tp_buffer[len++] = fc_effect.rgb.r;
    tp_buffer[len++] = fc_effect.rgb.g;
    tp_buffer[len++] = fc_effect.rgb.b;

    zd_fb_2_app(tp_buffer, len);
}

// void fb_RGBsequence(void)
// {
//     uint8_t tp_buffer[6];
//     tp_buffer[0] = 0x04;
//     tp_buffer[1] = 0x05;
//     tp_buffer[2] = fc_effect.sequence;
//     zd_fb_2_app(tp_buffer, 6);
// }

/**
 * @brief 反馈电机速度
 *
 */
void fb_motor_speed(void)
{
    uint8_t tp_buffer[10];
    u8 len = 0;
    // 0x 01 E9 00，指令前缀
    tp_buffer[len++] = 0x01;
    tp_buffer[len++] = 0xE9;
    tp_buffer[len++] = 0x00;

    tp_buffer[len++] = 0x2F;
    tp_buffer[len++] = 0x07;
    tp_buffer[len++] = fc_effect.base_ins.period;
    zd_fb_2_app(tp_buffer, len);
}

void fb_motor_mode(void)
{
    uint8_t tp_buffer[10];
    u8 len = 0;
    // 0x 01 E9 00，指令前缀
    tp_buffer[len++] = 0x01;
    tp_buffer[len++] = 0xE9;
    tp_buffer[len++] = 0x00;

    tp_buffer[len++] = 0x2F;
    tp_buffer[len++] = 0x06;
    tp_buffer[len++] = fc_effect.base_ins.mode;
    zd_fb_2_app(tp_buffer, len);
}

void parse_zd_data(unsigned char *LedCommand, u8 len)
{
    if (LedCommand[0] == 0x01 && LedCommand[1] == 0x03) {
        // 收到了APP传过来的同步指令
        user_alarm_t alarm[3];

        report_dev_type(0x01);                          // 0x01 灯具类型：RGB
        report_dev_on_off_state(fc_effect.on_off_flag); // 设备总开关状态
        report_brightness(fc_effect.app_b);
        report_speed(fc_effect.app_speed);
        report_led_strip_rgb_len(fc_effect.led_num);
        report_sound_control_sensitivity(fc_effect.music.s); // 灵敏度
        report_meteor_period(fc_effect.meteor_period);
        report_rgb_sequence(fc_effect.sequence);
        report_sound_control_type(fc_effect.music.m_type);
        report_sound_control_mode(fc_effect.music.m);

        // report_meteor_speed(led_strip_white.app_speed);
        // report_meteor_on_off_status(led_strip_white.is_dev_open);

        user_rtc_get_alarm_info(&alarm[0], 0);
        user_rtc_get_alarm_info(&alarm[1], 1);
        user_rtc_get_alarm_info(&alarm[2], 2);
        report_alarm_info(0, alarm[0]);
        report_alarm_info(1, alarm[1]);
        report_alarm_info(2, alarm[2]);
    } else if (LedCommand[0] == 0x01 && LedCommand[1] == 0x01) {
        // 总开关
        u8 on_off_status = LedCommand[2];

        // 总开关，控制RGB幻彩灯和纯白色流星灯
        fc_effect.on_off_flag = on_off_status;
        // led_strip_white.is_dev_open = on_off_status;

        led_strip_rgb_schedule();
        // led_strip_white_schedule();

        report_dev_on_off_state(on_off_status);
        // report_meteor_on_off_status(led_strip_white.is_dev_open);
    } else if (LedCommand[0] == 0x06 && LedCommand[1] == 0x02) {
        // 设置系统时间 小时-分钟-秒-星期
        user_time_t cur_time;

        cur_time.hour = LedCommand[2];
        cur_time.min = LedCommand[3];
        cur_time.sec = LedCommand[4];
        cur_time.weekday = LedCommand[5];

        printf("cur_time.hour == %u\n", (u16)cur_time.hour);
        printf("cur_time.min == %u\n", (u16)cur_time.min);
        printf("cur_time.sec == %u\n", (u16)cur_time.sec);
        printf("cur_time.weekday == %u\n", (u16)cur_time.weekday);

        user_rtc_set_time(&cur_time);
    } else if (LedCommand[0] == 0x05) {
        // 设置闹钟
        u8 alarm_idx;
        user_alarm_t alarm;

        alarm_idx = LedCommand[1];

        alarm.hour = LedCommand[2];
        alarm.min = LedCommand[3];
        alarm.sec = 0; // 通信协议中没有秒，默认为0

        // byte 4，bit7 : 0-关闭闹钟，1-开启闹钟
        alarm.enable = (LedCommand[4] >> 7);
        // byte 5, bit 7, 0：关闭设备，1：开启设备
        alarm.power_on = (LedCommand[5] >> 7);
        alarm.weekday = LedCommand[5] & 0x7F;

        user_rtc_set_alarm(&alarm, alarm_idx);
    }

    // if (fc_effect.on_off_flag)
    // {
    //---------------------------------动态处理-----------------------------------
    if (LedCommand[0] == 0x04 && LedCommand[1] == 0x02 &&
        LedCommand[2] >= 0x07 && LedCommand[2] <= 0x1c) {
        switch (LedCommand[2]) {
        case 0x07: // 3色跳变
            ls_set_color(0, BLUE);
            ls_set_color(1, GREEN);
            ls_set_color(2, RED);
            fc_effect.dream_scene.change_type = MODE_JUMP;
            fc_effect.dream_scene.c_n = 3;
            fc_effect.Now_state = IS_light_scene;
            break;
        case 0x08: // 7色跳变
            ls_set_color(0, BLUE);
            ls_set_color(1, GREEN);
            ls_set_color(2, RED);
            ls_set_color(3, WHITE);
            ls_set_color(4, YELLOW);
            ls_set_color(5, CYAN);
            ls_set_color(6, MAGENTA);
            fc_effect.dream_scene.change_type = MODE_JUMP;
            fc_effect.dream_scene.c_n = 7;
            fc_effect.Now_state = IS_light_scene;
            break;
        case 0x09: // 3色渐变
            ls_set_color(0, BLUE);
            ls_set_color(1, GREEN);
            ls_set_color(2, RED);
            fc_effect.dream_scene.change_type = MODE_MUTIL_C_GRADUAL;
            fc_effect.dream_scene.c_n = 3;
            fc_effect.Now_state = IS_light_scene;
            break;
        case 0x0A: // 七彩渐变
            ls_set_color(0, BLUE);
            ls_set_color(1, GREEN);
            ls_set_color(2, RED);
            ls_set_color(3, WHITE);
            ls_set_color(4, YELLOW);
            ls_set_color(5, CYAN);
            ls_set_color(6, MAGENTA);
            fc_effect.dream_scene.change_type = MODE_MUTIL_C_GRADUAL;
            fc_effect.dream_scene.c_n = 7;
            fc_effect.Now_state = IS_light_scene;
            break;
        case 0x0B:
            ls_set_color(0, RED);
            ls_set_color(1, BLACK);
            fc_effect.dream_scene.change_type = MODE_SINGLE_C_BREATH;
            fc_effect.dream_scene.c_n = 2;
            fc_effect.Now_state = IS_light_scene;
            break;

        case 0x0c:
            ls_set_color(0, BLUE);
            ls_set_color(1, BLACK);
            fc_effect.dream_scene.change_type = MODE_SINGLE_C_BREATH;
            fc_effect.dream_scene.c_n = 2;
            fc_effect.Now_state = IS_light_scene;
            break;
        case 0x0D:
            ls_set_color(0, GREEN);
            ls_set_color(1, BLACK);
            fc_effect.dream_scene.change_type = MODE_SINGLE_C_BREATH;
            fc_effect.dream_scene.c_n = 2;
            fc_effect.Now_state = IS_light_scene;
            break;

        case 0x0E:
            ls_set_color(0, CYAN);
            ls_set_color(1, BLACK);
            fc_effect.dream_scene.change_type = MODE_SINGLE_C_BREATH;
            fc_effect.dream_scene.c_n = 2;
            fc_effect.Now_state = IS_light_scene;
            break;

        case 0x0F:
            ls_set_color(0, YELLOW);
            ls_set_color(1, BLACK);
            fc_effect.dream_scene.change_type = MODE_SINGLE_C_BREATH;
            fc_effect.dream_scene.c_n = 2;
            fc_effect.Now_state = IS_light_scene;
            break;

        case 0x10:
            ls_set_color(0, MAGENTA);
            ls_set_color(1, BLACK);
            fc_effect.dream_scene.change_type = MODE_SINGLE_C_BREATH;
            fc_effect.dream_scene.c_n = 2;
            fc_effect.Now_state = IS_light_scene;
            break;

        case 0x11:
            ls_set_color(0, WHITE);
            ls_set_color(1, BLACK);
            fc_effect.dream_scene.change_type = MODE_SINGLE_C_BREATH;
            fc_effect.dream_scene.c_n = 2;
            fc_effect.Now_state = IS_light_scene;
            break;

        case 0x12:
            ls_set_color(0, RED);
            ls_set_color(1, GREEN);
            fc_effect.dream_scene.change_type = MODE_MUTIL_C_GRADUAL;
            fc_effect.dream_scene.c_n = 2;
            fc_effect.Now_state = IS_light_scene;
            break;

        case 0x13:
            ls_set_color(0, BLUE);
            ls_set_color(1, RED);
            fc_effect.dream_scene.change_type = MODE_MUTIL_C_GRADUAL;
            fc_effect.dream_scene.c_n = 2;
            fc_effect.Now_state = IS_light_scene;
            break;

        case 0x14:
            ls_set_color(0, GREEN);
            ls_set_color(1, BLUE);
            fc_effect.dream_scene.change_type = MODE_MUTIL_C_GRADUAL;
            fc_effect.dream_scene.c_n = 2;
            fc_effect.Now_state = IS_light_scene;
            break;

        case 0x15: // 七色频闪
            ls_set_color(0, BLUE);
            ls_set_color(1, GREEN);
            ls_set_color(2, RED);
            ls_set_color(3, WHITE);
            ls_set_color(4, YELLOW);
            ls_set_color(5, CYAN);
            ls_set_color(6, MAGENTA);

            fc_effect.dream_scene.change_type = MODE_STROBE;
            fc_effect.dream_scene.c_n = 7;
            fc_effect.Now_state = IS_light_scene;

            break;

        case 0x16:
            ls_set_color(0, RED);
            fc_effect.dream_scene.change_type = MODE_STROBE;
            fc_effect.dream_scene.c_n = 1;
            fc_effect.Now_state = IS_light_scene;

            break;

        case 0x17:
            ls_set_color(0, BLUE);
            fc_effect.dream_scene.change_type = MODE_STROBE;
            fc_effect.dream_scene.c_n = 1;
            fc_effect.Now_state = IS_light_scene;

            break;

        case 0x18:
            ls_set_color(0, GREEN);
            fc_effect.dream_scene.change_type = MODE_STROBE;
            fc_effect.dream_scene.c_n = 1;
            fc_effect.Now_state = IS_light_scene;

            break;
        case 0x19:

            ls_set_color(0, CYAN);
            fc_effect.dream_scene.change_type = MODE_STROBE;
            fc_effect.dream_scene.c_n = 1;
            fc_effect.Now_state = IS_light_scene;

            break;

        case 0x1a:

            ls_set_color(0, YELLOW);
            fc_effect.dream_scene.change_type = MODE_STROBE;
            fc_effect.dream_scene.c_n = 1;
            fc_effect.Now_state = IS_light_scene;

            break;
        case 0x1B:

            ls_set_color(0, MAGENTA);
            fc_effect.dream_scene.change_type = MODE_STROBE;
            fc_effect.dream_scene.c_n = 1;
            fc_effect.Now_state = IS_light_scene;

            break;
        case 0x1C:
            ls_set_color(0, WHITE);
            fc_effect.dream_scene.change_type = MODE_STROBE;
            fc_effect.dream_scene.c_n = 1;
            fc_effect.Now_state = IS_light_scene;

            break;
        }

        led_strip_rgb_schedule();
    } else if (LedCommand[0] == 0x04 && LedCommand[1] == 0x01 &&
               LedCommand[2] == 0x1e) {
        // 设置为静态模式，调节静态颜色
        extern void set_static_mode(u8 r, u8 g, u8 b);
        set_static_mode(LedCommand[3], LedCommand[4], LedCommand[5]);
    } else if (LedCommand[0] == 0x04 && LedCommand[1] == 0x03) {
        // 调节亮度
        u8 percent = LedCommand[2];
        if (percent > 100) {
            percent = 100;
        }

        fc_effect.app_b = percent;
        fc_effect.b = (u16)percent * (255 - 25) / 100 + 25;
        WS2812FX_setBrightness(fc_effect.b);

        report_brightness(fc_effect.app_b);
    } else if (LedCommand[0] == 0x04 && LedCommand[1] == 0x04) {
        // 调节速度
        u8 speed = LedCommand[2];

        fc_effect.app_speed = speed;
        fc_effect.dream_scene.speed = 500 - ((u32)500 * speed / 100);
        if (fc_effect.dream_scene.speed <= get_max_speed()) {
            fc_effect.dream_scene.speed = get_max_speed();
        }

        led_strip_rgb_schedule();
        report_speed(fc_effect.app_speed);
    } else if (LedCommand[0] == 0x04 && LedCommand[1] == 0x05) {
        // 更改RGB接口
        u8 sequence = LedCommand[2];
        if (sequence < ARRAY_SIZE(rgb_sequence_map)) {
            fc_effect.sequence = rgb_sequence_map[sequence];
            WS2812FX_init(fc_effect.led_num, fc_effect.sequence);
            fc_effect.custom_mode_index = 2; // 调整RGB顺序效果
            fc_effect.Now_state = ACT_CUSTOM;
            led_strip_rgb_schedule(); // 重新开始跑动画
        }
    } else if (LedCommand[0] == 0x04 && LedCommand[1] == 0x08) {
        // 调节 灯带长度 (RGB灯珠数量)
        u16 len = LedCommand[2] << 8 | LedCommand[3];
        if (len > 2048) {
            len = 2048;
        }
        fc_effect.led_num = len;
        WS2812FX_init(fc_effect.led_num, fc_effect.sequence);
        // 更新灯带对应的动画速度
        if (get_max_speed() > fc_effect.dream_scene.speed) {
            fc_effect.dream_scene.speed = get_max_speed();
        }

        led_strip_rgb_schedule(); // 重新开始跑动画

        report_led_strip_rgb_len(fc_effect.led_num);
    } else if (LedCommand[0] == 0x06 && LedCommand[1] == 0x06) {
        // 外麦声控模式

        fc_effect.music.m = LedCommand[2];
        fc_effect.Now_state = IS_light_music;
        led_strip_rgb_schedule();
        report_sound_control_mode(fc_effect.music.m);
    } else if (LedCommand[0] == 0x2F && LedCommand[1] == 0x05) {
        // 设置灵敏度
        u8 sensitivity = LedCommand[2];
        fc_effect.music.s = sensitivity;
        led_strip_white.sensitivity = sensitivity;
        report_sound_control_sensitivity(fc_effect.music.s);
    } else if (LedCommand[0] == 0x06 && LedCommand[1] == 0x04) {
        // 手机音乐律动
        set_static_mode(LedCommand[2], LedCommand[3], LedCommand[4]);

        u8 percent = LedCommand[5];
        if (percent > 100) {
            percent = 100;
        }

        fc_effect.app_b = percent;
        fc_effect.b = (u16)percent * (255 - 25) / 100 + 25;
        WS2812FX_setBrightness(fc_effect.b);
    }
    if (LedCommand[0] == 0x2F && LedCommand[1] == 0x00) {
        // 纯白色流星灯 流星模式
        u8 mode_index = LedCommand[2];
        if (mode_index >= 1 && mode_index <= 16) {
            led_strip_white.mode_index = mode_index;
        }

        led_strip_white_schedule();
    } else if (LedCommand[0] == 0x2F && LedCommand[1] == 0x01) {
        // 流星速度
        u8 speed = LedCommand[2];

        led_strip_white.app_speed = speed;
        // 最后得到的数值会在 30 ~ 330，数值越小，速度越快
        led_strip_white.speed =
            300 * (100 - led_strip_white.app_speed + 10) / 100;
#if USER_DEBUG_ENABLE
        // printf("led_strip_white.speed == %u\n", (u16)led_strip_white.speed);
#endif
        led_strip_white_schedule();
        report_meteor_speed(led_strip_white.app_speed);
    } else if (LedCommand[0] == 0x2F && LedCommand[1] == 0x02) {
        // 流星灯开关
        u8 on_off = LedCommand[2];
        if (on_off == 0x01) {
            led_strip_white.is_dev_open = 1;
        } else {
            led_strip_white.is_dev_open = 0;
        }

        led_strip_white_schedule();
        report_meteor_on_off_status(led_strip_white.is_dev_open);
    } else if (LedCommand[0] == 0x2F && LedCommand[1] == 0x03) {
        // 流星周期(时间间隔)
        u8 period = LedCommand[2];

        if (period >= 2 && period <= 20) {
            fc_effect.meteor_period = period;
            fc_effect.period_cnt = 0; // 清空计数值

            led_strip_white.period = period;
            led_strip_white.period_cnt = (u16)led_strip_white.period * 1000;
        }

        report_meteor_period(fc_effect.meteor_period);
    } else if (LedCommand[0] == 0x06 && LedCommand[1] == 0x07) {
        // 设置为手机麦或者外麦
        fc_effect.music.m_type = LedCommand[2];
        report_sound_control_type(fc_effect.music.m_type);
    }
    // }
}

/* APP数据解析入口函数 */
void parse_led_strip_data(u8 *pBuf, u8 len)
{
    /* 协议解析 */
    parse_zd_data(pBuf, len);
    dp_extract_data_handle(pBuf); // 额外的数据包解析
    user_data_save_enable();
}
