#include "app_msg_handle.h"
#include "typedef.h"
#include "app_msg_typedef.h"
#include "os_cpu.h"
#include <string.h>

#include "led_strand_effect.h"
#include "Adafruit_NeoPixel.h" //

#include "user_rtc.h"
#include "user_thread_communication.h"
#include "user_config.h"

static const u8 rgb_sequence_map[6] = {
    NEO_RGB, NEO_RBG, NEO_GRB, NEO_GBR, NEO_BRG, NEO_BGR,
};

/*
    指令分发表（表驱动）
    通过摘要表 + 统一签名的方式，组织指令和指令处理函数
    ----------------------------------------------------------------------
    新增一条 app 指令时只需要两步：
        1. 写一个统一签名的处理函数：void (*)(const u8 *payload, u16 len)
        2. 在 app_msg_cmd_table[] 中增加一行

    - cmd     : 用于匹配的指令字节，从 payload 的第 0 字节开始比对
    - cmd_len : 参与匹配的字节数（1 或 2）
    - min_len : payload 的最小长度（含 cmd 占用的字节），长度不足视为不匹配
    - handler : 匹配成功后调用的处理函数

    注意：表是从上往下顺序匹配的，匹配到第一条即返回，
          因此「指令字节更长」的条目必须排在「指令字节更短」的条目前面。
    ----------------------------------------------------------------------
*/

// 各指令对应的处理函数
static void app_msg_color_light_handle(const u8 *payload, u16 len);
static void app_msg_syn_handle(const u8 *payload, u16 len);
static void app_msg_power_switch_handle(const u8 *payload, u16 len);
static void app_msg_set_time_handle(const u8 *payload, u16 len);
static void app_msg_set_alarm_handle(const u8 *payload, u16 len);
static void app_msg_set_rgb_sequence_handle(const u8 *payload, u16 len);
static void app_msg_set_led_num_handle(const u8 *payload, u16 len);
static void app_msg_set_sound_control_mode_handle(const u8 *payload, u16 len);
static void app_msg_set_sound_control_sensitivity_handle(const u8 *payload,
                                                         u16 len);
static void app_msg_set_sound_control_by_phone_handle(const u8 *payload,
                                                      u16 len);

typedef void (*app_msg_cmd_handler_t)(const u8 *payload, u16 len);

typedef struct
{
    const u8 cmd[2];               // 用于匹配的指令字节
    u8 cmd_len;                    // 参与匹配的字节数（1~2）
    u8 min_len;                    // payload 的最小长度（含指令字节）
    app_msg_cmd_handler_t handler; // 指令对应的处理函数
} app_msg_cmd_entry_t;

static const app_msg_cmd_entry_t app_msg_cmd_table[] = {
    // 幻彩灯动画参数：7F FF ...
    {{0x7F, 0xFF}, 2, 15, app_msg_color_light_handle},

    // 同步指令：01 03
    {{0x01, 0x03}, 2, 2, app_msg_syn_handle},

    // 总开关：01 01 xx
    {{0x01, 0x01}, 2, 3, app_msg_power_switch_handle},

    // 设置系统时间：06 02 时 分 秒 星期
    {{0x06, 0x02}, 2, 6, app_msg_set_time_handle},

    // 设置闹钟：05 索引 时 分 (开关<<7)|保留 (开关<<7)|星期
    // 只比对第 1 个字节，第 2 个字节是闹钟索引
    {{0x05, 0x00}, 1, 6, app_msg_set_alarm_handle},

    /*
        更改RGB线序
        04 05 线序
    */
    {{0x04, 0x05}, 2, 3, app_msg_set_rgb_sequence_handle},

    /*
        调节灯带长度
        04 08 长度高8位 长度低8位
    */
    {{0x04, 0x08}, 2, 4, app_msg_set_led_num_handle},

    /*
        外麦声控模式
        06 06 模式
    */
    {{0x06, 0x06}, 2, 3, app_msg_set_sound_control_mode_handle},

    /*
        设置灵敏度
        2F 05 灵敏度
    */
    {{0x2F, 0x05}, 2, 3, app_msg_set_sound_control_sensitivity_handle},

    /*
        手机音乐律动模式
        06    04    R    G    B    亮度百分比
    */
    {{0x06, 0x04}, 2, 6, app_msg_set_sound_control_by_phone_handle},
};

#define APP_MSG_CMD_TABLE_SIZE                                                 \
    (sizeof(app_msg_cmd_table) / sizeof(app_msg_cmd_table[0]))

// 只用在当前源文件，存放当前app传递过来的动画数据
static volatile app_msg_anim_info_t app_msg_anim_info = {0};

/**
 * @brief 幻彩灯动画参数（0x7F 0xFF ...）对应的处理函数
 *
 * @param payload 指令数据（不含 3 字节指令前缀），首字节为 0x7F 0xFF
 * @param len     payload 的长度
 */
static void app_msg_color_light_handle(const u8 *payload, u16 len)
{
    u8 i = 0;
    u8 color_buf_idx = 0;
    app_msg_anim_info_t new_info = {0};

    if (len < 15) {
        return;
    }

    new_info.format_head = (uint16_t)payload[i++] << 8;
    new_info.format_head |= payload[i++];
    new_info.mode_idx = payload[i++];
    new_info.anim_dir = payload[i++];
    new_info.anim_speed = payload[i++];
    new_info.anim_brightness = payload[i++];
    new_info.seg_size = payload[i++];
    new_info.seg_num = payload[i++];

    new_info.background_color_r = payload[i++];
    new_info.background_color_g = payload[i++];
    new_info.background_color_b = payload[i++];
    new_info.color_num = payload[i++];

    // 颜色数据长度超出了内部缓冲区大小，或者是颜色数据长度小于一个颜色对应的长度
    if (new_info.color_num > APP_MSG_COLOR_NUM_MAX ||
        len < ((15 - 3) + (new_info.color_num * 3))) {
        return;
    }

    for (; (i < len && color_buf_idx < (new_info.color_num * 3));
         i++, color_buf_idx++) {
        new_info.color_buf[color_buf_idx] = payload[i];
    }

    OS_ENTER_CRITICAL();
    // 更新接收到的数据
    memcpy((void *)&app_msg_anim_info, &new_info, sizeof(new_info));
    OS_EXIT_CRITICAL();

#if USER_DEBUG_ENABLE
    // 打印接收到的数据
#endif

    // 通知其他模块，幻彩灯模式发生了变化
    user_thread_communication_send_msg(
        LED_STRIP_RGB_TASK_NAME,
        USER_THREAD_COMM_MSG_TYPE_APP_MSG_ANIM_INFO_UPDATE);
}

void app_msg_anim_info_get(app_msg_anim_info_t *info)
{
    if (info == NULL) {
        return;
    }

    OS_ENTER_CRITICAL();
    // 获取接收到的数据
    memcpy(info, (const void *)&app_msg_anim_info, sizeof(*info));
    OS_EXIT_CRITICAL();
}

/**
 * @brief app 同步指令（0x01 0x03）对应的处理函数
 *
 * @param payload 指令数据（不含指令前缀）
 * @param len     payload 的长度
 */
static void app_msg_syn_handle(const u8 *payload, u16 len)
{
    user_alarm_t alarm[3] = {0};

    // 同步指令不带参数，加void修饰，防止编译器报错
    (void)payload;
    (void)len;

    user_ble_notify_dev_type(0x01);                     // 0x01 灯具类型：RGB
    user_ble_notify_dev_pwr_sta(fc_effect.on_off_flag); // 设备总开关状态
    user_ble_notify_brightness(fc_effect.app_b);
    user_ble_notify_speed(fc_effect.app_speed);
    user_ble_notify_led_strip_rgb_len(fc_effect.led_num);
    user_ble_notify_sound_control_sensitivity(fc_effect.music.s); // 灵敏度
    user_ble_notify_meteor_period(fc_effect.meteor_period);
    user_ble_notify_rgb_sequence(fc_effect.sequence);
    user_ble_notify_sound_control_type(fc_effect.music.m_type);
    user_ble_notify_sound_control_mode(fc_effect.music.m);

    user_rtc_get_alarm_info(&alarm[0], 0);
    user_rtc_get_alarm_info(&alarm[1], 1);
    user_rtc_get_alarm_info(&alarm[2], 2);
    user_ble_notify_alarm_info(0, alarm[0]);
    user_ble_notify_alarm_info(1, alarm[1]);
    user_ble_notify_alarm_info(2, alarm[2]);

    // USER_TO_DO app发送同步指令时，要整理当前动画的信息，并返回给app
    // app_msg_anim_info_t -> app
}

/**
 * @brief 总开关（0x01 0x01 xx）对应的处理函数
 *
 * @param payload 指令数据（不含指令前缀）：01 01 开关状态
 * @param len     payload 的长度
 */
static void app_msg_power_switch_handle(const u8 *payload, u16 len)
{
    (void)len;

    fc_effect.on_off_flag = payload[2];
    led_strip_rgb_schedule();
    user_ble_notify_dev_pwr_sta(fc_effect.on_off_flag);
}

/**
 * @brief 设置系统时间（0x06 0x02 时 分 秒 星期）对应的处理函数
 *
 * @param payload 指令数据（不含指令前缀）：06 02 时 分 秒 星期
 * @param len     payload 的长度
 */
static void app_msg_set_time_handle(const u8 *payload, u16 len)
{
    user_time_t cur_time;

    (void)len;

    cur_time.hour = payload[2];
    cur_time.min = payload[3];
    cur_time.sec = payload[4];
    cur_time.weekday = payload[5];

    printf("cur_time.hour == %u\n", (u16)cur_time.hour);
    printf("cur_time.min == %u\n", (u16)cur_time.min);
    printf("cur_time.sec == %u\n", (u16)cur_time.sec);
    printf("cur_time.weekday == %u\n", (u16)cur_time.weekday);

    user_rtc_set_time(&cur_time);
}

/**
 * @brief 设置闹钟（0x05 索引 时 分 开关 星期）对应的处理函数
 *
 * @param payload 指令数据（不含指令前缀）：
 *                05 闹钟索引 时 分 (开关闹钟<<7) (开关设备<<7)|星期
 * @param len     payload 的长度
 */
static void app_msg_set_alarm_handle(const u8 *payload, u16 len)
{
    u8 alarm_idx;
    user_alarm_t alarm;

    (void)len;

    alarm_idx = payload[1];

    alarm.hour = payload[2];
    alarm.min = payload[3];
    alarm.sec = 0; // 通信协议中没有秒，默认为0

    // byte 4，bit7 : 0-关闭闹钟，1-开启闹钟
    alarm.enable = payload[4] >> 7;
    // byte 5, bit 7, 0：关闭设备，1：开启设备
    alarm.power_on = payload[5] >> 7;
    alarm.weekday = payload[5] & 0x7F;

    user_rtc_set_alarm(&alarm, alarm_idx);
}

/**
 * @brief 设置RGB线序
 *
 * @param payload 指令数据（不含指令前缀）
 *                
 * @param len     payload 的长度
 */
static void app_msg_set_rgb_sequence_handle(const u8 *payload, u16 len)
{
    u8 sequence = payload[2];
    (void)len;

    if (sequence < ARRAY_SIZE(rgb_sequence_map)) {
        fc_effect.sequence = rgb_sequence_map[sequence];
        WS2812FX_init(fc_effect.led_num, fc_effect.sequence);
        fc_effect.custom_mode_index = 2; // 调整RGB顺序效果
        fc_effect.Now_state = ACT_CUSTOM;
        led_strip_rgb_schedule(); // 重新开始跑动画
    }
}

static void app_msg_set_led_num_handle(const u8 *payload, u16 len)
{
    fc_effect.led_num = (u16)payload[2] << 8 | payload[3];
    (void)len;
    if (fc_effect.led_num > 2048) {
        fc_effect.led_num = 2048;
    }

    WS2812FX_init(fc_effect.led_num, fc_effect.sequence);
    // 更新灯带对应的动画速度
    if (get_max_speed() > fc_effect.dream_scene.speed) {
        fc_effect.dream_scene.speed = get_max_speed();
    }

    led_strip_rgb_schedule(); // 重新开始跑动画
    user_ble_notify_led_strip_rgb_len(fc_effect.led_num);
}

static void app_msg_set_sound_control_mode_handle(const u8 *payload, u16 len)
{
    fc_effect.music.m = payload[2];
    (void)len;
    fc_effect.Now_state = IS_light_music;
    led_strip_rgb_schedule();
    user_ble_notify_sound_control_mode(fc_effect.music.m);
}

static void app_msg_set_sound_control_sensitivity_handle(const u8 *payload,
                                                         u16 len)
{
    fc_effect.music.s = payload[2];
    (void)len;
    user_ble_notify_sound_control_sensitivity(fc_effect.music.s);
}

static void app_msg_set_sound_control_by_phone_handle(const u8 *payload,
                                                      u16 len)
{
    u8 brightness_percent;
    set_static_mode(payload[2], payload[3], payload[4]);
    (void)len;

    brightness_percent = payload[5];
    if (brightness_percent > 100) {
        brightness_percent = 100;
    }

    fc_effect.app_b = brightness_percent;
    fc_effect.b = (u16)brightness_percent * (255 - 25) / 100 + 25;
    WS2812FX_setBrightness(fc_effect.b);
}

/*
    指令投递缓冲区
    ----------------------------------------------------------------------
    蓝牙写回调（btstack 任务）里只做一次拷贝 + 发一条消息，
    指令的解析和执行都放到 app_msg_handle 任务里完成，
    避免耗时操作长时间占用蓝牙回调。

    写入方：app_msg_post()          —— 蓝牙写回调
    读取方：app_msg_post_fetch()    —— app_msg_handle 任务

    注意：缓冲区满时，新指令会覆盖最旧的一条指令。
    ----------------------------------------------------------------------
*/
#define APP_MSG_POST_BUF_NUM 8 // 投递缓冲区可缓存的指令条数

static volatile u8 app_msg_post_buf[APP_MSG_POST_BUF_NUM]
                                   [APP_MSG_POST_BUF_MAX_LEN];
static volatile u16 app_msg_post_len[APP_MSG_POST_BUF_NUM];
static volatile u8 app_msg_post_wr = 0;  // 投递缓冲区的写索引
static volatile u8 app_msg_post_cnt = 0; // 待处理的指令条数

/**
 * @brief 投递一条 app 指令（在蓝牙写回调里调用）
 *
 * @note  只做一次拷贝 + 发一条消息，不做任何解析和执行，
 *        保证蓝牙回调尽快返回
 *
 * @param buf 收到的原始指令（含指令前缀）
 * @param len 指令的长度
 */
void app_msg_post(uint8_t *buf, uint16_t len)
{
    u8 idx;

    if (buf == NULL || len == 0 || len > APP_MSG_POST_BUF_MAX_LEN) {
        return;
    }

    OS_ENTER_CRITICAL();
    idx = app_msg_post_wr;

    // 把指令拷贝到投递缓冲区
    memcpy((void *)app_msg_post_buf[idx], buf, len);
    app_msg_post_len[idx] = len;

    // 先偏移索引，再增加指令条数
    app_msg_post_wr++;
    if (app_msg_post_wr >= APP_MSG_POST_BUF_NUM) {
        app_msg_post_wr = 0;
    }
    if (app_msg_post_cnt < APP_MSG_POST_BUF_NUM) {
        app_msg_post_cnt++;
    }
    OS_EXIT_CRITICAL();

    // 通知 app_msg_handle 任务，有新的指令需要处理
    user_thread_communication_send_msg(LED_STRIP_RGB_TASK_NAME,
                                       USER_THREAD_COMM_MSG_TYPE_APP_MSG_RAW);
}

/**
 * @brief 从投递缓冲区取出一条待处理的指令（在 app_msg_handle 任务里调用）
 *
 * @param buf 输出参数，用于存放取出的指令
 * @param len 输出参数，用于返回指令的长度
 * @return u8 1：取到指令；0：没有待处理的指令
 */
u8 app_msg_post_fetch(uint8_t *buf, uint16_t *len)
{
    u8 idx;

    if (buf == NULL || len == NULL) {
        return 0;
    }

    OS_ENTER_CRITICAL();
    if (0 == app_msg_post_cnt) {
        OS_EXIT_CRITICAL();
        return 0;
    }

    // 计算最旧一条指令的存放位置，并取出
    idx = (u8)((app_msg_post_wr + APP_MSG_POST_BUF_NUM - app_msg_post_cnt) %
               APP_MSG_POST_BUF_NUM);
    memcpy(buf, (const void *)app_msg_post_buf[idx], app_msg_post_len[idx]);
    *len = app_msg_post_len[idx];

    app_msg_post_cnt--;
    OS_EXIT_CRITICAL();

    return 1;
}

void app_msg_handle(uint8_t *buf, uint16_t len)
{
    const u8 *payload;
    u16 payload_len;
    u8 i;
    const app_msg_cmd_entry_t *entry;

    if (len < INSTRUCTION_PREFIX_LEN ||
        (0 != memcmp(instruction_prefix, buf, INSTRUCTION_PREFIX_LEN))) {
        /*
			- 接收到的指令长度小于 INSTRUCTION_PREFIX_LEN
			- 接收到的指令前缀不匹配

			直接返回
		*/
        return;
    }

    payload =
        &buf[INSTRUCTION_PREFIX_LEN]; // 跳过指令的前缀，直接定位到指令数据
    payload_len = len - INSTRUCTION_PREFIX_LEN; // 计算指令数据的长度

    /*
        顺序遍历指令分发表：
        长度不足或指令字节不匹配的条目直接跳过，
        匹配到第一条就调用它的处理函数并返回。
    */
    for (i = 0; i < APP_MSG_CMD_TABLE_SIZE; i++) {
        entry = &app_msg_cmd_table[i];

        if (payload_len < entry->min_len ||
            0 != memcmp(payload, entry->cmd, entry->cmd_len)) {
            continue;
        }

        entry->handler(payload, payload_len);
        return;
    }

#if USER_DEBUG_ENABLE
    // 没有匹配到任何指令，打印出来方便调试协议
    if (payload_len > 0) {
        printf("app_msg: unknown cmd 0x%02x 0x%02x, payload_len %u\n",
               payload[0], (payload_len > 1) ? payload[1] : 0,
               (u16)payload_len);
    }
#endif
}