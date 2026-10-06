// 幻彩灯「场景」数据层
// 负责：保存 app 下发的动画参数、做范围校验、参数换算、与旧 fc_effect 互相兼容
#include "led_strip_rgb_scene.h"

#include <string.h>

#include "WS2812FX.H"
#include "led_strand_effect.h" // 旧的 fc_effect（兼容用）
#include "led_strip_driver.h"  // LED_STRIP_RGB_SEG_INDEX 等
#include "led_strip_sys.h"     // get_max_speed()
#include "user_config.h"

/*
	亮度百分比 -> 0 ~ 255
	与旧实现保持一致：最低给 25，避免 app 把亮度调到 0 之后灯带完全黑掉，
	用户以为设备坏了。
*/
#define SCENE_BRIGHTNESS_MIN 25

// 速度百分比 -> 帧间隔(ms)：0% 最慢(500ms)，100% 最快(get_max_speed())
#define SCENE_SPEED_SLOWEST 500

// WS2812FX 一帧最快 10ms（见 WS2812FX_service 中的 SPEED_MIN）
#define SCENE_FRAME_MS_MIN 10

volatile led_strip_rgb_scene_t led_strip_rgb_scene;

/* ------------------------------------------------------------------------------ */
/*                                   内部工具                                      */
/* ------------------------------------------------------------------------------ */

static u8 scene_limit_percent(u8 percent)
{
    if (percent > 100) {
        return 100;
    }
    return percent;
}

/*
	把场景中的关键字段同步到旧的 fc_effect：
	遥控器、DP 协议、白光流星灯、app 状态同步、flash 保存等旧代码都还在读 fc_effect，
	同步一份出去可以保证新旧通道看到的灯带状态是一致的。
*/
static void scene_mirror_to_fc_effect(void)
{
    u8 color_index;
    u8 color_num;

    fc_effect.on_off_flag = led_strip_rgb_scene.pwr_sta;
    fc_effect.app_b = led_strip_rgb_scene.brightness;
    fc_effect.b =
        led_strip_rgb_scene_brightness_to_255(led_strip_rgb_scene.brightness);
    fc_effect.app_speed = led_strip_rgb_scene.speed;
    fc_effect.dream_scene.speed =
        led_strip_rgb_scene_speed_to_ms(led_strip_rgb_scene.speed);

    fc_effect.dream_scene.direction =
        led_strip_rgb_scene.dir ? IS_back : IS_forward;
    /*
        旧通道(fc_effect)的动画会把 seg_size 直接当作循环步长，
        seg_size = 0 会让它们死循环/除零，所以镜像给旧通道时 0 按 1 个灯处理；
        新场景里仍然保留 0 表示「所有灯为同一组」。
    */
    fc_effect.dream_scene.seg_size =
        led_strip_rgb_scene.leds_per_seg ? led_strip_rgb_scene.leds_per_seg : 1;

    // 颜色池：fc_effect 的颜色池只有 MAX_NUM_COLORS 个，超出部分直接丢弃
    color_num = led_strip_rgb_scene_get_color_num();
    if (color_num > MAX_NUM_COLORS) {
        color_num = MAX_NUM_COLORS;
    }
    fc_effect.dream_scene.c_n = color_num;
    for (color_index = 0; color_index < color_num; color_index++) {
        fc_effect.dream_scene.rgb[color_index] =
            led_strip_rgb_scene.colors[color_index];
    }

    fc_effect.Now_state =
        (APP_MSG_MOD_IDX_STATIC == led_strip_rgb_scene.mode_idx)
            ? IS_STATIC
            : IS_light_scene;
}

/* ------------------------------------------------------------------------------ */
/*                                   参数换算                                      */
/* ------------------------------------------------------------------------------ */

u16 led_strip_rgb_scene_speed_to_ms(u8 percent)
{
    u16 speed;
    u16 fastest;

    percent = scene_limit_percent(percent);

    /*
		速度百分比越大 -> 帧间隔越小 -> 动画越快
		0% -> 500ms，100% -> 0ms（下面再钳到硬件能跑的最快速度）
	*/
    speed =
        (u16)(SCENE_SPEED_SLOWEST - ((u32)SCENE_SPEED_SLOWEST * percent / 100));

    fastest = get_max_speed();
    if (fastest < SCENE_FRAME_MS_MIN) {
        fastest = SCENE_FRAME_MS_MIN;
    }
    if (speed < fastest) {
        speed = fastest;
    }

    return speed;
}

u8 led_strip_rgb_scene_brightness_to_255(u8 percent)
{
    percent = scene_limit_percent(percent);
    return (u8)((u16)percent * (255 - SCENE_BRIGHTNESS_MIN) / 100 +
                SCENE_BRIGHTNESS_MIN);
}

/* ------------------------------------------------------------------------------ */
/*                                   有效值                                        */
/* ------------------------------------------------------------------------------ */

u8 led_strip_rgb_scene_get_leds_per_seg(void)
{
    /*
		原样返回 app 下发的「多少个灯为一组」（协议 byte7）：
			- byte7 = 0  表示「所有灯为同一组」；
			- byte7 超过当前设备所有灯的数量时，也表示「所有灯为同一组」；

		这两种「同一组」都需要知道真实的灯数(_seg_len)才能换算，
		场景层拿不到（也不能用 fc_effect.led_num 去夹：它可能偏小、或者 app 还没下发，
		之前把 app 下发的 5 夹成 1，动画就变成「每个灯一种颜色」），
		所以统一交给动画层的 anim_leds_per_seg() 换算成整条灯带。
	*/
    return led_strip_rgb_scene.leds_per_seg;
}

u8 led_strip_rgb_scene_get_color_num(void)
{
    u8 color_num = led_strip_rgb_scene.color_num;

    if (0 == color_num) {
        color_num = 1;
    }
    if (color_num > LED_STRIP_RGB_SCENE_COLOR_MAX) {
        color_num = LED_STRIP_RGB_SCENE_COLOR_MAX;
    }

    return color_num;
}

color_t led_strip_rgb_scene_get_color(u8 color_index)
{
    color_index %= led_strip_rgb_scene_get_color_num();
    return led_strip_rgb_scene.colors[color_index];
}

color_t led_strip_rgb_scene_get_background(void)
{
    return led_strip_rgb_scene.background;
}

u8 led_strip_rgb_scene_is_reverse(void)
{
    return led_strip_rgb_scene.dir ? 1 : 0;
}

/* ------------------------------------------------------------------------------ */
/*                                   写入接口                                      */
/* ------------------------------------------------------------------------------ */

void led_strip_rgb_scene_init(void)
{
    memset((void *)&led_strip_rgb_scene, 0, sizeof(led_strip_rgb_scene));

    led_strip_rgb_scene.pwr_sta = DEVICE_ON;
    led_strip_rgb_scene.mode_idx = APP_MSG_MOD_IDX_STATIC;
    led_strip_rgb_scene.dir = LED_STRIP_RGB_SCENE_DIR_FORWARD;
    led_strip_rgb_scene.brightness = 100;  // 百分比
    led_strip_rgb_scene.speed = 80;        // 百分比
    led_strip_rgb_scene.byte_reserved = 0; // 保留字节，未使用
    led_strip_rgb_scene.leds_per_seg = 1;  // 多少个灯为一组

    // 底色默认黑色
    led_strip_rgb_scene.background.r = 0;
    led_strip_rgb_scene.background.g = 0;
    led_strip_rgb_scene.background.b = 0;

    // 默认静态红色
    led_strip_rgb_scene.color_num = 1;
    led_strip_rgb_scene.colors[0].r = 255;
    led_strip_rgb_scene.colors[0].g = 0;
    led_strip_rgb_scene.colors[0].b = 0;

    scene_mirror_to_fc_effect();
}

void led_strip_rgb_scene_set_from_app_info(const app_msg_anim_info_t *info)
{
    u8 color_index;
    u8 color_num;

    if (NULL == info) {
        return;
    }

    /*
        多少个灯为一组（协议 byte7）：
        byte7 = 0（或超过设备灯数）都表示「所有灯为同一组」，
        这里原样保存，由动画层按真实的段长度(_seg_len)换算成整条灯带。
    */
    led_strip_rgb_scene.leds_per_seg = info->leds_per_seg;
    led_strip_rgb_scene.byte_reserved = 0;

    // 方向
    led_strip_rgb_scene.dir = info->anim_dir ? LED_STRIP_RGB_SCENE_DIR_REVERSE
                                             : LED_STRIP_RGB_SCENE_DIR_FORWARD;

    // 亮度 / 速度（百分比）
    led_strip_rgb_scene.brightness = scene_limit_percent(info->anim_brightness);
    led_strip_rgb_scene.speed = scene_limit_percent(info->anim_speed);

    // 模式索引
    led_strip_rgb_scene.mode_idx = info->mode_idx;

    // 底色
    led_strip_rgb_scene.background.r = info->background_color_r;
    led_strip_rgb_scene.background.g = info->background_color_g;
    led_strip_rgb_scene.background.b = info->background_color_b;

    // 颜色池
    color_num = info->color_num;
    if (color_num > LED_STRIP_RGB_SCENE_COLOR_MAX) {
        color_num = LED_STRIP_RGB_SCENE_COLOR_MAX;
    }
    led_strip_rgb_scene.color_num = color_num;
    for (color_index = 0; color_index < color_num; color_index++) {
        led_strip_rgb_scene.colors[color_index].r =
            info->color_buf[color_index * 3 + 0];
        led_strip_rgb_scene.colors[color_index].g =
            info->color_buf[color_index * 3 + 1];
        led_strip_rgb_scene.colors[color_index].b =
            info->color_buf[color_index * 3 + 2];
    }

    /*
		静态色模式至少要有一个颜色，否则灯带会全黑（app 传错数据时不至于黑屏）
	*/
    if (APP_MSG_MOD_IDX_STATIC == led_strip_rgb_scene.mode_idx &&
        0 == led_strip_rgb_scene.color_num) {
        led_strip_rgb_scene.color_num = 1;
        led_strip_rgb_scene.colors[0].r = 255;
        led_strip_rgb_scene.colors[0].g = 0;
        led_strip_rgb_scene.colors[0].b = 0;
    }

    scene_mirror_to_fc_effect();

    // 亮度、速度是「不重启动画」就能生效的，这里立刻作用到灯带上
    WS2812FX_setBrightness(
        led_strip_rgb_scene_brightness_to_255(led_strip_rgb_scene.brightness));
    WS2812FX_setSpeed_seg(
        LED_STRIP_RGB_SEG_INDEX,
        led_strip_rgb_scene_speed_to_ms(led_strip_rgb_scene.speed));
}

void led_strip_rgb_scene_to_app_info(app_msg_anim_info_t *info)
{
    u8 color_index;
    u8 color_num;

    if (NULL == info) {
        return;
    }

    memset(info, 0, sizeof(app_msg_anim_info_t));

    info->format_head = 0x7FFF; // 幻彩灯动画参数的格式头
    info->mode_idx = led_strip_rgb_scene.mode_idx;
    info->anim_dir = led_strip_rgb_scene_is_reverse();
    info->anim_speed = led_strip_rgb_scene.speed;
    info->anim_brightness = led_strip_rgb_scene.brightness;
    info->byte_reserved = 0; // byte6：保留字节，固定填 0
    info->leds_per_seg =
        led_strip_rgb_scene_get_leds_per_seg(); // byte7：多少个灯为一组

    info->background_color_r = led_strip_rgb_scene.background.r;
    info->background_color_g = led_strip_rgb_scene.background.g;
    info->background_color_b = led_strip_rgb_scene.background.b;

    color_num = led_strip_rgb_scene.color_num;
    if (color_num > LED_STRIP_RGB_SCENE_COLOR_MAX) {
        color_num = LED_STRIP_RGB_SCENE_COLOR_MAX;
    }
    info->color_num = color_num;
    for (color_index = 0; color_index < color_num; color_index++) {
        info->color_buf[color_index * 3 + 0] =
            led_strip_rgb_scene.colors[color_index].r;
        info->color_buf[color_index * 3 + 1] =
            led_strip_rgb_scene.colors[color_index].g;
        info->color_buf[color_index * 3 + 2] =
            led_strip_rgb_scene.colors[color_index].b;
    }
}

void led_strip_rgb_scene_set_power(u8 on_off)
{
    led_strip_rgb_scene.pwr_sta = on_off ? DEVICE_ON : DEVICE_OFF;
    fc_effect.on_off_flag = led_strip_rgb_scene.pwr_sta;
}

void led_strip_rgb_scene_set_brightness(u8 percent)
{
    led_strip_rgb_scene.brightness = scene_limit_percent(percent);

    fc_effect.app_b = led_strip_rgb_scene.brightness;
    fc_effect.b =
        led_strip_rgb_scene_brightness_to_255(led_strip_rgb_scene.brightness);
    WS2812FX_setBrightness(fc_effect.b);
}

void led_strip_rgb_scene_set_speed(u8 percent)
{
    led_strip_rgb_scene.speed = scene_limit_percent(percent);

    fc_effect.app_speed = led_strip_rgb_scene.speed;
    fc_effect.dream_scene.speed =
        led_strip_rgb_scene_speed_to_ms(led_strip_rgb_scene.speed);
    // 只改帧间隔，不重启动画，动画可以从当前位置继续跑
    WS2812FX_setSpeed_seg(LED_STRIP_RGB_SEG_INDEX, fc_effect.dream_scene.speed);
}

void led_strip_rgb_scene_set_static_color(u8 r, u8 g, u8 b)
{
    led_strip_rgb_scene.mode_idx = APP_MSG_MOD_IDX_STATIC;
    led_strip_rgb_scene.color_num = 1;
    led_strip_rgb_scene.colors[0].r = r;
    led_strip_rgb_scene.colors[0].g = g;
    led_strip_rgb_scene.colors[0].b = b;

    scene_mirror_to_fc_effect();
}

void led_strip_rgb_scene_set_direction(u8 dir)
{
    led_strip_rgb_scene.dir =
        dir ? LED_STRIP_RGB_SCENE_DIR_REVERSE : LED_STRIP_RGB_SCENE_DIR_FORWARD;
    fc_effect.dream_scene.direction =
        led_strip_rgb_scene.dir ? IS_back : IS_forward;
}

void led_strip_rgb_scene_set_colors(const color_t *colors, u8 color_num,
                                    color_t background)
{
    u8 color_index;

    if (NULL == colors) {
        return;
    }

    if (color_num > LED_STRIP_RGB_SCENE_COLOR_MAX) {
        color_num = LED_STRIP_RGB_SCENE_COLOR_MAX;
    }

    led_strip_rgb_scene.color_num = color_num;
    for (color_index = 0; color_index < color_num; color_index++) {
        led_strip_rgb_scene.colors[color_index] = colors[color_index];
    }
    led_strip_rgb_scene.background = background;

    scene_mirror_to_fc_effect();
}
