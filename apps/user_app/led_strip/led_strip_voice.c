#include "led_strip_voice.h"
#include "asm/adc_api.h"
#include "led_strip_driver.h"

#include "WS2812FX.H"
#include "user_include.h"
#include "led_strip_white_schedule.h"
#include "led_strip_rgb_anim.h"

// 标志位，七彩灯触发声控。0--未触发，1--触发
static volatile u8 flag_sound_triggered_in_colorful_lights = 0;
// 标志位，流星灯触发声控。0--未触发，1--触发
static volatile u8 flag_sound_triggered_in_meteor_lights = 0;
// 标志位，电机声控模式下，触发声控，0--未触发，1--触发
static volatile u8 flag_sound_triggered_in_motor = 0;

// 纯白色流星灯使用到的声控触发标志位
static volatile u8 flag_sound_triggered_in_led_strip_white = 0;
// RGB 幻彩灯使用到的声控触发标志位
static volatile u8 flag_sound_triggered_in_led_strip_rgb = 0;

/*
    声控电平（0 ~ 100），供幻彩灯的声控动画读取：
    检测到声音时直接跳到当前的声音强度（快起），没有声音时逐次衰减（慢落）。
    采样周期是 10ms（sound_handle() 在 10ms 任务里调用），
    所以衰减步进取 4，声音停下后大约 0.25s 掉到 0。
*/
static volatile u8 voice_sound_level = 0;
#define VOICE_SOUND_LEVEL_DECAY_STEP 4

/*
    声控「一拍」的判定门限：声音强度（0 ~ 100）达到这个值就算一次节拍。
    灵敏度(fc_effect.music.s)调大时，同样的声音算出来的强度更大，拍子也更密。
    太小 → 拍子太密（节奏一直在起点、滚动一直在加速）；
    太大 → 拍子太少（看起来像没反应）。实测满音量时强度只有 60 左右，取 15。
*/
#define SOUND_BEAT_PERCENT 15

u8 led_strip_voice_get_level(void)
{
    return voice_sound_level;
}

void sound_ctl_init(void)
{
    adc_add_sample_ch(SOUND_CTL_ADC_CHANNEL);
    gpio_set_die(SOUND_CTL_PIN, 0);
    gpio_set_direction(SOUND_CTL_PIN, 1);
    gpio_set_pull_down(SOUND_CTL_PIN, 0);
}

/**
 * @brief 获取声控检测脚对应的ad值
 * 
 * @return u16
 * 
 */
u16 sound_ctl_get_adc_val(void)
{
    return adc_get_value(SOUND_CTL_ADC_CHANNEL);
}

u8 get_sound_triggered_by_led_strip_white(void)
{
    u8 ret = flag_sound_triggered_in_led_strip_white;
    flag_sound_triggered_in_led_strip_white = 0;
    return ret;
}

u8 get_sound_triggered_by_led_strip_rgb(void)
{
    u8 ret = flag_sound_triggered_in_led_strip_rgb;
    flag_sound_triggered_in_led_strip_rgb = 0;
    return ret;
}

// 获取七彩灯的声控结果
u8 get_sound_triggered_by_colorful_lights(void)
{
    u8 ret = flag_sound_triggered_in_colorful_lights;
    flag_sound_triggered_in_colorful_lights = 0;
    return ret;
}

// 获取流星灯的声控结果
u8 get_sound_triggered_by_meteor_lights(void)
{
    u8 ret = flag_sound_triggered_in_meteor_lights;
    flag_sound_triggered_in_meteor_lights = 0;
    return ret;
}

// 电机声控模式下，获取声控结果
// u8 get_sound_triggered_by_motor(void)
u8 sound_triggered_by_motor_get(void)
{
    // u8 ret = flag_sound_triggered_in_motor;
    // flag_sound_triggered_in_motor = 0;
    // return ret;

    return flag_sound_triggered_in_motor;
}

void sound_triggered_by_motor_clear(void)
{
    flag_sound_triggered_in_motor = 0;
}

/**
 * @brief 七彩灯声控模式下的灵敏度 增加
 *      由遥控器调节时调用
 *
 */
void colorful_lights_sound_sensitivity_add(void)
{
    if (IS_light_music != fc_effect.Now_state) {
        return;
    }

    const u8 step = 10;
    if (fc_effect.colorful_lights_sensitivity < 100 - step) {
        fc_effect.colorful_lights_sensitivity += step;
    } else {
        fc_effect.colorful_lights_sensitivity = 100;
    }

    printf("fc_effect.colorful_lights_sensitivity %u\n",
           (u16)fc_effect.colorful_lights_sensitivity);
}

/**
 * @brief 七彩灯声控模式下的灵敏度 减少
 *      由遥控器调节时调用
 *
 */
void colorful_lights_sound_sensitivity_sub(void)
{
    if (IS_light_music != fc_effect.Now_state) {
        return;
    }

    const u8 step = 10;
    if (fc_effect.colorful_lights_sensitivity > step) {
        fc_effect.colorful_lights_sensitivity -= step;
    } else {
        fc_effect.colorful_lights_sensitivity = 0;
    }

    printf("fc_effect.colorful_lights_sensitivity %u\n",
           (u16)fc_effect.colorful_lights_sensitivity);
}

/**
 * @brief 流星灯声控模式下的灵敏度 增加
 *       由遥控器调节时调用
 *
 */
void meteor_lights_sound_sensitivity_add(void)
{
    const u8 step = 10;
    if (fc_effect.meteor_lights_sensitivity < 100 - step) {
        fc_effect.meteor_lights_sensitivity += step;
    } else {
        fc_effect.meteor_lights_sensitivity = 100;
    }

    printf("fc_effect.meteor_lights_sensitivity %u\n",
           (u16)fc_effect.meteor_lights_sensitivity);
}

/**
 * @brief 流星灯声控模式下的灵敏度 减少
 *       由遥控器调节时调用
 *
 */
void meteor_lights_sound_sensitivity_sub(void)
{
    const u8 step = 10;
    if (fc_effect.meteor_lights_sensitivity > step) {
        fc_effect.meteor_lights_sensitivity -= step;
    } else {
        fc_effect.meteor_lights_sensitivity = 0;
    }

    printf("fc_effect.meteor_lights_sensitivity %u\n",
           (u16)fc_effect.meteor_lights_sensitivity);
}

void motor_sound_sensitivity_add(void)
{
    const u8 step = 10;
    if (fc_effect.base_ins.sensitivity < 100 - step) {
        fc_effect.base_ins.sensitivity += step;
    } else {
        fc_effect.base_ins.sensitivity = 100;
    }

#if USER_DEBUG_ENABLE
    printf("motor sensitivity %u\n", (u16)fc_effect.base_ins.sensitivity);
#endif
}

void motor_sound_sensitivity_sub(void)
{
    const u8 step = 10;
    if (fc_effect.base_ins.sensitivity > 0 + step) {
        fc_effect.base_ins.sensitivity -= step;
    } else {
        fc_effect.base_ins.sensitivity = 0;
    }

    printf("motor sensitivity %u\n", (u16)fc_effect.base_ins.sensitivity);
}

void sound_handle(void)
{

#if 1 // 移植其他项目的声控程序

#define SAMPLE_N 20
    static volatile u32 adc_sum = 0;
    static volatile u32 adc_sum_n = 0;
    static volatile u8 adc_v_n = 0;
    static volatile u8 adc_avrg_n = 0;
    static volatile u16 adc_v[SAMPLE_N] = {0};
    static volatile u32 adc_avrg[10] = {0}; // 记录5个平均值
    static volatile u32 adc_total[15] = {0};
    // u8 trg = 0;
    u8 trg_v = 0;
    volatile u16 adc = 0;
    u32 adc_all = 0;
    u32 adc_ttl = 0;

    /*
        声控电平的慢落（快起见下面检测到声音的地方）：
        放在这里是为了「即使本次采集到的 adc 异常、提前 return，电平也能正常衰减」
    */
    if (voice_sound_level > VOICE_SOUND_LEVEL_DECAY_STEP) {
        voice_sound_level -= VOICE_SOUND_LEVEL_DECAY_STEP;
    } else {
        voice_sound_level = 0;
    }

    // 记录adc值
    // 每次进入，采集一次ad值（即使不在声控模式，也会占用一些时间）
    adc = sound_ctl_get_adc_val();

    // printf("adc == %u\n", adc);

    if (adc >= 1000) {
        return;
    }

    if (adc_sum_n < 2000) {
        // 从0开始，一直加到2000，每10ms加一，总共要20s
        adc_sum_n++;
    }

    if (adc_sum_n == 2000) {
        if (adc / (adc_sum / adc_sum_n) > 3)
            return; // adc突变，大于平均值的3倍，丢弃该值

        adc_sum = adc_sum - adc_sum / adc_sum_n;
    }

    adc_sum += adc; // 累加adc值

    adc_v_n %= SAMPLE_N;
    adc_v[adc_v_n] = adc;
    adc_v_n++;
    adc_all = 0;

    // 计算ad值总和
    for (u8 i = 0; i < SAMPLE_N; i++) {
        adc_all += adc_v[i];
    }

    // 获取ad值平均值
    adc_avrg_n %= 10;
    adc_avrg[adc_avrg_n] = adc_all / SAMPLE_N;
    adc_avrg_n++;
    adc_ttl = 0;

    // 在平均值的基础上，再求总和
    for (u8 i = 0; i < 10; i++) {
        adc_ttl += adc_avrg[i];
    }

    memmove(
        (u8 *)adc_total, (u8 *)adc_total + 4,
        14 *
            4); // 将 src 指向的内存区域中的前 n 个字节复制到 dest 指向的内存区域（能够安全地处理内存重叠的情况）

    adc_total[14] = adc_ttl / 10; // 总数平均值
    // trg = 0;

    if (adc_sum_n != 0) {
        if (adc * fc_effect.music.s / 100 > adc_sum / adc_sum_n) {
            u32 adc_sum_avrg = adc_sum / adc_sum_n;

            if (adc > adc_sum_avrg) {
                /*
                    声音强度：本次采集比长期平均值大多少（0 ~ 100）。
                    灵敏度 fc_effect.music.s 越大，同样的声音算出来的强度越大。
                    它是「声控电平」和「声控一拍」的共同来源：
                        - 电平：快起慢落，能量 / 频谱用（led_strip_voice_get_level()）；
                        - 一拍：强度达到 SOUND_BEAT_PERCENT 就算一次节拍，
                          节奏 / 滚动用（get_sound_triggered_by_led_strip_rgb()）。
                */
                u8 adc_percent = (adc - adc_sum_avrg) * fc_effect.music.s / adc;

                if (fc_effect.Now_state == IS_light_music) {
                    // 声控电平的快起：直接跳到本次的声音强度
                    if (adc_percent > voice_sound_level) {
                        voice_sound_level = adc_percent;
                    }

                    /*
                        声控「一拍」：
                        原来这里用的是白光流星灯的灵敏度 led_strip_white.sensitivity，
                        但 led_strip_white_schedule_init() 在本工程里根本没被调用，
                        它一直是 0 → 0 > adc_sum_avrg 永远不成立
                        → 触发标志永远不会置起来，靠「一拍」驱动的声控效果（节奏、频谱）
                        就一直没有反应。现在改成和声控电平用同一个 adc_percent 判定，
                        两边完全一致（灵敏度也只受 fc_effect.music.s 影响）。
                    */
                    if ((adc_percent >= SOUND_BEAT_PERCENT) &&
                        (DEVICE_ON == fc_effect.on_off_flag)) {
                        flag_sound_triggered_in_led_strip_rgb = 1;
                        WS2812FX_triggered_by_led_strip_rgb();
                    }

                    /*
                        下面两句只给旧的声控动画（led_strip_rgb_anim.c 里那批，
                        已经不再被调度）用，保留不动；
                        重新设计的声控动画（led_strip_rgb_scene_anim.c 里的
                        led_strip_rgb_scene_anim_sound_*）读的是声控电平和触发标志。
                    */
                    __led_strip_rgb_anim_sound_control_feq_rise_set__(
                        adc_percent);
                    music_open_close_set_trigger_len(adc_percent);
                }
            }
        }
    }

#endif // 移植其他项目的声控程序
}
