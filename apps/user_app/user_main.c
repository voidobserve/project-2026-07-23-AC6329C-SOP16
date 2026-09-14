#include "user_config.h"
#include "user_include.h"
#include "includes.h"

#include "led_strip_voice.h"
#include "led_strip_driver.h"
#include "save_flash.h"
#include "led_strip_rgb_schedule.h"
#include "user_ble_notify.h"

#include "user_rtc.h"

extern void WS2812_circle_task(void);
extern void user_main_task(void *p);

void WS2812_circle_task(void)
{
    meteor_period_sub();

    sound_handle();
    run_tick_per_10ms();
    WS2812FX_service();

    dot_runningh_handle();
}

void user_init(void)
{
    sound_ctl_init();
    led_strip_driver_init();

    user_rtc_init(); // 设置默认的rtc时间

    sys_s_hi_timer_add(NULL, user_10ms_isr, 10);
    sys_s_hi_timer_add(NULL, WS2812_circle_task, 10); // 10ms
    user_data_init();

    // REVIEW 还不确定要不要加入白色流星
    // WS2812FX_init((LED_STRIP_RGB_NUMS + LED_STRIP_WHITE_NUMS),
    //           fc_effect.sequence);
    WS2812FX_init((LED_STRIP_RGB_NUMS), fc_effect.sequence);
    WS2812FX_setBrightness(fc_effect.b);

    led_strip_rgb_schedule();
    task_create(user_main_task, NULL, "user_task");
}

void user_main_task(void *p)
{
    u16 cnt = 0;

    while (1) {
        user_ble_notify_param_handle();
        user_data_save_handle();

        os_time_dly(1);
    }
}

void user_10ms_isr(void)
{
    user_data_save_time_10ms_isr();
    user_rtc_handle();
}
