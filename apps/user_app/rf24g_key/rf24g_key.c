#include "rf24g_key.h"
#include "led_strand_effect.h"
#include "save_flash.h" // 包含读写flash的接口
#include "ws2812fx_effect.h"
#include "led_strip_rgb_app.h"
#include "report.h"

#include "user_include.h"

#if (RF24GKEY_ENABLE)

/*
    用 AK803-SOP16 写的遥控器
*/
const u8 rf24g_key_type_28keys_table[][RF34G_KEY_EVENT_MAX + 1] = {
    {RF24G_KEY_VAL_R1C1, RF24G_28_KEY_EVENT_R1C1_PRESS, RF24G_28_KEY_EVENT_R1C1_CLICK, RF24G_28_KEY_EVENT_R1C1_LONG, RF24G_28_KEY_EVENT_R1C1_HOLD, RF24G_28_KEY_EVENT_R1C1_LOOSE},
    {RF24G_KEY_VAL_R1C2, RF24G_28_KEY_EVENT_R1C2_PRESS, RF24G_28_KEY_EVENT_R1C2_CLICK, RF24G_28_KEY_EVENT_R1C2_LONG, RF24G_28_KEY_EVENT_R1C2_HOLD, RF24G_28_KEY_EVENT_R1C2_LOOSE},
    {RF24G_KEY_VAL_R1C3, RF24G_28_KEY_EVENT_R1C3_PRESS, RF24G_28_KEY_EVENT_R1C3_CLICK, RF24G_28_KEY_EVENT_R1C3_LONG, RF24G_28_KEY_EVENT_R1C3_HOLD, RF24G_28_KEY_EVENT_R1C3_LOOSE},
    {RF24G_KEY_VAL_R1C4, RF24G_28_KEY_EVENT_R1C4_PRESS, RF24G_28_KEY_EVENT_R1C4_CLICK, RF24G_28_KEY_EVENT_R1C4_LONG, RF24G_28_KEY_EVENT_R1C4_HOLD, RF24G_28_KEY_EVENT_R1C4_LOOSE},

    {RF24G_KEY_VAL_R2C1, RF24G_28_KEY_EVENT_R2C1_PRESS, RF24G_28_KEY_EVENT_R2C1_CLICK, RF24G_28_KEY_EVENT_R2C1_LONG, RF24G_28_KEY_EVENT_R2C1_HOLD, RF24G_28_KEY_EVENT_R2C1_LOOSE},
    {RF24G_KEY_VAL_R2C2, RF24G_28_KEY_EVENT_R2C2_PRESS, RF24G_28_KEY_EVENT_R2C2_CLICK, RF24G_28_KEY_EVENT_R2C2_LONG, RF24G_28_KEY_EVENT_R2C2_HOLD, RF24G_28_KEY_EVENT_R2C2_LOOSE},
    {RF24G_KEY_VAL_R2C3, RF24G_28_KEY_EVENT_R2C3_PRESS, RF24G_28_KEY_EVENT_R2C3_CLICK, RF24G_28_KEY_EVENT_R2C3_LONG, RF24G_28_KEY_EVENT_R2C3_HOLD, RF24G_28_KEY_EVENT_R2C3_LOOSE},
    {RF24G_KEY_VAL_R2C4, RF24G_28_KEY_EVENT_R2C4_PRESS, RF24G_28_KEY_EVENT_R2C4_CLICK, RF24G_28_KEY_EVENT_R2C4_LONG, RF24G_28_KEY_EVENT_R2C4_HOLD, RF24G_28_KEY_EVENT_R2C4_LOOSE},

    {RF24G_KEY_VAL_R3C1, RF24G_28_KEY_EVENT_R3C1_PRESS, RF24G_28_KEY_EVENT_R3C1_CLICK, RF24G_28_KEY_EVENT_R3C1_LONG, RF24G_28_KEY_EVENT_R3C1_HOLD, RF24G_28_KEY_EVENT_R3C1_LOOSE},
    {RF24G_KEY_VAL_R3C2, RF24G_28_KEY_EVENT_R3C2_PRESS, RF24G_28_KEY_EVENT_R3C2_CLICK, RF24G_28_KEY_EVENT_R3C2_LONG, RF24G_28_KEY_EVENT_R3C2_HOLD, RF24G_28_KEY_EVENT_R3C2_LOOSE},
    {RF24G_KEY_VAL_R3C3, RF24G_28_KEY_EVENT_R3C3_PRESS, RF24G_28_KEY_EVENT_R3C3_CLICK, RF24G_28_KEY_EVENT_R3C3_LONG, RF24G_28_KEY_EVENT_R3C3_HOLD, RF24G_28_KEY_EVENT_R3C3_LOOSE},
    {RF24G_KEY_VAL_R3C4, RF24G_28_KEY_EVENT_R3C4_PRESS, RF24G_28_KEY_EVENT_R3C4_CLICK, RF24G_28_KEY_EVENT_R3C4_LONG, RF24G_28_KEY_EVENT_R3C4_HOLD, RF24G_28_KEY_EVENT_R3C4_LOOSE},

    {RF24G_KEY_VAL_R4C1, RF24G_28_KEY_EVENT_R4C1_PRESS, RF24G_28_KEY_EVENT_R4C1_CLICK, RF24G_28_KEY_EVENT_R4C1_LONG, RF24G_28_KEY_EVENT_R4C1_HOLD, RF24G_28_KEY_EVENT_R4C1_LOOSE},
    {RF24G_KEY_VAL_R4C2, RF24G_28_KEY_EVENT_R4C2_PRESS, RF24G_28_KEY_EVENT_R4C2_CLICK, RF24G_28_KEY_EVENT_R4C2_LONG, RF24G_28_KEY_EVENT_R4C2_HOLD, RF24G_28_KEY_EVENT_R4C2_LOOSE},
    {RF24G_KEY_VAL_R4C3, RF24G_28_KEY_EVENT_R4C3_PRESS, RF24G_28_KEY_EVENT_R4C3_CLICK, RF24G_28_KEY_EVENT_R4C3_LONG, RF24G_28_KEY_EVENT_R4C3_HOLD, RF24G_28_KEY_EVENT_R4C3_LOOSE},
    {RF24G_KEY_VAL_R4C4, RF24G_28_KEY_EVENT_R4C4_PRESS, RF24G_28_KEY_EVENT_R4C4_CLICK, RF24G_28_KEY_EVENT_R4C4_LONG, RF24G_28_KEY_EVENT_R4C4_HOLD, RF24G_28_KEY_EVENT_R4C4_LOOSE},

    {RF24G_KEY_VAL_R5C1, RF24G_28_KEY_EVENT_R5C1_PRESS, RF24G_28_KEY_EVENT_R5C1_CLICK, RF24G_28_KEY_EVENT_R5C1_LONG, RF24G_28_KEY_EVENT_R5C1_HOLD, RF24G_28_KEY_EVENT_R5C1_LOOSE},
    {RF24G_KEY_VAL_R5C2, RF24G_28_KEY_EVENT_R5C2_PRESS, RF24G_28_KEY_EVENT_R5C2_CLICK, RF24G_28_KEY_EVENT_R5C2_LONG, RF24G_28_KEY_EVENT_R5C2_HOLD, RF24G_28_KEY_EVENT_R5C2_LOOSE},
    {RF24G_KEY_VAL_R5C3, RF24G_28_KEY_EVENT_R5C3_PRESS, RF24G_28_KEY_EVENT_R5C3_CLICK, RF24G_28_KEY_EVENT_R5C3_LONG, RF24G_28_KEY_EVENT_R5C3_HOLD, RF24G_28_KEY_EVENT_R5C3_LOOSE},
    {RF24G_KEY_VAL_R5C4, RF24G_28_KEY_EVENT_R5C4_PRESS, RF24G_28_KEY_EVENT_R5C4_CLICK, RF24G_28_KEY_EVENT_R5C4_LONG, RF24G_28_KEY_EVENT_R5C4_HOLD, RF24G_28_KEY_EVENT_R5C4_LOOSE},

    {RF24G_KEY_VAL_R6C1, RF24G_28_KEY_EVENT_R6C1_PRESS, RF24G_28_KEY_EVENT_R6C1_CLICK, RF24G_28_KEY_EVENT_R6C1_LONG, RF24G_28_KEY_EVENT_R6C1_HOLD, RF24G_28_KEY_EVENT_R6C1_LOOSE},
    {RF24G_KEY_VAL_R6C2, RF24G_28_KEY_EVENT_R6C2_PRESS, RF24G_28_KEY_EVENT_R6C2_CLICK, RF24G_28_KEY_EVENT_R6C2_LONG, RF24G_28_KEY_EVENT_R6C2_HOLD, RF24G_28_KEY_EVENT_R6C2_LOOSE},
    {RF24G_KEY_VAL_R6C3, RF24G_28_KEY_EVENT_R6C3_PRESS, RF24G_28_KEY_EVENT_R6C3_CLICK, RF24G_28_KEY_EVENT_R6C3_LONG, RF24G_28_KEY_EVENT_R6C3_HOLD, RF24G_28_KEY_EVENT_R6C3_LOOSE},
    {RF24G_KEY_VAL_R6C4, RF24G_28_KEY_EVENT_R6C4_PRESS, RF24G_28_KEY_EVENT_R6C4_CLICK, RF24G_28_KEY_EVENT_R6C4_LONG, RF24G_28_KEY_EVENT_R6C4_HOLD, RF24G_28_KEY_EVENT_R6C4_LOOSE},

    {RF24G_KEY_VAL_R7C1, RF24G_28_KEY_EVENT_R7C1_PRESS, RF24G_28_KEY_EVENT_R7C1_CLICK, RF24G_28_KEY_EVENT_R7C1_LONG, RF24G_28_KEY_EVENT_R7C1_HOLD, RF24G_28_KEY_EVENT_R7C1_LOOSE},
    {RF24G_KEY_VAL_R7C2, RF24G_28_KEY_EVENT_R7C2_PRESS, RF24G_28_KEY_EVENT_R7C2_CLICK, RF24G_28_KEY_EVENT_R7C2_LONG, RF24G_28_KEY_EVENT_R7C2_HOLD, RF24G_28_KEY_EVENT_R7C2_LOOSE},
    {RF24G_KEY_VAL_R7C3, RF24G_28_KEY_EVENT_R7C3_PRESS, RF24G_28_KEY_EVENT_R7C3_CLICK, RF24G_28_KEY_EVENT_R7C3_LONG, RF24G_28_KEY_EVENT_R7C3_HOLD, RF24G_28_KEY_EVENT_R7C3_LOOSE},
    {RF24G_KEY_VAL_R7C4, RF24G_28_KEY_EVENT_R7C4_PRESS, RF24G_28_KEY_EVENT_R7C4_CLICK, RF24G_28_KEY_EVENT_R7C4_LONG, RF24G_28_KEY_EVENT_R7C4_HOLD, RF24G_28_KEY_EVENT_R7C4_LOOSE},
};

volatile u8 rf24g_key_driver_event = 0; // 由 key_driver_scan() 更新
volatile u8 rf24g_key_driver_value = 0; // 由 key_driver_scan() 更新

static volatile u8 rf24g_rx_flag = 0;       // 是否收到了新的数据
volatile rf24g_recv_info_t rf24g_recv_info; // 存放接收到的数据包
volatile u8 chromatic_circle_val = 0;       // 存放色环按键对应的数值，范围：0x00~0xFF
volatile u8 rf24g_key_val = NO_KEY;         // 存放按键键值

static u8 rf24g_get_key_value(void); // 获取按键键值的函数声明
volatile struct key_driver_para rf24g_scan_para = {
    .scan_time = RF24G_KEY_SCAN_TIME_MS,                                                     // 按键扫描频率, 单位: ms
    .last_key = NO_KEY,                                                                      // 上一次get_value按键值, 初始化为NO_KEY;
    .filter_time = RF24G_KEY_SCAN_FILTER_TIME_MS,                                            // 按键消抖延时;
    .long_time = RF24G_KEY_LONG_TIME_MS / RF24G_KEY_SCAN_TIME_MS,                            // 按键判定长按数量
    .hold_time = (RF24G_KEY_LONG_TIME_MS + RF24G_KEY_HOLD_TIME_MS) / RF24G_KEY_SCAN_TIME_MS, // 按键判定HOLD数量
    .click_delay_time = RF24G_KEY_SCAN_CLICK_DELAY_TIME_MS,                                  // 按键被抬起后等待连击延时数量
    .key_type = KEY_DRIVER_TYPE_RF24GKEY,
    .get_value = rf24g_get_key_value,
};

// 底层按键扫描，由 __resolve_adv_report() 调用
void rf24g_scan(u8 *recv_buff)
{
    rf24g_recv_info_t *p = (rf24g_recv_info_t *)recv_buff;
    if (p->header1 == REMOTE_TYPE_28KEY_HEADER_1 &&
        p->header2 == REMOTE_TYPE_28KEY_HEADER_2)
    {
#if USER_DEBUG_ENABLE
        // printf_buf(recv_buff, sizeof(rf24g_recv_info_t)); // 打印接收到的数据包
#endif

        rf24g_key_val = p->key_val;
        rf24g_rx_flag = 1;
    }
}

static u8 rf24g_get_key_value(void)
{
    u8 key_value = 0;
    static u16 time_out_cnt = 0;  // 加入超时，防止丢包（超时时间与按键扫描时间有关）
    static u8 last_key_value = 0; // 上一次按键键值，在超时时间内返回上一次按键键值

    if (rf24g_rx_flag == 1) // 收到2.4G广播
    {
        rf24g_rx_flag = 0;

        key_value = rf24g_key_val;

        /*
            2.4G接收可能会丢失100~200ms的数据包（响应会慢一些）
            值 == 20，10ms调用一次该函数，这里填充200ms的超时值
        */
        time_out_cnt = 20;
        // time_out_cnt = 5;

        last_key_value = key_value;
        return key_value;
    }

    if (time_out_cnt != 0)
    {
        time_out_cnt--;
        return last_key_value;
    }

    return NO_KEY;
}

// 根据按键键值和key_driver_scan得到的事件值，转换为对应的按键事件
u8 rf24g_convert_key_event(u8 key_value, u8 key_driver_event)
{
    // 将key_driver_scan得到的key_event转换成自定义的key_event对应的索引
    // 索引对应 rf24g_key_event_table[][] 中的索引
    u8 key_event_index = 0; // 默认为0，0对应无效索引
    if (KEY_EVENT_PRESS == key_driver_event)
    {
        key_event_index = 1;
    }
    else if (KEY_EVENT_CLICK == key_driver_event)
    {
        key_event_index = 2;
    }
    else if (KEY_EVENT_LONG == key_driver_event)
    {
        key_event_index = 3;
    }
    else if (KEY_EVENT_HOLD == key_driver_event)
    {
        key_event_index = 4;
    }
    else if (KEY_EVENT_UP == key_driver_event)
    {
        // 长按后松手
        key_event_index = 5;
    }

    if (0 == key_event_index || NO_KEY == key_value)
    {
        // 按键事件与上面的事件都不匹配
        // 得到的键值是无效键值
        return RF24G_28_KEY_EVENT_NONE;
    }

    // 遍历表格中的每一个按键：
    for (u8 i = 0; i < sizeof(rf24g_key_type_28keys_table) / sizeof(rf24g_key_type_28keys_table[0]); i++)
    {
        if (key_value == rf24g_key_type_28keys_table[i][0])
        {
            return rf24g_key_type_28keys_table[i][key_event_index];
        }
    }

    // 如果运行到这里，都没有找到对应的按键，返回无效按键事件
    return RF24G_28_KEY_EVENT_NONE;
}

void rf24_key_handle(void)
{
    u8 rf24g_key_event = 0;
    rf24_key_handle_func_t rf24g_key_handle_func_ptr = NULL;

    rf24g_key_event = rf24g_convert_key_event(rf24g_key_driver_value, rf24g_key_driver_event);
    rf24g_key_driver_value = NO_KEY; // 置为无效键值（由于扫描函数只更新，不会清除，在这里要清除）

    if (rf24g_key_event == RF24G_28_KEY_EVENT_NONE)
    {
        // 如果是无效的按键事件，直接返回
        return;
    }

#if USER_DEBUG_ENABLE
    // printf("rf24g_key_event == %u\n", (u16)rf24g_key_event);
#endif

    rf24g_key_handle_func_ptr = rf24_28keys_handle_func_buff[rf24g_key_event];

    if (NULL == rf24g_key_handle_func_ptr)
    {
        // 如果按键事件没有对应的处理函数，直接退出
        return;
    }

    // 直接调用对应的处理函数，这样需要每个处理函数内都要判断一下设备是否开机
    rf24g_key_handle_func_ptr();

    os_taskq_post("msg_task", 1, MSG_USER_SAVE_INFO);
}

// =============================================================================

void rf24g_28keys_event_r2c1_press_handle(void)
{
#if USER_DEBUG_ENABLE
    printf("28keys event r2c1\n");
#endif

    if (fc_effect.on_off_flag == DEVICE_OFF)
    {
        // 灯没有打开，直接返回
        return;
    }

    if (IS_STATIC == fc_effect.Now_state)
    {
        const u8 step = 10;
        if (fc_effect.app_b < 100 - step)
        {
            fc_effect.app_b += step;
        }
        else
        {
            fc_effect.app_b = 100;
        }

        led_strip_rgb_set_brightness(fc_effect.app_b);
        WS2812FX_setBrightness(fc_effect.b);
        report_brightness(fc_effect.app_b);
#if USER_DEBUG_ENABLE
        printf("fc_effect.app_b %u\n", (u16)fc_effect.app_b);
        printf("fc_effect.b %u\n", (u16)fc_effect.b);
#endif
    }
    else if (IS_light_scene == fc_effect.Now_state)
    {
        // 动态模式下，调节速度
        // 注意：速度值是越小越快
        const u8 step = 10;
        if (fc_effect.app_speed < 100 - step)
        {
            fc_effect.app_speed += step;
        }
        else
        {
            fc_effect.app_speed = 100;
        }

        led_strip_rgb_set_speed(fc_effect.app_speed);
        led_strip_rgb_schedule();
        report_speed(fc_effect.app_speed);

#if USER_DEBUG_ENABLE
        printf("fc_effect.app_speed %u\n", (u16)fc_effect.app_speed);
        printf("fc_effect.dream_scene.speed %u\n", (u16)fc_effect.dream_scene.speed);
#endif
    }
    else if (IS_light_music == fc_effect.Now_state ||
             (DEVICE_ON == led_strip_white.is_dev_open &&
              (15 == led_strip_white.mode_index ||
               16 == led_strip_white.mode_index)))
    {
        // RGB幻彩灯处于声控模式，或者是纯白色流星灯处于声控模式
        const u8 step = 10;
        if (fc_effect.music.s < 100 - step)
        {
            fc_effect.music.s += step;
        }
        else
        {
            fc_effect.music.s = 100;
        }

        led_strip_white.sensitivity = fc_effect.music.s;
        report_sound_control_sensitivity(fc_effect.music.s);
    }
    else
    {
        // 其他模式，直接退出
        return;
    }
}

void rf24g_28keys_event_r2c2_press_handle(void)
{
#if USER_DEBUG_ENABLE
    printf("28keys event r2c2\n");
#endif

    if (fc_effect.on_off_flag == DEVICE_OFF)
    {
        // 灯没有打开，直接返回
        return;
    }

    if (IS_STATIC == fc_effect.Now_state)
    {
        // 注意：不能让灯光亮度减到0%，灯光会熄灭
        const u8 step = 10;
        if (fc_effect.app_b > 0 + step)
        {
            fc_effect.app_b -= step;
        }
        else
        {
            fc_effect.app_b = 0;
        }

        led_strip_rgb_set_brightness(fc_effect.app_b);
        WS2812FX_setBrightness(fc_effect.b);
        report_brightness(fc_effect.app_b);
#if USER_DEBUG_ENABLE
        printf("fc_effect.app_b %u\n", (u16)fc_effect.app_b);
        printf("fc_effect.b %u\n", (u16)fc_effect.b);
#endif
    }
    else if (IS_light_scene == fc_effect.Now_state)
    {
        // 动态模式下，调节速度
        // 注意：速度值是越小越快
        const u8 step = 10;
        if (fc_effect.app_speed > 0 + step)
        {
            fc_effect.app_speed -= step;
        }
        else
        {
            fc_effect.app_speed = 0;
        }

        led_strip_rgb_set_speed(fc_effect.app_speed);
        led_strip_rgb_schedule();
        report_speed(fc_effect.app_speed);

#if USER_DEBUG_ENABLE
        printf("fc_effect.app_speed %u\n", (u16)fc_effect.app_speed);
        printf("fc_effect.dream_scene.speed %u\n", (u16)fc_effect.dream_scene.speed);
#endif
    }
    else if (IS_light_music == fc_effect.Now_state ||
             (DEVICE_ON == led_strip_white.is_dev_open &&
              (15 == led_strip_white.mode_index ||
               16 == led_strip_white.mode_index)))
    {
        // RGB幻彩灯处于声控模式，或者是纯白色流星灯处于声控模式
        const u8 step = 10;
        if (fc_effect.music.s > step)
        {
            fc_effect.music.s -= step;
        }
        else
        {
            fc_effect.music.s = 0;
        }

        led_strip_white.sensitivity = fc_effect.music.s;
        report_sound_control_sensitivity(fc_effect.music.s);
    }
    else
    {
        // 其他模式，直接退出
        return;
    }
}

void rf24g_28keys_event_r2c3_press_handle(void)
{
#if USER_DEBUG_ENABLE
    printf("28keys event r2c3\n");
#endif

    if (fc_effect.on_off_flag == DEVICE_OFF)
    {
        // 七彩灯没有打开，直接返回
        return;
    }

    // 使用固定的声控模式：
    fc_effect.Now_state = IS_light_music;
    fc_effect.music.m = 1; // 设置 声控模式索引
    led_strip_rgb_schedule();
}

void rf24g_28keys_event_r2c4_press_handle(void)
{
#if USER_DEBUG_ENABLE
    printf("28keys event r2c4\n");
#endif

    if (fc_effect.on_off_flag == DEVICE_OFF)
    {
        // 没有打开 -> 打开它
#if USER_DEBUG_ENABLE
        printf("light open\n");
#endif

        fc_effect.on_off_flag = DEVICE_ON;
        led_strip_rgb_schedule();

        led_strip_white.is_dev_open = 1;
        led_strip_white_schedule();

        report_dev_on_off_state(fc_effect.on_off_flag);
        report_meteor_on_off_status(led_strip_white.is_dev_open);
    }
    else
    {
// 已经打开 -> 关闭它
#if USER_DEBUG_ENABLE
        printf("light close\n");
#endif

        fc_effect.on_off_flag = DEVICE_OFF;
        led_strip_rgb_schedule();

        led_strip_white.is_dev_open = 0;
        led_strip_white_schedule();

        report_dev_on_off_state(fc_effect.on_off_flag);
        report_meteor_on_off_status(led_strip_white.is_dev_open);
    }
}

void rf24g_28keys_event_r3c1_press_handle(void)
{
#if USER_DEBUG_ENABLE
    printf("28keys event r3c1\n");
#endif

    if (fc_effect.on_off_flag == DEVICE_OFF)
    {
        // 七彩灯没有打开，直接返回
        return;
    }

    led_strip_rgb_set_static_color(RED);
}

void rf24g_28keys_event_r3c2_press_handle(void)
{
#if USER_DEBUG_ENABLE
    printf("28keys event r3c2\n");
#endif

    if (fc_effect.on_off_flag == DEVICE_OFF)
    {
        // 七彩灯没有打开，直接返回
        return;
    }

    led_strip_rgb_set_static_color(GREEN);
}

void rf24g_28keys_event_r3c3_press_handle(void)
{
#if USER_DEBUG_ENABLE
    printf("28keys event r3c3\n");
#endif

    if (fc_effect.on_off_flag == DEVICE_OFF)
    {
        // 七彩灯没有打开，直接返回
        return;
    }

    led_strip_rgb_set_static_color(BLUE);
}

void rf24g_28keys_event_r3c4_press_handle(void)
{
#if USER_DEBUG_ENABLE
    printf("28keys event r3c4\n");
#endif

    if (fc_effect.on_off_flag == DEVICE_OFF)
    {
        // 七彩灯没有打开，直接返回
        return;
    }

    // 混白色
    color_t color_structure = {0};
    color_structure.r = 255;
    color_structure.g = 255;
    color_structure.b = 255;
    led_strip_rgb_set_static_color_by_structure(color_structure);
}

void rf24g_28keys_event_r4c1_press_handle(void)
{
#if USER_DEBUG_ENABLE
    printf("28keys event r4c1\n");
#endif

    if (fc_effect.on_off_flag == DEVICE_OFF)
    {
        // 七彩灯没有打开，直接返回
        return;
    }

    color_t color_structure = {0};
    color_structure.r = 235;
    color_structure.g = 84;
    color_structure.b = 5;
    led_strip_rgb_set_static_color_by_structure(color_structure);
}

void rf24g_28keys_event_r4c2_press_handle(void)
{
#if USER_DEBUG_ENABLE
    printf("28keys event r4c2\n");
#endif

    if (fc_effect.on_off_flag == DEVICE_OFF)
    {
        // 七彩灯没有打开，直接返回
        return;
    }

    color_t color_structure = {0};
    color_structure.r = 60;
    color_structure.g = 176;
    color_structure.b = 53;
    led_strip_rgb_set_static_color_by_structure(color_structure);
}

void rf24g_28keys_event_r4c3_press_handle(void)
{
#if USER_DEBUG_ENABLE
    printf("28keys event r4c3\n");
#endif

    if (fc_effect.on_off_flag == DEVICE_OFF)
    {
        // 七彩灯没有打开，直接返回
        return;
    }

    color_t color_structure = {0};
    color_structure.r = 1;
    color_structure.g = 145;
    color_structure.b = 216;
    led_strip_rgb_set_static_color_by_structure(color_structure);
}

void rf24g_28keys_event_r4c4_press_handle(void)
{
#if USER_DEBUG_ENABLE
    printf("28keys event r4c4\n");
#endif

    if (fc_effect.on_off_flag == DEVICE_OFF)
    {
        // 七彩灯没有打开，直接返回
        return;
    }

    // 七彩跳变
    ls_set_color(0, BLUE);
    ls_set_color(1, GREEN);
    ls_set_color(2, RED);
    ls_set_color(3, WHITE);
    ls_set_color(4, YELLOW);
    ls_set_color(5, CYAN);
    ls_set_color(6, PURPLE);
    fc_effect.dream_scene.change_type = MODE_JUMP;
    fc_effect.dream_scene.c_n = 7;
    fc_effect.Now_state = IS_light_scene;

    led_strip_rgb_schedule();
}

void rf24g_28keys_event_r5c1_press_handle(void)
{
#if USER_DEBUG_ENABLE
    printf("28keys event r5c1\n");
#endif

    if (fc_effect.on_off_flag == DEVICE_OFF)
    {
        // 七彩灯没有打开，直接返回
        return;
    }

    color_t color_structure = {0};
    color_structure.r = 238;
    color_structure.g = 131;
    color_structure.b = 3;
    led_strip_rgb_set_static_color_by_structure(color_structure);
}

void rf24g_28keys_event_r5c2_press_handle(void)
{
#if USER_DEBUG_ENABLE
    printf("28keys event r5c2\n");
#endif

    if (fc_effect.on_off_flag == DEVICE_OFF)
    {
        // 七彩灯没有打开，直接返回
        return;
    }

    color_t color_structure = {0};
    color_structure.r = 48;
    color_structure.g = 185;
    color_structure.b = 191;
    led_strip_rgb_set_static_color_by_structure(color_structure);
}

void rf24g_28keys_event_r5c3_press_handle(void)
{
#if USER_DEBUG_ENABLE
    printf("28keys event r5c3\n");
#endif

    if (fc_effect.on_off_flag == DEVICE_OFF)
    {
        // 七彩灯没有打开，直接返回
        return;
    }

    color_t color_structure = {0};
    color_structure.r = 49;
    color_structure.g = 32;
    color_structure.b = 126;
    led_strip_rgb_set_static_color_by_structure(color_structure);
}

void rf24g_28keys_event_r5c4_press_handle(void)
{
#if USER_DEBUG_ENABLE
    printf("28keys event r5c4\n");
#endif

    if (fc_effect.on_off_flag == DEVICE_OFF)
    {
        // 七彩灯没有打开，直接返回
        return;
    }

    // 七彩渐变
    ls_set_color(0, BLUE);
    ls_set_color(1, GREEN);
    ls_set_color(2, RED);
    ls_set_color(3, WHITE);
    ls_set_color(4, YELLOW);
    ls_set_color(5, CYAN);
    ls_set_color(6, PURPLE);
    fc_effect.dream_scene.change_type = MODE_MUTIL_C_GRADUAL;
    fc_effect.dream_scene.c_n = 7;
    fc_effect.Now_state = IS_light_scene;
    led_strip_rgb_schedule();
}

void rf24g_28keys_event_r6c1_press_handle(void)
{
#if USER_DEBUG_ENABLE
    printf("28keys event r6c1\n");
#endif

    color_t color_structure = {0};
    color_structure.r = 251;
    color_structure.g = 190;
    color_structure.b = 4;
    led_strip_rgb_set_static_color_by_structure(color_structure);
}

void rf24g_28keys_event_r6c2_press_handle(void)
{
#if USER_DEBUG_ENABLE
    printf("28keys event r6c2\n");
#endif

    if (fc_effect.on_off_flag == DEVICE_OFF)
    {
        // 如果 没有启动，不做处理
        return;
    }

    color_t color_structure = {0};
    color_structure.r = 0;
    color_structure.g = 170;
    color_structure.b = 177;
    led_strip_rgb_set_static_color_by_structure(color_structure);
}

void rf24g_28keys_event_r6c3_press_handle(void)
{
#if USER_DEBUG_ENABLE
    printf("28keys event r6c3\n");
#endif

    if (fc_effect.on_off_flag == DEVICE_OFF)
    {
        // 如果 没有启动，不做处理
        return;
    }

    color_t color_structure = {0};
    color_structure.r = 104;
    color_structure.g = 58;
    color_structure.b = 146;
    led_strip_rgb_set_static_color_by_structure(color_structure);
}

void rf24g_28keys_event_r6c4_press_handle(void)
{
#if USER_DEBUG_ENABLE
    printf("28keys event r6c4\n");
#endif

    if (fc_effect.on_off_flag == DEVICE_OFF)
    {
        // 如果 没有启动，不做处理
        return;
    }

    // 七彩呼吸
    ls_set_color(0, BLUE);
    ls_set_color(1, GREEN);
    ls_set_color(2, RED);
    ls_set_color(3, WHITE);
    ls_set_color(4, YELLOW);
    ls_set_color(5, CYAN);
    ls_set_color(6, PURPLE);
    fc_effect.dream_scene.change_type = MODE_BREATH;
    fc_effect.dream_scene.c_n = 7;
    fc_effect.Now_state = IS_light_scene;
    led_strip_rgb_schedule();
}

void rf24g_28keys_event_r7c1_press_handle(void)
{
#if USER_DEBUG_ENABLE
    printf("28keys event r7c1\n");
#endif

    if (fc_effect.on_off_flag == DEVICE_OFF)
    {
        // 如果 没有打开，不打开电机，直接返回
        return;
    }

    color_t color_structure = {0};
    color_structure.r = 255;
    color_structure.g = 255;
    color_structure.b = 0;
    led_strip_rgb_set_static_color_by_structure(color_structure);
}

void rf24g_28keys_event_r7c2_press_handle(void)
{
#if USER_DEBUG_ENABLE
    printf("28keys event r7c2\n");
#endif

    if (fc_effect.on_off_flag == DEVICE_OFF)
    {
        // 如果 没有打开，不打开电机，直接返回
        return;
    }

    color_t color_structure = {0};
    color_structure.r = 2;
    color_structure.g = 130;
    color_structure.b = 143;
    led_strip_rgb_set_static_color_by_structure(color_structure);
}

void rf24g_28keys_event_r7c3_press_handle(void)
{
#if USER_DEBUG_ENABLE
    printf("28keys event r7c3\n");
#endif

    if (fc_effect.on_off_flag == DEVICE_OFF)
    {
        // 没有打开，直接返回
        return;
    }

    // 粉
    color_t color_structure = {0};
    color_structure.r = 255;
    color_structure.g = 0;
    color_structure.b = 255;
    led_strip_rgb_set_static_color_by_structure(color_structure);
}

void rf24g_28keys_event_r7c4_press_handle(void)
{
#if USER_DEBUG_ENABLE
    printf("28keys event r7c4\n");
#endif

    if (fc_effect.on_off_flag == DEVICE_OFF)
    {
        // 没有打开，直接返回
        return;
    }

    // 七彩频闪
    ls_set_color(0, BLUE);
    ls_set_color(1, GREEN);
    ls_set_color(2, RED);
    ls_set_color(3, WHITE);
    ls_set_color(4, YELLOW);
    ls_set_color(5, CYAN);
    ls_set_color(6, PURPLE);
    fc_effect.dream_scene.change_type = MODE_STROBE;
    fc_effect.dream_scene.c_n = 7;
    fc_effect.Now_state = IS_light_scene;
    led_strip_rgb_schedule();
}

const rf24_key_handle_func_t rf24_28keys_handle_func_buff[RF24G_28_KEY_EVENT_MAX] = {
    [RF24G_28_KEY_EVENT_R2C1_PRESS] = rf24g_28keys_event_r2c1_press_handle,

    [RF24G_28_KEY_EVENT_R2C2_PRESS] = rf24g_28keys_event_r2c2_press_handle,

    [RF24G_28_KEY_EVENT_R2C3_PRESS] = rf24g_28keys_event_r2c3_press_handle,

    [RF24G_28_KEY_EVENT_R2C4_PRESS] = rf24g_28keys_event_r2c4_press_handle,

    // =======================================================================
    [RF24G_28_KEY_EVENT_R3C1_PRESS] = rf24g_28keys_event_r3c1_press_handle,

    [RF24G_28_KEY_EVENT_R3C2_PRESS] = rf24g_28keys_event_r3c2_press_handle,

    [RF24G_28_KEY_EVENT_R3C3_PRESS] = rf24g_28keys_event_r3c3_press_handle,

    [RF24G_28_KEY_EVENT_R3C4_PRESS] = rf24g_28keys_event_r3c4_press_handle,
    // =======================================================================
    [RF24G_28_KEY_EVENT_R4C1_PRESS] = rf24g_28keys_event_r4c1_press_handle,

    [RF24G_28_KEY_EVENT_R4C2_PRESS] = rf24g_28keys_event_r4c2_press_handle,

    [RF24G_28_KEY_EVENT_R4C3_PRESS] = rf24g_28keys_event_r4c3_press_handle,

    [RF24G_28_KEY_EVENT_R4C4_PRESS] = rf24g_28keys_event_r4c4_press_handle,
    // =======================================================================
    [RF24G_28_KEY_EVENT_R5C1_PRESS] = rf24g_28keys_event_r5c1_press_handle,

    [RF24G_28_KEY_EVENT_R5C2_PRESS] = rf24g_28keys_event_r5c2_press_handle,

    [RF24G_28_KEY_EVENT_R5C3_PRESS] = rf24g_28keys_event_r5c3_press_handle,

    [RF24G_28_KEY_EVENT_R5C4_PRESS] = rf24g_28keys_event_r5c4_press_handle,
    // =======================================================================
    [RF24G_28_KEY_EVENT_R6C1_PRESS] = rf24g_28keys_event_r6c1_press_handle,

    [RF24G_28_KEY_EVENT_R6C2_PRESS] = rf24g_28keys_event_r6c2_press_handle,

    [RF24G_28_KEY_EVENT_R6C3_PRESS] = rf24g_28keys_event_r6c3_press_handle,

    [RF24G_28_KEY_EVENT_R6C4_PRESS] = rf24g_28keys_event_r6c4_press_handle,
    // =======================================================================
    [RF24G_28_KEY_EVENT_R7C1_PRESS] = rf24g_28keys_event_r7c1_press_handle,

    [RF24G_28_KEY_EVENT_R7C2_PRESS] = rf24g_28keys_event_r7c2_press_handle,

    [RF24G_28_KEY_EVENT_R7C3_PRESS] = rf24g_28keys_event_r7c3_press_handle,

    [RF24G_28_KEY_EVENT_R7C4_PRESS] = rf24g_28keys_event_r7c4_press_handle,

};

#endif // RF24GKEY_ENABLE
