#include "user_rtc.h"
#include "rtc_alarm.h"
#include "rtc.h"

#include "led_strand_effect.h"
#include "led_strip_rgb_schedule.h"
#include "report.h"

#include "user_config.h"

static volatile user_time_t cur_time = {0};
static volatile user_alarm_t user_alarm[3] = {0};

void user_rtc_init(void)
{
    memset(&cur_time, 0, sizeof(user_time_t));
    cur_time.weekday = 1; // 默认星期一

    memset(&user_alarm, 0, sizeof(user_alarm_t) * 3);
}

void user_rtc_get_time(user_time_t *get_time)
{
    memcpy(get_time, &cur_time, sizeof(user_time_t));
}

void user_rtc_set_time(user_time_t *set_time)
{
    memcpy(&cur_time, set_time, sizeof(user_time_t));
}

void user_rtc_set_alarm(user_alarm_t *alarm, u8 alarm_idx)
{
    memcpy(&user_alarm[alarm_idx], alarm, sizeof(user_alarm_t));
}

void user_rtc_get_alarm_info(user_alarm_t *alarm, u8 alarm_idx)
{
    memcpy(alarm, &user_alarm[alarm_idx], sizeof(user_alarm_t));
}

/**
 * @brief 用户自定义的rtc处理函数
 * 
 * @note 每隔 10ms 调用一次
 * 
 */
void user_rtc_handle(void)
{
    // 每隔一段时间，读取一次时间
    static volatile u16 cnt = 0;
    u8 i;
    u8 weekday_idx; // 遍历星期时，使用到的索引值

    cnt++;
    if (cnt < (1000 / 10)) {
        return;
    }
    cnt = 0;

    cur_time.sec++;
    if (cur_time.sec >= 60) {
        cur_time.sec = 0;
        cur_time.min++;
        if (cur_time.min >= 60) {
            cur_time.min = 0;
            cur_time.hour++;
            if (cur_time.hour >= 24) {
                cur_time.hour = 0;
                cur_time.weekday++;
                if (cur_time.weekday >= 8) {
                    cur_time.weekday = 1;
                }
            }
        }
    }

    printf("%u:%u:%u\n", (u16)cur_time.hour, (u16)cur_time.min,
           (u16)cur_time.sec);
    printf("weekday == %u\n", (u16)cur_time.weekday);

    // USER_TO_DO 如果到了闹钟时间，则执行相应的操作

    // 遍历闹钟 0 ~ 2
    for (i = 0; i < 3; i++) {
        if (0 == user_alarm[i].enable) {
            continue;
        }

        //
        if (user_alarm[i].weekday & 0x7F) {
            // 闹钟里配置了 星期，遍历 0 ~ 6，对应 星期 1 ~ 7
            for (weekday_idx = 0; weekday_idx < 7; weekday_idx++) {
                if (user_alarm[i].weekday & (1 << weekday_idx)) {
                    if (user_alarm[i].hour == cur_time.hour &&
                        user_alarm[i].min == cur_time.min &&
                        cur_time.sec == 0) {
                        // 执行闹钟操作
#if USER_DEBUG_ENABLE
                        printf("alarm triggered\n");
                        printf("alarm idx == %u\n", (u8)i);
                        printf("alarm weekday valid\n");
#endif

                        if (user_alarm[i].power_on) {
                            // 设备开机
                            fc_effect.on_off_flag = 1;
                            led_strip_rgb_schedule();
                        } else {
                            fc_effect.on_off_flag = 0;
                            led_strip_rgb_schedule();
                        }

                        report_dev_on_off_state(fc_effect.on_off_flag);
                    }
                }
            }
        } else {
            // 闹钟里没有配置星期，认为每一天都执行闹钟
            if (user_alarm[i].hour == cur_time.hour &&
                user_alarm[i].min == cur_time.min && cur_time.sec == 0) {
                // 执行闹钟操作

#if USER_DEBUG_ENABLE
                printf("alarm triggered\n");
                printf("alarm idx == %u\n", (u8)i);
                printf("alarm weekday invalid\n");
#endif

                if (user_alarm[i].power_on) {
                    // 设备开机
                    fc_effect.on_off_flag = 1;
                    led_strip_rgb_schedule();
                } else {
                    fc_effect.on_off_flag = 0;
                    led_strip_rgb_schedule();
                }

                report_dev_on_off_state(fc_effect.on_off_flag);
            }
        }
    }
}
