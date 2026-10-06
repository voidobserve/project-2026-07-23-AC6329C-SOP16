// 幻彩灯「场景」调度：场景 -> WS2812FX 段参数的唯一桥梁
#include "led_strip_rgb_scene_schedule.h"

#include <string.h>

#include "WS2812FX.H"
#include "app_msg_handle.h"    // app_msg_anim_info（全局动画参数）
#include "led_strand_effect.h" // 旧的 fc_effect（兼容用）
#include "led_strip_driver.h"  // LED_STRIP_RGB_SEG_INDEX 等
#include "led_strip_rgb_scene.h"
#include "led_strip_rgb_scene_anim.h"
#include "os_cpu.h"
#include "user_config.h"

/*
	app 模式索引 -> 动画实现 + 段选项
	- options 里只写「与模式强相关的固定选项」，例如闪烁类动画的拖尾长度(FADE_*)；
	  方向(REVERSE)由场景的 dir 统一叠加，段大小不再使用 SIZE_*。
*/
typedef struct
{
    u8 mode_idx;   // app 模式索引
    mode_ptr mode; // 动画实现
    u8 options;    // 固定段选项（不含方向）
} led_strip_rgb_scene_mode_entry_t;

// 动画索引和对应的函数映射表
static const led_strip_rgb_scene_mode_entry_t led_strip_rgb_scene_mode_table[] =
    {
        // 静态色
        {APP_MSG_MOD_IDX_STATIC, led_strip_rgb_scene_anim_static, NO_OPTIONS},
        // 渐变
        {APP_MSG_MOD_IDX_GRADUAL, led_strip_rgb_scene_anim_gradual, NO_OPTIONS},
        // 跳变
        {APP_MSG_MOD_IDX_JUMP, led_strip_rgb_scene_anim_jump, NO_OPTIONS},
        // 呼吸
        {APP_MSG_MOD_IDX_BREATH, led_strip_rgb_scene_anim_breath, NO_OPTIONS},
        // 流水
        {APP_MSG_MOD_IDX_RUNNING_WATER, led_strip_rgb_scene_anim_running_water,
         NO_OPTIONS},
        // 堆积
        {APP_MSG_MOD_IDX_ACCUMULATION, led_strip_rgb_scene_anim_accumulation,
         NO_OPTIONS},
        // 星空：随机点闪，用 FADE_* 让亮点留下余晖
        // （其它动画都是每帧整条重画，不调 fade_out，所以不需要 FADE_*）
        {APP_MSG_MOD_IDX_STARRY_SKY, led_strip_rgb_scene_anim_starry_sky,
         FADE_XSLOW},
        // 星云
        {APP_MSG_MOD_IDX_NEBULA, led_strip_rgb_scene_anim_nebula, NO_OPTIONS},
        // 流星/流星雨：每帧整条重画
        {APP_MSG_MOD_IDX_METEOR, led_strip_rgb_scene_anim_meteor, NO_OPTIONS},
        // 流星雨
        {APP_MSG_MOD_IDX_METEOR_SHOWER, led_strip_rgb_scene_anim_meteor_shower,
         NO_OPTIONS},
        // 开合：dir = 0 开幕，dir = 1 闭幕
        {APP_MSG_MOD_IDX_OPENING_AND_CLOSING,
         led_strip_rgb_scene_anim_open_close, NO_OPTIONS},
        // 跑动
        {APP_MSG_MOD_IDX_RUN, led_strip_rgb_scene_anim_run, NO_OPTIONS},
        // 跑动集合：内置多个跑动子动画循环执行
        {APP_MSG_MOD_IDX_RUN_COLLECTION,
         led_strip_rgb_scene_anim_run_collection, NO_OPTIONS},
};

#define LED_STRIP_RGB_SCENE_MODE_NUM                                           \
    (sizeof(led_strip_rgb_scene_mode_table) /                                  \
     sizeof(led_strip_rgb_scene_mode_table[0]))

static const led_strip_rgb_scene_mode_entry_t *
led_strip_rgb_scene_mode_find(u8 mode_idx)
{
    u8 i;

    for (i = 0; i < LED_STRIP_RGB_SCENE_MODE_NUM; i++) {
        if (led_strip_rgb_scene_mode_table[i].mode_idx == mode_idx) {
            return &led_strip_rgb_scene_mode_table[i];
        }
    }

    return NULL;
}

/*
	把一个动画实现 + 段选项 + 帧间隔下发给 WS2812FX 的幻彩灯段，
	并把整条灯带的亮度、动画内部状态一起刷新。
	普通动画模式（app 的动画指令）和声控模式都走这里。
*/
static void led_strip_rgb_scene_start_mode(mode_ptr mode, u8 seg_options,
                                           u16 frame_interval_ms)
{
    /*
		WS2812FX_service() 在 10ms 定时里跑，重配置段参数要和它互斥
	*/
    OS_ENTER_CRITICAL();
    WS2812FX_setSegment_colorOptions(
        LED_STRIP_RGB_SEG_INDEX,                           // 第 0 段（幻彩灯）
        LED_STRIP_RGB_STAR_INDEX,                          // 起始位置
        LED_STRIP_RGB_STAR_INDEX + LED_STRIP_RGB_NUMS - 1, // 结束位置
        mode,                                              // 动画实现
        0,                 // 颜色（新动画自己管理颜色）
        frame_interval_ms, // 速度（帧间隔 ms）
        seg_options);      // 段选项
    WS2812FX_resetSegmentRuntime(LED_STRIP_RGB_SEG_INDEX);
    WS2812FX_running_flag_set();
    OS_EXIT_CRITICAL();

    /*
		整条灯带的亮度由 WS2812FX 统一控制（app 下发的动画亮度）。
		注意：setBrightness() 内部会立刻刷新一次灯带(show)，比较耗时，
		      必须放在临界区外面。
	*/
    WS2812FX_setBrightness(
        led_strip_rgb_scene_brightness_to_255(led_strip_rgb_scene.brightness));

    // 动画内部状态（例如流星雨的位置、频谱的柱高）也要跟着复位
    led_strip_rgb_scene_anim_reset();
}

void led_strip_rgb_scene_apply(void)
{
    const led_strip_rgb_scene_mode_entry_t *entry = NULL;
    mode_ptr mode;
    u8 seg_options = NO_OPTIONS;
    u16 frame_interval_ms;

    if (DEVICE_OFF == led_strip_rgb_scene.pwr_sta) {
        // 关机：跑关灯动画（渐灭）
        mode = &led_strip_rgb_scene_anim_power_off;
        seg_options = FADE_MEDIUM;
    } else {
        entry = led_strip_rgb_scene_mode_find(led_strip_rgb_scene.mode_idx);
        if (NULL == entry) {
            // app 下发了不支持的模式，退化到静态色
            mode = &led_strip_rgb_scene_anim_static;
        } else {
            mode = entry->mode;
            seg_options = entry->options;
        }
    }

    if (led_strip_rgb_scene_is_reverse()) {
        seg_options |= REVERSE;
    }

    frame_interval_ms =
        led_strip_rgb_scene_speed_to_ms(led_strip_rgb_scene.speed);

    led_strip_rgb_scene_start_mode(mode, seg_options, frame_interval_ms);

#if USER_DEBUG_ENABLE

    printf("mode idx == %u\n", (u16)led_strip_rgb_scene.mode_idx);
    printf("dir == %u\n", (u16)led_strip_rgb_scene.dir);
    printf("speed == %u(%ums)\n", (u16)led_strip_rgb_scene.speed,
           frame_interval_ms);
    printf("reserved == %u, leds_per_seg == %u\n",
           (u16)led_strip_rgb_scene.byte_reserved,
           (u16)led_strip_rgb_scene_get_leds_per_seg());
    printf("color_num == %u\n", (u16)led_strip_rgb_scene.color_num);
    printf("background_color == %u-%u-%u\n",
           (u16)led_strip_rgb_scene.background.r,
           (u16)led_strip_rgb_scene.background.g,
           (u16)led_strip_rgb_scene.background.b);
    printf("dev pwr sta == %u\n", (u16)led_strip_rgb_scene.pwr_sta);

#endif
}

/*
	声控模式索引 -> 声控动画实现
	================================================================================
	原来做好的声控动画不符合要求（没有考虑灯串数量），已经废弃，
	这里按「能量 / 节奏 / 频谱 / 滚动」四个主题重新设计，
	动画实现见 led_strip_rgb_scene_anim.c 的 led_strip_rgb_scene_anim_sound_*：
	所有长度/位置都按当前段的真实长度(_seg_len)计算并夹紧，不会超过灯串数量。
	================================================================================
*/
static const mode_ptr led_strip_rgb_scene_sound_mode_table[] = {
    led_strip_rgb_scene_anim_sound_energy,   // 0：能量
    led_strip_rgb_scene_anim_sound_rhythm,   // 1：节奏
    led_strip_rgb_scene_anim_sound_spectrum, // 2：频谱
    led_strip_rgb_scene_anim_sound_scroll,   // 3：滚动
};

#define LED_STRIP_RGB_SCENE_SOUND_MODE_NUM                                     \
    (sizeof(led_strip_rgb_scene_sound_mode_table) /                            \
     sizeof(led_strip_rgb_scene_sound_mode_table[0]))

void led_strip_rgb_scene_sound_apply(u8 sound_mode)
{
    mode_ptr mode;
    u8 seg_options = NO_OPTIONS;

    if (sound_mode >= LED_STRIP_RGB_SCENE_SOUND_MODE_NUM) {
        // app 下发了不支持的声控模式：用第一种，别让灯带黑着
        sound_mode = 0;
    }

    mode = led_strip_rgb_scene_sound_mode_table[sound_mode];
    if (led_strip_rgb_scene_is_reverse()) {
        seg_options |= REVERSE;
    }

    /*
		帧间隔固定成 ANIM_SOUND_FRAME_MS：
		声控动画要跟得上声音，跟着 app 下发的动画速度（往往是最慢）走就没法看了。
	*/
    led_strip_rgb_scene_start_mode(mode, seg_options, ANIM_SOUND_FRAME_MS);

#if USER_DEBUG_ENABLE
    printf("sound mode == %u\n", (u16)sound_mode);
    printf("leds_per_seg == %u, color_num == %u\n",
           (u16)led_strip_rgb_scene_get_leds_per_seg(),
           (u16)led_strip_rgb_scene_get_color_num());
#endif
}

void led_strip_rgb_scene_start(void)
{
    u8 brightness_percent;
    u8 speed_percent;
    u8 on_off;

    /*
		亮度、速度、开关在 fc_effect 里有独立的保存通道（遥控器、DP 协议也会改它们），
		而 led_strip_rgb_scene_init() 会把默认值镜像回 fc_effect，
		所以这里先把旧通道恢复出来的值备份出来，最后再套用。
	*/
    brightness_percent = fc_effect.app_b;
    speed_percent = fc_effect.app_speed;
    on_off = fc_effect.on_off_flag;

    // 1. 先把场景复位到一个确定的默认值
    led_strip_rgb_scene_init();

    /*
		2. 用 app 层保存/恢复的动画参数填充场景
		   （app_msg_anim_info_init() 已经给了默认值，这里直接读全局变量，
		     不在栈上再开一份 app_msg_anim_info_t）
	*/
    led_strip_rgb_scene_set_from_app_info(
        (app_msg_anim_info_t *)&app_msg_anim_info);

    // 3. 亮度/速度/开关以旧通道保存的值为准
    led_strip_rgb_scene_set_brightness(brightness_percent);
    led_strip_rgb_scene_set_speed(speed_percent);
    led_strip_rgb_scene_set_power(on_off);

    // 4. 开跑
    led_strip_rgb_scene_apply();
}

void led_strip_rgb_scene_tick_10ms(void)
{
    /*
		旧通道（遥控器 rf24g_key、DP 协议 dp_data_tran 等）直接修改 fc_effect，
		这里把它们同步进场景，保证新旧通道看到的亮度/速度/开关是一致的。
		注意：这里只同步「不重启动画」的参数和开关，
		      旧通道切换模式（change_type）时仍然由旧调度处理，避免两套调度互相打架。
	*/
    if ((u8)fc_effect.app_b != led_strip_rgb_scene.brightness) {
        led_strip_rgb_scene_set_brightness(fc_effect.app_b);
    }

    if ((u8)fc_effect.app_speed != led_strip_rgb_scene.speed) {
        led_strip_rgb_scene_set_speed(fc_effect.app_speed);
    }

    if ((u8)fc_effect.on_off_flag != led_strip_rgb_scene.pwr_sta) {
        led_strip_rgb_scene_set_power(fc_effect.on_off_flag);
        led_strip_rgb_scene_apply();
    }
}
