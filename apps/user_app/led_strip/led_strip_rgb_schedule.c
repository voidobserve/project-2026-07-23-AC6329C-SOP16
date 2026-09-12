// RBG幻彩灯带，调度 .c 文件
#include "led_strip_rgb_schedule.h"
#include "led_strip_driver.h"
#include "WS2812FX.H"
#include "led_strip_rgb_anim.h" // 动画效果
#include "Adafruit_NeoPixel.H"
#include "ws2812fx_effect.h"
#include "user_include.h"

void led_strip_rgb_strand_rainbow_handler(void);
void led_strip_rgb_multi_jump_handler(void); // 多种颜色跳变
void led_strip_rgb_strand_breath_handler(void);

// led_strip_rgb_handler_map_t led_strip_rgb_handler_map[] = {
// 	{MODE_MUTIL_RAINBOW, led_strip_rgb_strand_rainbow_handler},
// 	{MODE_MUTIL_JUMP,  led_strip_rgb_multi_jump_handler},
// 	{MODE_MUTIL_BRAETH,  },
// 	{MODE_MUTIL_TWIHKLE,  },
// };

void led_strip_rgb_schedule_init(void)
{
    fc_effect.on_off_flag = DEVICE_ON;
    fc_effect.led_num = LED_STRIP_RGB_NUMS;
    fc_effect.Now_state = IS_STATIC;

    fc_effect.rgb.r = 255;
    fc_effect.rgb.g = 0;
    fc_effect.rgb.b = 0;

    fc_effect.sequence = NEO_RGB;
    fc_effect.b = 255;
    fc_effect.app_b = 100; // 反馈给app的亮度，单位：百分比

    fc_effect.app_speed = 80;
    fc_effect.dream_scene.speed = (500 - (u32)fc_effect.app_speed * 500 / 100);
    fc_effect.music.m = 1;
    fc_effect.music.s = 85;

    // 流星周期控制
    fc_effect.meteor_period = 8;                           // 默认8秒  周期值
    fc_effect.period_cnt = fc_effect.meteor_period * 1000; // ms,运行时的计数器
    fc_effect.mode_cycle = 0; // 模式完成一个循环的标志

    fc_effect.dream_scene.c_n = 1;
    fc_effect.dream_scene.rgb[0].r = 255;
    fc_effect.dream_scene.rgb[0].g = 0;
    fc_effect.dream_scene.rgb[0].b = 0;
    // fc_effect.dream_scene.change_type = MODE_COLOR_METEOR;

#if 0
	// led_strip_rgb.dream_scene.speed = 100;
	// led_strip_rgb.dream_scene.c_n = 1;
	// led_strip_rgb.dream_scene.rgb[0].r = 0;
	// led_strip_rgb.dream_scene.rgb[0].g = 255;
	// led_strip_rgb.dream_scene.rgb[0].b = 0;
	// led_strip_rgb.meteor_period = 8;							   // 默认8秒  周期值
	// led_strip_rgb.period_cnt = led_strip_rgb.meteor_period * 1000; // ms,运行时的计数器
	// led_strip_rgb.mode_cycle = 0;								   // 模式完成一个循环的标志
	// led_strip_rgb.dream_scene.change_type = MODE_COLOR_METEOR;
#endif
}

// USER_TO_DO 下面这些处理函数，都可以用查表的方法，把动画、速度、配置写到映射表中：

// 单个，全彩颜色的流星效果
// void led_strip_rgb_color_meteor_handler(void)
// {
// 	mode_ptr mode;
// 	uint8_t option;

// 	mode = &led_strip_rgb_anim_single_color_meteor;

// 	if (fc_effect.dream_scene.direction == IS_forward)
// 	{
// 		option = 0 | SIZE_XLARGE;
// 	}
// 	else
// 	{
// 		option = REVERSE | SIZE_XLARGE;
// 	}

// 	led_strip_rgb_schedule_set_mode(
// 		mode,
// 		fc_effect.dream_scene.speed,
// 		option);
// }

void led_strip_rgb_strand_rainbow_handler(void)
{
    u8 option;
    mode_ptr mode;

    option = SIZE_SMALL;
    mode = &led_strip_rgb_anim_mutil_fade;

    led_strip_rgb_schedule_set_mode(mode, fc_effect.dream_scene.speed, option);
}

// 多段颜色跳变
void led_strip_rgb_multi_jump_handler(void)
{
    uint8_t option;
    mode_ptr mode;

    // 正向
    if (fc_effect.dream_scene.direction == IS_forward) {
        option = 0;
    } else {
        option = REVERSE;
    }

    mode = &led_strip_rgb_anim_single_block_scan;

    led_strip_rgb_schedule_set_mode(mode, fc_effect.dream_scene.speed, option);
}

void led_strip_rgb_strand_breath_handler(void)
{
    mode_ptr mode;
    u8 option;
    mode = &led_strip_rgb_anim_mutil_breath;
    option = SIZE_MEDIUM;

    led_strip_rgb_schedule_set_mode(mode, fc_effect.dream_scene.speed, option);
}

void led_strip_rgb_strand_twihkle_handler(void)
{
    mode_ptr mode;
    uint8_t option;
    u16 speed;

    mode = &led_strip_rgb_anim_mutil_twihkle;
    option = SIZE_SMALL;
    speed = fc_effect.dream_scene.speed;

    led_strip_rgb_schedule_set_mode(mode, speed, option);
}

void led_strip_rgb_strand_flow_water_handler(void)
{
    mode_ptr mode;
    u16 speed;
    uint8_t option;

    mode = &led_strip_rgb_anim_multi_block_scan;
    speed = fc_effect.dream_scene.speed;

    if (fc_effect.dream_scene.direction == IS_forward) {
        option = SIZE_MEDIUM | 0;
    } else {
        option = SIZE_MEDIUM | REVERSE;
    }

    led_strip_rgb_schedule_set_mode(mode, speed, option);
}

void led_strip_rgb_strand_chas_light_handler(void)
{
    mode_ptr mode;
    u16 speed;
    uint8_t options;

    speed = fc_effect.dream_scene.speed;
    options = NO_OPTIONS;

    // 正向
    if (fc_effect.dream_scene.direction == IS_forward) {
        mode = &WS2812FX_mode_multi_forward_same;
    } else {
        mode = &WS2812FX_mode_multi_back_same;
    }

    led_strip_rgb_schedule_set_mode(mode, speed, options);
}

void led_strip_rgb_strand_colorful_handler(void)
{
    mode_ptr mode;
    u16 speed;
    uint8_t options;

    mode = &led_strip_rgb_anim_multi_block_scan;
    speed = fc_effect.dream_scene.speed;
    options = SIZE_SMALL;

    led_strip_rgb_schedule_set_mode(mode, speed, options);
}

void led_strip_rgb_mutil_seg_grandual_handler(void)
{
    mode_ptr mode;
    u16 speed;
    uint8_t options;

    mode = &led_strip_rgb_anim_mutil_fade;
    speed = fc_effect.dream_scene.speed;
    options = SIZE_MEDIUM;

    led_strip_rgb_schedule_set_mode(mode, speed, options);
}

void led_strip_rgb_standard_jump_handler(void)
{
    mode_ptr mode;
    u16 speed;
    uint8_t options;

    mode = &WS2812FX_mutil_c_jump;
    speed = fc_effect.dream_scene.speed;
    options = NO_OPTIONS;

    led_strip_rgb_schedule_set_mode(mode, speed, options);
}

void led_strip_rgb_strobe_handler(void)
{
    mode_ptr mode;
    u16 speed;
    uint8_t options;

    mode = &WS2812FX_mutil_strobe;
    speed = fc_effect.dream_scene.speed;
    options = NO_OPTIONS;

    led_strip_rgb_schedule_set_mode(mode, speed, options);
}

void led_strip_rgb_multi_colors_grandual_handler(void)
{
    mode_ptr mode;
    u16 speed;
    uint8_t options;

    mode = &led_strip_rgb_anim_multi_colors_gradual;
    speed = fc_effect.dream_scene.speed;
    options = SIZE_MEDIUM;

    led_strip_rgb_schedule_set_mode(mode, speed, options);
}

void led_strip_rgb_tow_color_fix_flow_handler(void)
{
    mode_ptr mode;
    u16 speed;
    uint8_t options;

    mode = &WS2812FX_mode_running_lights;
    speed = fc_effect.dream_scene.speed;
    if (fc_effect.dream_scene.direction == IS_forward) {
        options = 0;
    } else {
        options = REVERSE;
    }

    led_strip_rgb_schedule_set_mode(mode, speed, options);
}

void led_strip_rgb_single_color_flash_random_handler(void)
{
    mode_ptr mode;
    u16 speed;
    uint8_t options;

    mode = &led_strip_rgb_anim_fire_flicker_intense;
    speed = fc_effect.dream_scene.speed;
    options = 0;

    led_strip_rgb_schedule_set_mode(mode, speed, options);
}

void led_strip_rgb_seg_mutil_color_flash_random_handler(void)
{
    mode_ptr mode;
    u16 speed;
    uint8_t options;

    mode = &led_strip_rgb_anim_fireworks;
    speed = fc_effect.dream_scene.speed;
    options = 0;

    led_strip_rgb_schedule_set_mode(mode, speed, options);
}

void led_strip_rgb_single_color_breath_handler(void)
{
    mode_ptr mode;
    u16 speed;
    uint8_t options;

    mode = &led_strip_rgb_anim_breath;
    speed = fc_effect.dream_scene.speed;
    options = SIZE_MEDIUM;

    led_strip_rgb_schedule_set_mode(mode, speed, options);
}

void led_strip_rgb_single_color_meteor_handler(void)
{
    mode_ptr mode;
    u16 speed;
    uint8_t options;

    mode = &led_strip_rgb_anim_meteor_signle_color;
    speed = fc_effect.dream_scene.speed;
    if (fc_effect.dream_scene.direction == IS_forward) {
        options = 0 | FADE_XSLOW;
    } else {
        options = REVERSE | FADE_XSLOW;
    }

    led_strip_rgb_schedule_set_mode(mode, speed, options);
}

void led_strip_rgb_background_meteor_handler(void)
{
    mode_ptr mode;
    u16 speed;
    uint8_t options;

    mode = &led_strip_rgb_anim_background_meteor;
    speed = fc_effect.dream_scene.speed;
    options = SIZE_MEDIUM;

    led_strip_rgb_schedule_set_mode(mode, speed, options);
}

void led_strip_rgb_mode_open_handler(void)
{
    mode_ptr mode;
    u16 speed;
    uint8_t options;

    mode = &led_strip_rgb_anim_open;
    speed = fc_effect.dream_scene.speed;
    options = 0;

    led_strip_rgb_schedule_set_mode(mode, speed, options);
}

void led_strip_rgb_mode_close_handler(void)
{
    mode_ptr mode;
    u16 speed;
    uint8_t options;

    mode = &led_strip_rgb_anim_close;
    speed = fc_effect.dream_scene.speed;
    options = FADE_XXSLOW;

    led_strip_rgb_schedule_set_mode(mode, speed, options);
}

void led_strip_rgb_dot_running_handler(void)
{
    mode_ptr mode;
    u16 speed;
    uint8_t options;

    mode = &led_strip_rgb_anim_multi_dot_running;
    speed = fc_effect.dream_scene.speed + 50;
    if (fc_effect.dream_scene.direction == IS_forward) {
        options = SIZE_MEDIUM | 0;
    } else {
        options = SIZE_MEDIUM | REVERSE;
    }

    led_strip_rgb_schedule_set_mode(mode, speed, options);
}

// void led_strip_rgb_meteor_jump_handler(void)
// {
// 	mode_ptr mode;
// 	u16 speed;
// 	uint8_t option;

// 	mode = &led_strip_rgb_anim_meteor_jump;
// 	speed = fc_effect.dream_scene.speed;
// 	option = SIZE_MEDIUM;

// 	led_strip_rgb_schedule_set_mode(
// 		mode,
// 		speed,
// 		option);
// }

// void led_strip_rgb_meteor_gradual_handler(void)
// {
// 	mode_ptr mode;
// 	u16 speed;
// 	uint8_t option;

// 	mode = &led_strip_rgb_anim_meteor_gradual;
// 	speed = fc_effect.dream_scene.speed;
// 	option = SIZE_MEDIUM;

// 	led_strip_rgb_schedule_set_mode(
// 		mode,
// 		speed,
// 		option);
// }

// void led_strip_rgb_rainbow_flow_handler(void)
// {
// 	mode_ptr mode;
// 	u16 speed;
// 	uint8_t option;

// 	mode = &led_strip_rgb_anim_rainbow_flow;
// 	speed = fc_effect.dream_scene.speed;
// 	option = SIZE_MEDIUM;

// 	led_strip_rgb_schedule_set_mode(
// 		mode,
// 		speed,
// 		option);
// }

// static void led_strip_rgb_run_light_scene(void)
// {
// 	u8 change_type = fc_effect.dream_scene.change_type;
// 	u8 i;

// 	for (i = 0; i < ARRAY_SIZE(light_scene_table); i++)
// 	{
// 		if (light_scene_table[i].change_type == change_type)
// 		{
// 			light_scene_table[i].handler();
// 			return;
// 		}
// 	}
// }

void led_strip_rgb_breathing_handler(void)
{
    mode_ptr mode;
    u16 speed;
    uint8_t options;

    mode = &led_strip_rgb_anim_breathing;
    speed = fc_effect.dream_scene.speed;
    options = 0;

    led_strip_rgb_schedule_set_mode(mode, speed, options);
}

void led_strip_rgb_single_superposition_handler(void)
{
    mode_ptr mode;
    u16 speed;
    uint8_t options;

    // 正向
    if (fc_effect.dream_scene.direction == IS_forward) {
        options = 0;
    } else {
        options = REVERSE;
    }

    mode = &led_strip_rgb_anim_single_superposition;
    speed = fc_effect.dream_scene.speed;

    led_strip_rgb_schedule_set_mode(mode, speed, options);
}

void led_strip_rgb_single_superposition_with_background_handler(void)
{
    mode_ptr mode;
    u16 speed;
    uint8_t options;

    // 正向
    if (fc_effect.dream_scene.direction == IS_forward) {
        options = 0;
    } else {
        options = REVERSE;
    }

    mode = &led_strip_rgb_anim_single_superposition_with_background;
    speed = fc_effect.dream_scene.speed;

    led_strip_rgb_schedule_set_mode(mode, speed, options);
}

void led_strip_rgb_multi_superposition_handler(void)
{
    mode_ptr mode;
    u16 speed;
    uint8_t options;

    // 正向
    if (fc_effect.dream_scene.direction == IS_forward) {
        options = 0;
    } else {
        options = REVERSE;
    }

    mode = &led_strip_rgb_anim_multi_superposition;
    speed = fc_effect.dream_scene.speed;

    led_strip_rgb_schedule_set_mode(mode, speed, options);
}

void __led_strip_rgb_schedule_in_light_scene__(void)
{
    // USER_TO_DO 使用查表的方法：
    // u8 change_type = fc_effect.dream_scene.change_type;
    // u8 i;
    // mode_ptr led_strip_rgb_mode = NULL; // 动画模式指针
    // u16 speed = 0;
    // u8 option = NO_OPTIONS;

    switch (fc_effect.dream_scene.change_type) {
    case MODE_MUTIL_RAINBOW: // 彩虹
        led_strip_rgb_strand_rainbow_handler();
#if USER_DEBUG_ENABLE
        printf("led_strip_rgb_strand_rainbow_handler\n");
#endif
        break;
    case MODE_MUTIL_JUMP: // 跳变模式
        led_strip_rgb_multi_jump_handler();
#if USER_DEBUG_ENABLE
        printf("led_strip_rgb_multi_jump_handler\n");
#endif
        break;
    case MODE_MUTIL_BRAETH: // 呼吸模式
        led_strip_rgb_strand_breath_handler();
#if USER_DEBUG_ENABLE
        printf("led_strip_rgb_strand_breath_handler\n");
#endif
        break;
    case MODE_MUTIL_TWIHKLE: // 闪烁模式
        led_strip_rgb_strand_twihkle_handler();
#if USER_DEBUG_ENABLE
        printf("led_strip_rgb_strand_twihkle_handler\n");
#endif
        break;

    case MODE_MUTIL_FLOW_WATER: // 流水模式
        led_strip_rgb_strand_flow_water_handler();
#if USER_DEBUG_ENABLE
        printf("led_strip_rgb_strand_flow_water_handler\n");
#endif
        break;

    case MODE_CHAS_LIGHT: // 追光模式
        led_strip_rgb_strand_chas_light_handler();
#if USER_DEBUG_ENABLE
        printf("led_strip_rgb_strand_chas_light_handler\n");
#endif
        break;

    case MODE_MUTIL_COLORFUL: // 炫彩模式
        led_strip_rgb_strand_colorful_handler();
#if USER_DEBUG_ENABLE
        printf("led_strip_rgb_strand_colorful_handler\n");
#endif
        break;

    case MODE_MUTIL_SEG_GRADUAL: // 渐变模式
        led_strip_rgb_mutil_seg_grandual_handler();
#if USER_DEBUG_ENABLE
        printf("led_strip_rgb_mutil_seg_grandual_handler\n");
#endif
        break;

    case MODE_JUMP: // 标准跳变
        led_strip_rgb_standard_jump_handler();
#if USER_DEBUG_ENABLE
        printf("led_strip_rgb_standard_jump_handler\n");
#endif
        break;

    case MODE_STROBE: // 标准频闪
        led_strip_rgb_strobe_handler();
#if USER_DEBUG_ENABLE
        printf("led_strip_rgb_strobe_handler\n");
#endif
        break;

    case MODE_MUTIL_C_GRADUAL: // 多段同时渐变
        led_strip_rgb_multi_colors_grandual_handler();
#if USER_DEBUG_ENABLE
        printf("led_strip_rgb_multi_colors_grandual_handler\n");
#endif
        break;

    case MODE_2_C_FIX_FLOW: // 两种颜色混色流水
        led_strip_rgb_tow_color_fix_flow_handler();
#if USER_DEBUG_ENABLE
        printf("led_strip_rgb_tow_color_fix_flow_handler\n");
#endif
        break;

    case MODE_SINGLE_FLASH_RANDOM:
        led_strip_rgb_single_color_flash_random_handler();
#if USER_DEBUG_ENABLE
        printf("led_strip_rgb_single_color_flash_random_handler\n");
#endif
        break;

    case MODE_SEG_FLASH_RANDOM:
        led_strip_rgb_seg_mutil_color_flash_random_handler();
#if USER_DEBUG_ENABLE
        printf("led_strip_rgb_seg_mutil_color_flash_random_handler\n");
#endif
        break;

    case MODE_SINGLE_C_BREATH: // 单色呼吸
        led_strip_rgb_single_color_breath_handler();
#if USER_DEBUG_ENABLE
        printf("led_strip_rgb_single_color_breath_handler\n");
#endif
        break;

    case MODE_SINGLE_METEOR:
        led_strip_rgb_single_color_meteor_handler();
#if USER_DEBUG_ENABLE
        printf("led_strip_rgb_single_color_meteor_handler\n");
#endif
        break;

    case MODE_B_G_METEOR:
        led_strip_rgb_background_meteor_handler();
#if USER_DEBUG_ENABLE
        printf("led_strip_rgb_background_meteor_handler\n");
#endif
        break;

    case MODE_OPEN:
        led_strip_rgb_mode_open_handler();
#if USER_DEBUG_ENABLE
        printf("led_strip_rgb_mode_open_handler\n");
#endif
        break;

    case MODE_CLOSE:
        led_strip_rgb_mode_close_handler();
#if USER_DEBUG_ENABLE
        printf("led_strip_rgb_mode_close_handler\n");
#endif
        break;

    case MODE_DOT_RUNNING:
        led_strip_rgb_dot_running_handler();
#if USER_DEBUG_ENABLE
        printf("MODE_DOT_RUNNING\n");
        printf("led_strip_rgb_dot_running_handler\n");
#endif
        break;

        // case MODE_COLOR_METEOR: // 无背景颜色的流星
        // 	led_strip_rgb_color_meteor_handler();
        // 	break;

        // case MODE_JUMP_METEORR: // 跳变 效果 的流星
        // 	led_strip_rgb_meteor_jump_handler();
        // 	break;

        // case MODE_GRADUAL_METEOR: // 渐变 效果 的流星
        // 	led_strip_rgb_meteor_gradual_handler();
        // 	break;

        // case MODE_RAINBOW_FLOW:
        // 	led_strip_rgb_rainbow_flow_handler();
        // 	break;

    case MODE_BREATH:
        led_strip_rgb_breathing_handler();
        break;

    case MODE_DOT_RUNNING_COLLECTIONS: // 跑动集合效果
        led_strip_rgb_dot_running_handler();
#if USER_DEBUG_ENABLE
        printf("MODE_DOT_RUNNING_COLLECTIONS\n");
        printf("led_strip_rgb_dot_running_handler\n");
#endif
        break;

    case MODE_SINGLE_SUPERPOSITION: // 单色堆积
        led_strip_rgb_single_superposition_handler();
#if USER_DEBUG_ENABLE
        printf("led_strip_rgb_single_superposition_handler\n");
#endif
        break;
    case MODE_B_G_SUPERPOSITION: // 带底色堆积
        led_strip_rgb_single_superposition_with_background_handler();
#if USER_DEBUG_ENABLE
        printf("led_strip_rgb_single_superposition_with_background_handler\n");
#endif
        break;
    case MODE_MUTILE_SUPERPOSITION:
        led_strip_rgb_multi_superposition_handler();
#if USER_DEBUG_ENABLE
        printf("led_strip_rgb_multi_superposition_handler\n");
#endif
        break;
    }
}

void __led_strip_rgb_schedule_in_custom__(void)
{
    mode_ptr mode;
    u16 speed;
    u8 options;

    mode = &led_strip_rgb_anim_when_adjust_sequence;
    speed = 0;
    options = 0;

    if (fc_effect.custom_mode_index == 2) // 调整RGB顺序效果
    {
        led_strip_rgb_schedule_set_mode(mode, speed, options);
    }
}

void __led_strip_rgb_schedule_in_light_music__(void)
{
    mode_ptr mode;
    u16 speed;
    u8 options;

    switch (fc_effect.music.m) {
    case 0:
        mode = &led_strip_rgb_anim_sound_control_star_random;
        break;
    case 1:
        mode = led_strip_rgb_anim_sound_control_star_white;
        break;
    case 2:
        mode = &led_strip_rgb_anim_sound_control_star_blue;
        break;
    case 3:
        mode = &led_strip_rgb_anim_sound_control_open_and_close_both_sides;
        break;
    case 4:
        mode = &led_strip_rgb_anim_sound_control_open_and_close_slide_to_center;
        break;
    case 5:
        mode = &led_strip_rgb_anim_sound_control_rainbow_flash;
        break;
    case 6:
        mode = &led_strip_rgb_anim_sound_control_feq_rise;
        break;
    case 7:
        mode = &led_strip_rgb_anim_sound_control_feq_rise_bround_color;
        break;
    case 8:
        mode = &led_strip_rgb_anim_sound_control_feq_rise_green_and_blue;
        break;
    case 9:
  mode = &led_strip_rgb_anim_sound_control_energy;
        break;
    case 10:
        break;
    case 11:
        break;

        break;
    default:
      
        break;
    }

    speed = 100;
    options = SIZE_MEDIUM | FADE_XSLOW;

    led_strip_rgb_schedule_set_mode(mode, speed, options);
}

void led_strip_rgb_schedule(void)
{
    mode_ptr led_strip_rgb_mode = NULL; // 动画模式指针
    u16 speed = 0;
    u8 option = NO_OPTIONS;

    if (fc_effect.on_off_flag == DEVICE_OFF) {
        // 跑关机动画
        led_strip_rgb_mode = &led_strip_rgb_anim_pwr_off;
        led_strip_rgb_schedule_set_mode(led_strip_rgb_mode, speed, option);

        return;
    }

    switch (fc_effect.Now_state) {
    case IS_light_scene:
        fc_effect.period_cnt = 0;
        __led_strip_rgb_schedule_in_light_scene__();
        break;
    // case ACT_TY_PAIR:
    // 	// 配对完成，要恢复fc_effect.Now_state
    // 	fc_pair_effect();
    // 	break;
    case ACT_CUSTOM:
        fc_effect.period_cnt = 0;
        __led_strip_rgb_schedule_in_custom__();
        break;
    case IS_light_music:
        // fc_music();
        __led_strip_rgb_schedule_in_light_music__();
        break;
    // case IS_smear_adjust:
    // 	printf("\n IS_smear_adjust");
    // 	fc_smear_adjust();
    // 	break;
    case IS_STATIC:
        led_strip_rgb_mode = &WS2812FX_mode_static;
        speed = 100;
        option = FADE_GLACIAL;
        led_strip_rgb_schedule_set_mode(led_strip_rgb_mode, speed, option);
#if USER_DEBUG_ENABLE
        printf("IS_STATIC\n");
#endif
        break;
    }
}

void meteor_period_sub(void)
{
    if (fc_effect.period_cnt > 10) {
        fc_effect.period_cnt -= 10;
    } else {
        fc_effect.period_cnt = 0; // 计数器清零
        if (fc_effect.mode_cycle) // 模式循环完成，更新
        {
            fc_effect.period_cnt = fc_effect.meteor_period * 1000;
            fc_effect.mode_cycle = 0;
        }
    }
}

/**
 * @brief 调用 WS2812FX 库执行RGB幻彩灯部分的动画效果
 *
 */
void led_strip_rgb_schedule_set_mode(mode_ptr mode, u16 speed, u8 option)
{
    WS2812FX_setSegment_colorOptions(
        LED_STRIP_RGB_SEG_INDEX,                           // 第0段
        LED_STRIP_RGB_STAR_INDEX,                          // 起始位置
        LED_STRIP_RGB_STAR_INDEX + LED_STRIP_RGB_NUMS - 1, // 结束位置
        mode,                                              // 效果
        0,                                                 // 颜色
        speed,                                             // 速度
        option); // 选项，这里像素点大小：3 REVERSE决定方向

    WS2812FX_set_coloQty(LED_STRIP_RGB_SEG_INDEX, fc_effect.dream_scene.c_n);
    ls_set_colors(fc_effect.dream_scene.c_n, &fc_effect.dream_scene.rgb);

    WS2812FX_resetSegmentRuntime(
        LED_STRIP_RGB_SEG_INDEX); // 清除指定段的显示缓存
    WS2812FX_running_flag_set();
}

// 多点跑动的集合效果
// 放在10ms执行一次
void dot_runningh_handle(void)
{
    static volatile u16 dot_cnt = 0;
    static volatile u8 next_one = 0;
    if (fc_effect.Now_state == IS_light_scene &&
        fc_effect.dream_scene.change_type == MODE_DOT_RUNNING_COLLECTIONS) {
        dot_cnt++;

        if (next_one == 0) // 绿
        {
            ls_set_color(0, GREEN);
            ls_set_color(1, BLACK);
            fc_effect.dream_scene.direction = IS_forward;
            fc_effect.dream_scene.c_n = 2;
        } else if (next_one == 1) // 蓝
        {
            ls_set_color(0, BLUE);
            ls_set_color(1, BLACK);
            fc_effect.dream_scene.direction = IS_forward;
            fc_effect.dream_scene.c_n = 2;
        } else if (next_one == 2) // 黄
        {
            ls_set_color(0, YELLOW);
            ls_set_color(1, BLACK);
            fc_effect.dream_scene.direction = IS_forward;
            fc_effect.dream_scene.c_n = 2;
        } else if (next_one == 3) // 紫
        {
            ls_set_color(0, PURPLE);
            ls_set_color(1, BLACK);
            fc_effect.dream_scene.direction = IS_forward;
            fc_effect.dream_scene.c_n = 2;
        } else if (next_one == 4) // 青
        {
            ls_set_color(0, CYAN);
            ls_set_color(1, BLACK);
            fc_effect.dream_scene.direction = IS_forward;
            fc_effect.dream_scene.c_n = 2;
        } else if (next_one == 5) // 白
        {
            ls_set_color(0, WHITE);
            ls_set_color(1, BLACK);
            fc_effect.dream_scene.direction = IS_forward;
            fc_effect.dream_scene.c_n = 2;
        } else if (next_one == 6) // 红绿蓝
        {

            ls_set_color(0, RED);
            ls_set_color(1, GREEN);
            ls_set_color(2, BLUE);
            ls_set_color(3, BLACK);
            fc_effect.dream_scene.direction = IS_forward;
            fc_effect.dream_scene.c_n = 4;
        } else if (next_one == 7) // 紫青黄
        {

            ls_set_color(0, PURPLE);
            ls_set_color(1, CYAN);
            ls_set_color(2, YELLOW);
            ls_set_color(3, BLACK);
            fc_effect.dream_scene.direction = IS_forward;
            fc_effect.dream_scene.c_n = 4;
        } else if (next_one == 8) // 七彩
        {
            ls_set_color(0, RED);
            ls_set_color(1, GREEN);
            ls_set_color(2, BLUE);
            ls_set_color(3, PURPLE);
            ls_set_color(4, CYAN);
            ls_set_color(5, YELLOW);
            ls_set_color(6, WHITE);
            ls_set_color(7, BLACK);
            fc_effect.dream_scene.direction = IS_forward;
            fc_effect.dream_scene.c_n = 8;
        } else if (next_one == 9) // 反向 红
        {
            ls_set_color(0, RED);
            ls_set_color(1, BLACK);
            fc_effect.dream_scene.direction = IS_back;
            fc_effect.dream_scene.c_n = 2;
        } else if (next_one == 10) // 反向 绿
        {
            ls_set_color(0, GREEN);
            ls_set_color(1, BLACK);
            fc_effect.dream_scene.direction = IS_back;
            fc_effect.dream_scene.c_n = 2;
        } else if (next_one == 11) // 反向 蓝
        {
            ls_set_color(0, BLUE);
            ls_set_color(1, BLACK);
            fc_effect.dream_scene.direction = IS_back;
            fc_effect.dream_scene.c_n = 2;
        } else if (next_one == 12) // 反向 黄
        {
            ls_set_color(0, YELLOW);
            ls_set_color(1, BLACK);
            fc_effect.dream_scene.direction = IS_back;
            fc_effect.dream_scene.c_n = 2;
        } else if (next_one == 13) // 反向 紫
        {
            ls_set_color(0, PURPLE);
            ls_set_color(1, BLACK);
            fc_effect.dream_scene.direction = IS_back;
            fc_effect.dream_scene.c_n = 2;
        } else if (next_one == 14) // 反向 青
        {
            ls_set_color(0, CYAN);
            ls_set_color(1, BLACK);
            fc_effect.dream_scene.direction = IS_back;
            fc_effect.dream_scene.c_n = 2;
        } else if (next_one == 15) // 反向 白
        {
            ls_set_color(0, WHITE);
            ls_set_color(1, BLACK);
            fc_effect.dream_scene.direction = IS_back;
            fc_effect.dream_scene.c_n = 2;
        } else if (next_one == 16) // 反向 红绿蓝
        {
            ls_set_color(0, RED);
            ls_set_color(1, GREEN);
            ls_set_color(2, BLUE);
            ls_set_color(3, BLACK);
            fc_effect.dream_scene.direction = IS_back;
            fc_effect.dream_scene.c_n = 4;
        } else if (next_one == 17) // 反向 紫青黄
        {
            ls_set_color(0, YELLOW);
            ls_set_color(1, BLACK);
            fc_effect.dream_scene.direction = IS_back;
            fc_effect.dream_scene.c_n = 2;
        } else if (next_one == 18) // 反向 七彩
        {
            ls_set_color(0, PURPLE);
            ls_set_color(1, CYAN);
            ls_set_color(2, YELLOW);
            ls_set_color(3, BLACK);
            fc_effect.dream_scene.direction = IS_back;
            fc_effect.dream_scene.c_n = 4;
        } else if (next_one == 19) // 反向 七彩
        {
            ls_set_color(0, RED);
            ls_set_color(1, GREEN);
            ls_set_color(2, BLUE);
            ls_set_color(3, PURPLE);
            ls_set_color(4, CYAN);
            ls_set_color(5, YELLOW);
            ls_set_color(6, WHITE);
            ls_set_color(7, BLACK);
            fc_effect.dream_scene.direction = IS_back;
            fc_effect.dream_scene.c_n = 8;
        } else if (next_one == 20) // 红
        {
            ls_set_color(0, RED);
            ls_set_color(1, BLACK);
            fc_effect.dream_scene.direction = IS_forward;
            fc_effect.dream_scene.c_n = 2;
        }

        if (dot_cnt >= 300) {
            dot_cnt = 0;
            next_one++;
            next_one %= 21;
            led_strip_rgb_schedule();
        }
    } else {
        dot_cnt = 0;
        next_one = 0;
    }
}
