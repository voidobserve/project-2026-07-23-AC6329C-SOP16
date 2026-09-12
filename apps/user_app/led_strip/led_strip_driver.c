#include "led_strip_sys.h"
#include "led_strip_driver.h"
#include "asm/adc_api.h"
#include "led_strand_effect.h"
#include "asm/ledc.h"

#define MIC_PIN IO_PORTA_08
#define MIC_CH  AD_CH_PA8

const struct ledc_platform_data ledc_rgb_light_data = {
    .index = 0,           // 控制器号
    .port = LEDC_RGB_PIN, // 输出引脚
    .idle_level = 0,      // 当前帧的空闲电平，0：低电平， 1：高电平
    .out_inv = 0,         // 起始电平，0：高电平开始， 1：低电平开始
    // 取数据时高低位镜像，0：不镜像，1：8位镜像，2:16位镜像，3:32位镜像
    .bit_inv = 1,
    .t_unit = t_42ns, // 时间单位
    .t1h_cnt = 21,    // 1码的高电平时间 = t1h_cnt * t_unit;21*42=882
    .t1l_cnt = 7,     // 1码的低电平时间 = t1l_cnt * t_unit;7*42=294
    .t0h_cnt = 8,     // 0码的高电平时间 = t0h_cnt * t_unit;8*42=336
    .t0l_cnt = 30,    // 0码的低电平时间 = t0l_cnt * t_unit;*/30*42=1260

    .t_rest_cnt = 20000, // 复位信号时间 = t_rest_cnt * t_unit;20000*42=840000
    .cbfun = NULL,       // 中断回调函数
};

const struct ledc_platform_data ledc_white_light_data = {
    .index = 1,             // 控制器号
    .port = LEDC_WHITE_PIN, // 输出引脚
    .idle_level = 0,        // 当前帧的空闲电平，0：低电平， 1：高电平
    .out_inv = 0,           // 起始电平，0：高电平开始， 1：低电平开始
    // 取数据时高低位镜像，0：不镜像，1：8位镜像，2:16位镜像，3:32位镜像
    .bit_inv = 1,
    .t_unit = t_42ns, // 时间单位
    .t1h_cnt = 21,    // 1码的高电平时间 = t1h_cnt * t_unit;21*42=882
    .t1l_cnt = 7,     // 1码的低电平时间 = t1l_cnt * t_unit;7*42=294
    .t0h_cnt = 8,     // 0码的高电平时间 = t0h_cnt * t_unit;8*42=336
    .t0l_cnt = 30,    // 0码的低电平时间 = t0l_cnt * t_unit;*/30*42=1260

    .t_rest_cnt = 20000, // 复位信号时间 = t_rest_cnt * t_unit;20000*42=840000
    .cbfun = NULL,       // 中断回调函数
};

void led_strip_driver_init(void)
{
    ledc_init(&ledc_rgb_light_data);
    // ledc_init(&ledc_white_light_data);
}

/*********************************mic脚IO口初始化***************************************************************/
#if 0
void mic_gpio_init()
{
    adc_add_sample_ch(MIC_CH); // 注意：初始化AD_KEY之前，先初始化ADC
    gpio_set_die(MIC_PIN, 0);
    gpio_set_direction(MIC_PIN, 1);
    gpio_set_pull_down(MIC_PIN, 0);
}
#endif
