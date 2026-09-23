#include "led_strip_rgb_app.h"
#include "led_strip_rgb_schedule.h"

#include "led_strand_effect.h" // fc_effect 定义
#include "save_flash.h"

#include "WS2812FX.H"
#include "user_thread_communication.h"
#include "app_msg_handle.h" //

#include "user_config.h"

// 存放 app 下发的原始指令，避免占用本任务的栈空间
static u8 led_strip_rgb_app_raw_cmd[APP_MSG_POST_BUF_MAX_LEN];

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

// 设置动画模式
void led_strip_rgb_apply_anim_info(app_msg_anim_info_t *info)
{
    u8 i;
    u8 color_buf_idx;

    /*
        在语义层做转换    
        将app下发的指令，翻译成动画调度对应的参数
    */

#if USER_DEBUG_ENABLE
    printf("%s %d\n", __FUNCTION__, __LINE__); // 定位用
#endif

    // 段大小(多少个灯为一组)
    fc_effect.dream_scene.seg_size = info->seg_size;
    if (0 == fc_effect.dream_scene.seg_size) {
        fc_effect.dream_scene.seg_size = 1;
    }

    /*
        颜色数量不能超过颜色池 rgb[MAX_NUM_COLORS] 的大小：
        "堆积"模式还会在 rgb[c_n] 的位置再放一个背景色，越界会写坏内存
    */
    fc_effect.dream_scene.c_n = info->color_num;
    if (fc_effect.dream_scene.c_n > MAX_NUM_COLORS) {
        fc_effect.dream_scene.c_n = MAX_NUM_COLORS;
    }
    fc_effect.dream_scene.direction = info->anim_dir;
    fc_effect.app_speed = info->anim_speed;
    // 将动画速度映射
    fc_effect.dream_scene.speed = 500 - ((u32)500 * fc_effect.app_speed / 100);
    if (fc_effect.dream_scene.speed <= get_max_speed()) {
        fc_effect.dream_scene.speed = get_max_speed();
    }

    // 映射动画亮度
    fc_effect.app_b = info->anim_brightness;
    fc_effect.b = (u16)fc_effect.app_b * (255 - 25) / 100 + 25;
    // 填充颜色池
    color_buf_idx = 0;
    for (i = 0; i < info->color_num && i < MAX_NUM_COLORS; i++) {
        fc_effect.dream_scene.rgb[i].r = info->color_buf[color_buf_idx++];
        fc_effect.dream_scene.rgb[i].g = info->color_buf[color_buf_idx++];
        fc_effect.dream_scene.rgb[i].b = info->color_buf[color_buf_idx++];
    }

    // 根据 info->mode_idx 执行不同的动画
    if (info->mode_idx == APP_MSG_MOD_IDX_STATIC) {
        fc_effect.Now_state = IS_STATIC;
    } else {
        fc_effect.Now_state = IS_light_scene;
    }

    switch (info->mode_idx) {
    case APP_MSG_MOD_IDX_STATIC:
        break;
    case APP_MSG_MOD_IDX_GRADUAL:
        fc_effect.dream_scene.change_type = MODE_MUTIL_C_GRADUAL;
        break;
    case APP_MSG_MOD_IDX_JUMP:
        fc_effect.dream_scene.change_type = MODE_MUTIL_JUMP;
        break;
    case APP_MSG_MOD_IDX_BREATH:
        if (info->color_num == 1) {
            // 单色呼吸，设置背景色为黑色
            fc_effect.dream_scene.change_type = MODE_SINGLE_C_BREATH;
            fc_effect.dream_scene.c_n = 2;
            fc_effect.dream_scene.rgb[1].r = 0x00;
            fc_effect.dream_scene.rgb[1].g = 0x00;
            fc_effect.dream_scene.rgb[1].b = 0x00;
        } else {
            // 多种颜色轮流呼吸
            fc_effect.dream_scene.change_type = MODE_MUTIL_BRAETH;
        }
        break;
    case APP_MSG_MOD_IDX_RUNNING_WATER:
        /*
            如果只有两种颜色，使用 MODE_2_C_FIX_FLOW ，当成两种颜色混色流水来跑
            如果不是两种颜色，使用 MODE_MUTIL_FLOW_WATER ，当成流水来跑
        */
        if (2 == info->color_num) {
            fc_effect.dream_scene.change_type = MODE_2_C_FIX_FLOW;
        } else {
            fc_effect.dream_scene.change_type = MODE_MUTIL_FLOW_WATER;
        }

        break;
    case APP_MSG_MOD_IDX_ACCUMULATION:

        // MODE_SINGLE_SUPERPOSITION // 单色堆积
        // MODE_B_G_SUPERPOSITION  // 带底色堆积
        // MODE_MUTILE_SUPERPOSITION // 多色堆积，不灭

        if (1 == info->color_num) {
            // 一共两种颜色，一种是前景色，一种是背景色
            fc_effect.dream_scene.c_n = 2;
            fc_effect.dream_scene.change_type = MODE_B_G_SUPERPOSITION;

        } else {
            /*
                背景色也要占一个槽位(rgb[c_n])，
                颜色数已经到上限时不能再 +1，否则 rgb[] 越界
            */
            if (fc_effect.dream_scene.c_n >= MAX_NUM_COLORS) {
                fc_effect.dream_scene.c_n = MAX_NUM_COLORS - 1;
            } else {
                fc_effect.dream_scene.c_n++;
            }
            fc_effect.dream_scene.change_type = MODE_MUTILE_SUPERPOSITION;
        }

        fc_effect.dream_scene.rgb[fc_effect.dream_scene.c_n].r =
            info->background_color_r;
        fc_effect.dream_scene.rgb[fc_effect.dream_scene.c_n].g =
            info->background_color_g;
        fc_effect.dream_scene.rgb[fc_effect.dream_scene.c_n].b =
            info->background_color_b;

        break;
    case APP_MSG_MOD_IDX_STARRY_SKY:
        fc_effect.dream_scene.change_type = MODE_SINGLE_FLASH_RANDOM;
        break;
    case APP_MSG_MOD_IDX_NEBULA:
        fc_effect.dream_scene.change_type = MODE_SEG_FLASH_RANDOM;
        break;
    case APP_MSG_MOD_IDX_METEOR:
        fc_effect.dream_scene.change_type = MODE_B_G_METEOR;
        /*
            MODE_B_G_METEOR 对应的动画中，
            固定用fc_effect.dream_scene.rgb[1]作为背景色
         */
        fc_effect.dream_scene.rgb[1].r = info->background_color_r;
        fc_effect.dream_scene.rgb[1].g = info->background_color_g;
        fc_effect.dream_scene.rgb[1].b = info->background_color_b;
        break;
    case APP_MSG_MOD_IDX_METEOR_SHOWER:
        fc_effect.dream_scene.change_type = MODE_SINGLE_METEOR;
        break;
    case APP_MSG_MOD_IDX_OPENING_AND_CLOSING:
        if (info->anim_dir == 0) {
            // 0 ，表示开幕
            fc_effect.dream_scene.change_type = MODE_OPEN;
        } else {
            // 1 ，表示闭幕
            fc_effect.dream_scene.change_type = MODE_CLOSE;
        }
        break;
    case APP_MSG_MOD_IDX_RUN:
        fc_effect.dream_scene.change_type = MODE_DOT_RUNNING;
        break;
    }

#if USER_DEBUG_ENABLE
    printf("%s %d\n", __FUNCTION__, __LINE__);
#endif

    // 调用对应的动画
    led_strip_rgb_schedule();
}

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
    // app_msg_anim_info_t info = {0};

    while (1) {
        // 接收任意类型的消息，再区分处理
        ret = user_thread_communication_recv_msg_any(&msg_type);
        if (ret) {
            // 接收出错，重新开始接收
            continue;
        }

        // app 下发的原始指令：在本任务里解析并执行，避免占用蓝牙写回调的时间
        if (USER_THREAD_COMM_MSG_TYPE_APP_MSG_RAW == msg_type) {
#if USER_DEBUG_ENABLE
            printf("%s %d\n", __FUNCTION__, __LINE__);
#endif

            while (
                app_msg_post_fetch(led_strip_rgb_app_raw_cmd, &raw_cmd_len)) {
                app_msg_handle(led_strip_rgb_app_raw_cmd, raw_cmd_len);
            }
            continue;
        }

#if 0
        // USER_THREAD_COMM_MSG_TYPE_APP_MSG_ANIM_INFO_UPDATE == msg_type
        // 处理 app 下发的动画指令
        app_msg_anim_info_get(&info);

        // fc_effect 中没有段数量、

        user_data_save_enable();
#endif
    }
}
