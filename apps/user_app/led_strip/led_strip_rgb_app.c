#include "led_strip_rgb_app.h"
#include "led_strip_rgb_schedule.h"

#include "led_strand_effect.h" // fc_effect 定义
#include "save_flash.h"

#include "user_thread_communication.h"
#include "app_msg_handle.h" // app_msg_anim_info_get()

void led_strip_rgb_set_brightness(u8 brightness_percent)
{
    fc_effect.app_b = brightness_percent;
    /*
		七彩灯的亮度值范围： 0 ~ 255 ，
		但是只用到 25（255的10%） ~ 255，
		这里通过计算，将 fc_effect.app_b 的 0 ~ 100 映射到 25 ~ 255
	*/
    fc_effect.b = brightness_percent * (255 - 25) / 100 + 25;
}

//
void led_strip_rgb_set_speed(u8 speed_percent)
{
    fc_effect.app_speed = speed_percent;
    fc_effect.dream_scene.speed = 500 - (500 * speed_percent / 100);
    if (fc_effect.dream_scene.speed < get_max_speed()) {
        fc_effect.dream_scene.speed = get_max_speed();
    }
}

void led_strip_rgb_set_static_color(u32 color)
{
    fc_effect.Now_state = IS_STATIC;

    fc_effect.dream_scene.rgb[0].r = color >> 16;
    fc_effect.dream_scene.rgb[0].g = color >> 8;
    fc_effect.dream_scene.rgb[0].b = color >> 0;

    led_strip_rgb_schedule();
}

void led_strip_rgb_set_static_color_by_structure(color_t color_structure)
{
    fc_effect.Now_state = IS_STATIC;

    fc_effect.dream_scene.rgb[0].r = color_structure.r;
    fc_effect.dream_scene.rgb[0].g = color_structure.g;
    fc_effect.dream_scene.rgb[0].b = color_structure.b;

    led_strip_rgb_schedule();
}

// 存放 app 下发的原始指令，避免占用本任务的栈空间
static u8 led_strip_rgb_app_raw_cmd[APP_MSG_POST_BUF_MAX_LEN];

/**
 * @brief 接收app传递过来的有效指令，并执行
 * 
 * @param p 未使用
 */
void led_strip_rgb_app_msg_handle_task(void *p)
{
    int ret = 0;
    u16 raw_cmd_len = 0;
    user_msg_type_t msg_type = USER_THREAD_COMM_MSG_TYPE_NONE;
    app_msg_anim_info_t info = {0};

    while (1) {
        // 接收任意类型的消息，再区分处理
        ret = user_thread_communication_recv_msg_any(&msg_type);
        if (ret) {
            // 接收出错，重新开始接收
            continue;
        }

        // app 下发的原始指令：在本任务里解析并执行，避免占用蓝牙写回调的时间
        if (USER_THREAD_COMM_MSG_TYPE_APP_MSG_RAW == msg_type) {
            while (
                app_msg_post_fetch(led_strip_rgb_app_raw_cmd, &raw_cmd_len)) {
                app_msg_handle(led_strip_rgb_app_raw_cmd, raw_cmd_len);
            }
            continue;
        }

        // USER_THREAD_COMM_MSG_TYPE_APP_MSG_ANIM_INFO_UPDATE == msg_type
        // 处理 app 下发的动画指令
        app_msg_anim_info_get(&info);

        // fc_effect 中没有段数量、

        // 段大小(多少个灯为一组)
        fc_effect.dream_scene.seg_size = info.seg_size;
        fc_effect.dream_scene.c_n = info.color_num; // 颜色数量
        fc_effect.app_speed = info.anim_speed;
        // 将动画速度映射
        fc_effect.dream_scene.speed =
            500 - ((u32)500 * fc_effect.app_speed / 100);
        if (fc_effect.dream_scene.speed <= get_max_speed()) {
            fc_effect.dream_scene.speed = get_max_speed();
        }
        // 映射动画亮度
        fc_effect.app_b = info.anim_brightness;
        fc_effect.b = (u16)fc_effect.app_b * (255 - 25) / 100 + 25;

        // 根据 info.mode_idx 执行不同的动画
        switch (info.mode_idx) {
        case APP_MSG_MOD_IDX_STATIC:
            // 静态模式
            fc_effect.Now_state = IS_STATIC;

            break;
        case APP_MSG_MOD_IDX_GRADUAL:
            break;
        case APP_MSG_MOD_IDX_JUMP:
            break;
        case APP_MSG_MOD_IDX_BREATH:
            break;
        case APP_MSG_MOD_IDX_RUNNING_WATER:
            break;
        case APP_MSG_MOD_IDX_ACCUMULATION:
            break;
        case APP_MSG_MOD_IDX_STARRY_SKY:
            break;
        case APP_MSG_MOD_IDX_NEBULA:
            break;
        case APP_MSG_MOD_IDX_METEOR:
            break;
        case APP_MSG_MOD_IDX_METEOR_SHOWER:
            break;
        case APP_MSG_MOD_IDX_OPENING_AND_CLOSING:
            break;
        case APP_MSG_MOD_IDX_RUN:
            break;
        }

		// 调用对应的动画
		led_strip_rgb_schedule();
        user_data_save_enable();
    }
}
