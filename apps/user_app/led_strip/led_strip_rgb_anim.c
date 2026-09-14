#include "led_strip_rgb_anim.h"
#include "WS2812FX.H"
#include "led_strip_rgb_schedule.h"
#include "led_strip_driver.h"
#include "ws2812fx_tool.h"
#include "Adafruit_NeoPixel.H"

volatile music_fs_t m_fs = {
    .rise_tag = 40,
    .bgc = GRAY,
    .act = E_TOP,
};

music_open_close_state_t music_open_close_state = {.target_len = 15};

// 单色流星
/**
 * @brief
 *
 * @return uint16_t
 */
uint16_t led_strip_rgb_anim_single_color_meteor(void)
{
    u8 offset;
    offset = 13;
    static uint8_t temp_color = 100;
    uint32_t r1, g1, b1, w1;

    uint8_t rate = 0;

    uint8_t size = 1 << SIZE_OPTION;
    uint8_t meteor_len = 2;
    if (size == 1) {
        meteor_len = 10;
    } else if (size == 2) {
        meteor_len = 5;
    } else if (size == 4) {
        meteor_len = 2;
    } else if (size == 8) {
        meteor_len = 1;
    }

    if (_seg_rt->counter_mode_step ==
        0) // 该判断放在这里，解决效果切换时，立即切换
    {
        temp_color = 100;
        SET_CYCLE;
        // fc_effect.mode_cycle = 1;
    }

    if ((get_effect_p() == 1) &&
        (fc_effect.mode_cycle == 1)) // 计时中 && 完成一个循环
    {
        return (_seg->speed);
    }

    rate = temp_color;
    int w = (_seg->colors[_seg_rt->aux_param] >> 24) & 0xff;
    int r = (_seg->colors[_seg_rt->aux_param] >> 16) & 0xff;
    int g = (_seg->colors[_seg_rt->aux_param] >> 8) & 0xff;
    int b = _seg->colors[_seg_rt->aux_param] & 0xff;

    if (IS_REVERSE) // 反向
    {
        WS2812FX_copyPixels(_seg->start, _seg->start + 1, _seg_len - 1);
    } else {
        WS2812FX_copyPixels(_seg->start + 1, _seg->start, _seg_len - 1);
    }

    r1 = r * rate / 100;
    g1 = g * rate / 100;
    b1 = b * rate / 100;
    w1 = w * rate / 100;

    if (temp_color >= meteor_len)
        temp_color -= meteor_len;
    else
        temp_color = 0;
    if (IS_REVERSE) // 反向
    {
        WS2812FX_setPixelColor_rgbw(_seg->stop, r1, g1, b1, w1);
    } else {
        WS2812FX_setPixelColor_rgbw(_seg->start, r1, g1, b1, w1);
    }

    _seg_rt->counter_mode_step =
        (_seg_rt->counter_mode_step + 1) % (_seg_len + offset);
    if (_seg_rt->counter_mode_step == 0) {
        SET_CYCLE;
        fc_effect.mode_cycle = 1;
        temp_color = 100;
    }

    return (_seg->speed);
}

u16 led_strip_rgb_anim_mutil_fade(void)
{
    uint8_t size = fc_effect.dream_scene.seg_size;
    uint16_t j;
    uint8_t cnt0 = 0, cnt1 = 1;
    uint32_t color, color1, color0;
    static uint32_t c1[MAX_NUM_COLORS];
    int lum = _seg_rt->counter_mode_step;

    if (size > (_seg->stop - _seg->start) && size == 0)
        return 0;

    if (lum > 255)
        lum = 511 - lum; // lum = 0 -> 255 -> 0
    _seg_rt->aux_param = 0;
    _seg_rt->aux_param2 = 0;

    if (_seg_rt->aux_param3 == 0) {
        _seg_rt->aux_param3 = 1;
        memcpy(c1, _seg->colors, MAX_NUM_COLORS * 4);
    }

    // while (_seg_rt->aux_param2 < _seg->stop)
    while (_seg_rt->aux_param2 < _seg_len) {
        color0 = _seg->colors[cnt0];
        cnt0++;
        cnt0 %= _seg->c_n;
        color1 = c1[cnt1];
        cnt1++;
        cnt1 %= _seg->c_n;
        color = WS2812FX_color_blend(color1, color0, lum);
        for (j = 0; j < size; j++) {
            WS2812FX_setPixelColor(_seg->start + _seg_rt->aux_param2, color);
            _seg_rt->aux_param2++;
            // if (_seg_rt->aux_param2 > _seg->stop)
            if (_seg_rt->aux_param2 > _seg_len) {
                break;
            }
        }
    }

    _seg_rt->counter_mode_step += 4;

    // 此时颜色停留在color1
    if (_seg_rt->counter_mode_step > 511) {
        _seg_rt->counter_mode_step = 0;
        // color0的颜色池左移1

        SET_CYCLE;
    }

    // 此时颜色停留在color1,把color0颜色变换,color0向左转盘
    if (_seg_rt->counter_mode_step == 0) {
        uint32_t c_tmp;
        c_tmp = _seg->colors[0];

        memmove(_seg->colors, _seg->colors + 1, (_seg->c_n - 1) * 4);

        _seg->colors[_seg->c_n - 1] = c_tmp;
    }

    // 此时颜色停留再color0
    if (_seg_rt->counter_mode_step == 256) {
        // color1的颜色池左移1
        uint32_t c_tmp;
        c_tmp = c1[0];
        memmove(&c1[0], &c1[1], (_seg->c_n - 1) * 4);

        c1[_seg->c_n - 1] = c_tmp;
    }
    return (_seg->speed / 32);
}

// 功能：颜色块跳变效果，多个颜色块组成背景,以块为单位步进做流水,
// _seg->c_n:有效颜色数量
// SIZE_OPTION：决定颜色块大小
// IS_REVERSE:0 反向流水 ；1正向流水，WS2812FX_setOptions(REVERSE)来设置
u16 led_strip_rgb_anim_single_block_scan(void)
{
    uint8_t size = fc_effect.dream_scene.seg_size;
    uint8_t j;
    uint32_t c;
    // printf("\n b=%d",Adafruit_NeoPixel_getBrightness());
    if (size > (_seg->stop - _seg->start))
        return 0;

    _seg_rt->counter_mode_step = 0;
    _seg_rt->aux_param = 0;
    // while (_seg_rt->counter_mode_step < _seg->stop)
    while (_seg_rt->counter_mode_step < _seg_len) {
        for (j = 0; j < size; j++) {
            if (IS_REVERSE) {
                WS2812FX_setPixelColor(_seg->start + _seg_rt->counter_mode_step,
                                       _seg->colors[_seg_rt->aux_param]);
            } else {
                WS2812FX_setPixelColor(
                    // _seg->stop - _seg_rt->counter_mode_step,
                    _seg_len - _seg_rt->counter_mode_step,
                    _seg->colors[_seg_rt->aux_param]);
            }
            _seg_rt->counter_mode_step++;
            // if (_seg_rt->counter_mode_step > _seg->stop)
            if (_seg_rt->counter_mode_step > _seg_len) {
                break;
            }
        }
        _seg_rt->aux_param++;
        _seg_rt->aux_param %= _seg->c_n;
    }

    c = _seg->colors[0];
    // 重新开始，对颜色转盘
    for (j = 1; j < _seg->c_n; j++) {
        // 把后面的颜色提前
        _seg->colors[j - 1] = _seg->colors[j];
    }
    _seg->colors[j - 1] = c;

    return _seg->speed * 4;
}

u16 led_strip_rgb_anim_breath(void)
{
    int lum = _seg_rt->counter_mode_step;
    if (lum > 255)
        lum = 511 - lum; // lum = 15 -> 255 -> 15

    uint32_t color =
        WS2812FX_color_blend(_seg->colors[1], _seg->colors[0], lum);
    Adafruit_NeoPixel_fill(color, _seg->start, _seg_len);

    if (_seg_rt->counter_mode_step < 35) {
        _seg_rt->counter_mode_step += 1;
    } else
        _seg_rt->counter_mode_step += 2; // 不能修改+2，否则呼吸有明显的不流畅

    if (_seg_rt->counter_mode_step > (512 - 5)) {
        _seg_rt->counter_mode_step = 5;
        SET_CYCLE;
    }
    return _seg->speed;
}

uint16_t led_strip_rgb_anim_mutil_breath(void)
{
    uint8_t size = fc_effect.dream_scene.seg_size;
    uint8_t j;
    uint16_t lum = _seg_rt->aux_param3;
    uint32_t color;

    // printf("size == %u\n", (u16)size);
    // printf("_seg->c_n == %u\n", (u16)_seg->c_n);
    // printf("_seg->stop == %u\n", (u16)_seg->stop);
    // printf("_seg_rt->counter_mode_step == %u\n", (u16)_seg_rt->counter_mode_step);

    if (lum > 255) {
        lum = 511 - lum;
    }

    if (size > (_seg->stop - _seg->start))
        return 0;

    _seg_rt->counter_mode_step = 0;
    _seg_rt->aux_param = 0;
    while (_seg_rt->counter_mode_step <= _seg->stop) // 遍历整条灯带
    {
        for (j = 0; (j < size) && (_seg_rt->counter_mode_step <= _seg->stop);
             j++) // 遍历颜色块
        {
            color =
                WS2812FX_color_blend(0, _seg->colors[_seg_rt->aux_param], lum);
            WS2812FX_setPixelColor(_seg->start + _seg_rt->counter_mode_step,
                                   color);
            _seg_rt->counter_mode_step++;

            // printf("_seg_rt->counter_mode_step == %u\n", (u16)_seg_rt->counter_mode_step);
            // printf("color == %x\n", color);
        }

        _seg_rt->aux_param++; // 切换到下一个颜色索引
        _seg_rt->aux_param %= _seg->c_n;
    }

    _seg_rt->aux_param3 += 2;
    if (_seg_rt->aux_param3 > (512 - 5)) {
        _seg_rt->aux_param3 = 5;
    }

    return _seg->speed;
}

u16 led_strip_rgb_anim_breathing(void)
{

    static u32 dest_color = BLACK;            // 目标颜色
    volatile u8 brightness_max = fc_effect.b; // 呼吸模式能达到的最大亮度值
    volatile u16 anim_speed = 0;              // 当前模式的速度值
    /*
		每个步骤用时至少10ms，因为ws2812fx_service() 10ms调用一次

		从 0 到 511，
		步长为1，共512个步骤，至少 5120 ms 完成一次循环
		步长为2，共256个步骤，至少 2560 ms 完成一次循环

		那么速度值与循环的关系
		一次循环的时间 == 步骤 * 10ms
		一次循环的时间 == 512 / 步长 * 10ms
		速度值 == 512 / 步长 * 10ms
		步长 == 512 * 10ms / 速度值


		如果是从 0 到 指定亮度(brightness)
		步长为1，共 brightness + 1 步，至少 brightness * 10 ms 完成一次循环
		步长为2，共 (brightness + 1) / 2 步，至少 brightness * 10 ms / 2 完成一次循环

		速度值与亮度值的关系
		一次循环的时间 == (brightness + 1) / 步长 * 10ms
		速度值 == (brightness + 1) / 步长 * 10ms
		(brightness + 1) / 步长 == 速度值 / 10ms
		(brightness + 1) == 速度值 / 10ms * 步长
		步长 == (brightness + 1) * 10ms / 速度值
	*/
    // u16 step = 0; // 步长
    // step = 512 * 10 / _seg->speed;

    static volatile u32 temp_step =
        0; // 累计放大了1000倍的步长，超过1000后，才执行动画的下一步骤
    static volatile u16 brightness = 0; // 亮度值
    u32 step = 0;                       // 步长

    // 将速度值映射到 10 ~ 500 ，值越大，速度越小
    // anim_speed = 500 - (u32)fc_effect.app_speed * (500 - 10) / 100;

    anim_speed = 5000 - (u32)fc_effect.app_speed * (5000 - 200) / 100;

    // 步长，放大了 1000 倍
    step = ((u32)brightness_max + 1) * 10 * 1000 / anim_speed;

    if (0 == _seg_rt->counter_mode_step && 0 == _seg_rt->aux_param &&
        0 == _seg_rt->counter_mode_call) {
        /*
			如果是第一次进入，设置默认颜色
			当前颜色为黑色，向目标颜色渐变（看起来像呼吸渐亮）
		*/
        dest_color = _seg->colors[_seg_rt->aux_param];
        brightness = 0;
        temp_step = 0;
        // Adafruit_NeoPixel_fill(BLACK, _seg->start, _seg_len);
    }

    temp_step += step;
    if (temp_step >= 1000) {
        // 有可能单次的步长会超过1000，这里用循环来逐个递减
        while (1) {
            if (temp_step < 1000) {
                break;
            }

            /*
				没有固定最大亮度的呼吸：
				brightness 变化范围： 0 -> brightness -> 0
			*/
            _seg_rt->counter_mode_step++;
            if (temp_step >= 1000) {
                temp_step -= 1000;
            } else {
                temp_step = 0;
            }

            brightness = _seg_rt->counter_mode_step;
            if (brightness > (u16)brightness_max) {
                brightness = ((u16)brightness_max * 2) - brightness;
            }

            /*
				0 -> brightness_max，共 brightness_max 个步骤，灯光渐亮
				brightness_max -> 0，共 brightness_max 个步骤，灯光渐暗
			*/
            if (_seg_rt->counter_mode_step >= ((u32)brightness_max * 2)) {
                _seg_rt->counter_mode_step = 0;
                temp_step = 0;
                brightness = 0;

                _seg_rt->aux_param +=
                    1; // 切换颜色数组 _seg->colors[] 中的下一个颜色
                if (_seg_rt->aux_param >= _seg->c_n) {
                    _seg_rt->aux_param = 0;
                }

                dest_color = _seg->colors[_seg_rt->aux_param];
                // Adafruit_NeoPixel_fill(BLACK, _seg->start, _seg_len); // 防止动画最后没有熄灭灯光
                // dest_color = WS2812FX_color_blend(BLACK, _seg->colors[_seg_rt->aux_param], (u8)fc_effect.b);

                // printf("__LINE__ %d\n", __LINE__);
                SET_CYCLE;
            }
        }
    }

    u32 color = WS2812FX_color_blend(BLACK, dest_color, (u8)brightness);
    Adafruit_NeoPixel_fill(color, _seg->start, _seg_len);

    // printf("brightness %u\n", (u16)brightness); //
    // printf("temp_step %lu\n", (u32)temp_step);  // 打印为0
    // printf("step %lu\n", (u32)step);
    // printf("_seg_rt->counter_mode_step %lu\n", (u32)_seg_rt->counter_mode_step);

    return 1; // ws2812fx_service() 10ms调用一次，这个值只需要小于10
}

uint16_t led_strip_rgb_anim_mutil_twihkle(void)
{
    uint8_t size = (SIZE_OPTION << 1) + 1;
    uint8_t j;
    if (size > (_seg->stop - _seg->start))
        return 0;

    _seg_rt->counter_mode_step = 0;
    _seg_rt->aux_param = 0;
    if (_seg_rt->aux_param3) {
        while (_seg_rt->counter_mode_step <= _seg->stop) {
            for (j = 0;
                 (j < size) && (_seg_rt->counter_mode_step <= _seg->stop);
                 j++) {
                WS2812FX_setPixelColor(_seg->start + _seg_rt->counter_mode_step,
                                       _seg->colors[_seg_rt->aux_param]);
                _seg_rt->counter_mode_step++;
            }
            _seg_rt->aux_param++;
            _seg_rt->aux_param %= _seg->c_n;
        }
    } else {
        Adafruit_NeoPixel_fill(BLACK, _seg->start, _seg_len);
    }

    _seg_rt->aux_param3 = !_seg_rt->aux_param3;

    return _seg->speed;
}

uint16_t led_strip_rgb_anim_multi_block_scan(void)
{
    uint8_t size = (SIZE_OPTION << 1) + 1;
    uint8_t j;
    uint16_t i;
    if (size > (_seg->stop - _seg->start))
        return 0;

    if (IS_REVERSE) // 反向流水
    {
        WS2812FX_copyPixels(_seg->start, _seg->start + 1, (_seg_len - 1));
        WS2812FX_setPixelColor(_seg->stop, _seg->colors[_seg_rt->aux_param]);
    } else {
        WS2812FX_copyPixels(_seg->start + 1, _seg->start, (_seg_len - 1));
        WS2812FX_setPixelColor(_seg->start, _seg->colors[_seg_rt->aux_param]);
    }

    _seg_rt->counter_mode_step++;

    _seg_rt->aux_param3++;
    if (_seg_rt->aux_param3 >= size) {
        _seg_rt->aux_param3 = 0;
        _seg_rt->aux_param++;
        _seg_rt->aux_param %= _seg->c_n;
    }

    _seg_rt->counter_mode_step %= _seg_len;

    return _seg->speed;
}

u16 led_strip_rgb_anim_multi_colors_gradual(void)
{
    static uint8_t index;
    uint32_t rgb;
    static uint32_t c0, c1;
    int lum = _seg_rt->counter_mode_step;
    if (lum > 255)
        lum = 511 - lum; // lum = 0 -> 255 -> 0
    if (_seg_rt->aux_param == 0) {
        _seg_rt->aux_param = 1;
        index = 0;
        c1 = _seg->colors[index];
        index++;
        c0 = _seg->colors[index];
    }
    // _seg->colors[1]:目标颜色
    uint32_t color = WS2812FX_color_blend(c1, c0, lum);

    Adafruit_NeoPixel_fill(color, _seg->start, _seg_len);

    if (_seg_rt->counter_mode_step == 256) {
        index++;
        index %= _seg->c_n;
        c1 = _seg->colors[index];
    }

    _seg_rt->counter_mode_step++;
    if (_seg_rt->counter_mode_step > 511) {
        _seg_rt->counter_mode_step = 0;
        index++;
        index %= _seg->c_n;
        c0 = _seg->colors[index];
        SET_CYCLE;
    }

    return (_seg->speed / 5);
}

/*
 * Fire flicker function
 实现随机位置闪烁，星空效果
 */
u16 led_strip_rgb_anim_fire_flicker(int rev_intensity)
{
    uint32_t c;
    _seg->colors[1] = 0;
    WS2812FX_fade_out();
    if (_seg->colors[0] == 0) {
        c = WS2812FX_color_wheel(WS2812FX_random8());
    } else {
        c = _seg->colors[0];
    }
    uint8_t w = (c >> 24) & 0xFF;
    uint8_t r = (c >> 16) & 0xFF;
    uint8_t g = (c >> 8) & 0xFF;
    uint8_t b = (c & 0xFF);
    uint8_t lum = max(w, max(r, max(g, b))) / rev_intensity;

    for (uint16_t i = 0; i <= 3; i++) {
        int flicker = WS2812FX_random8_lim(lum);

        WS2812FX_setPixelColor_rgbw(
            WS2812FX_random16_lim(
                _seg_len), // 按随机位置点亮灯光，范围：0 ~ 5（共 _seg_len 个灯光）
            max(r - flicker, 0), max(g - flicker, 0), max(b - flicker, 0),
            max(w - flicker, 0));
    }

    SET_CYCLE;
    return (_seg->speed);
}

/*
 * Random flickering, more intensity.
 */
uint16_t led_strip_rgb_anim_fire_flicker_intense(void)
{
    return led_strip_rgb_anim_fire_flicker(1);
}

u16 led_strip_rgb_anim_meteor_signle_color(void)
{
    const u8 max_rate = 17;
    uint16_t index;
    static uint8_t i = 0;
    uint32_t r1, g1, b1, w1;
    const uint8_t rate[max_rate] = {100, 75, 45, 30, 20, 15, 10, 7, 5,
                                    3,   2,  0,  0,  0,  0,  0,  0};
    _seg_rt->aux_param3++;
    if (_seg_rt->aux_param3 < 9) {
        i = 0;
        // _seg->options |= FADE_XSLOW;
        /* 渐灭当前灯 */
        // WS2812FX_fade_out_targetColor( BLACK);
        return (_seg->speed);
    }

    int w = (_seg->colors[_seg_rt->aux_param] >> 24) & 0xff;
    int r = (_seg->colors[_seg_rt->aux_param] >> 16) & 0xff;
    int g = (_seg->colors[_seg_rt->aux_param] >> 8) & 0xff;
    int b = _seg->colors[_seg_rt->aux_param] & 0xff;
    if (IS_REVERSE) // 反向
    {
        WS2812FX_copyPixels(_seg->start, _seg->start + 1, _seg_len - 1);
    } else {
        WS2812FX_copyPixels(_seg->start + 1, _seg->start, _seg_len - 1);
    }

    // if(_seg_rt->counter_mode_step == 0)
    // for( index = _seg->start; index < _seg->stop; index++)
    index = _seg_rt->counter_mode_step;
    {
        r1 = r * rate[i] / 100;
        g1 = g * rate[i] / 100;
        b1 = b * rate[i] / 100;
        w1 = w * rate[i] / 100;

        if (IS_REVERSE) // 反向
        {
            WS2812FX_setPixelColor_rgbw(_seg->stop, r1, g1, b1, w1);
        } else {
            WS2812FX_setPixelColor_rgbw(_seg->start, r1, g1, b1, w1);
        }

        i++;
        i %= max_rate;
        if (i == 0) {
            _seg_rt->aux_param++;
            _seg_rt->aux_param %= _seg->c_n;
        }
    }
    _seg_rt->counter_mode_step++;
    _seg_rt->counter_mode_step %= _seg_len;

    return (_seg->speed);
}

u16 led_strip_rgb_anim_background_meteor(void)
{
    const u8 max_rate = 12;
    static uint8_t i = 0;
    uint32_t r1, g1, b1, w1;

    const uint8_t rate[max_rate] = {100, 70, 45, 30, 20, 15, 10, 7, 5, 4, 3, 2};
    // const uint8_t rate[meteor_len] = {100,60,45,30,20,15,10,7,5,4};  //亮度值 0-100
    int w = (_seg->colors[0] >> 24) & 0xff;
    int r = (_seg->colors[0] >> 16) & 0xff;
    int g = (_seg->colors[0] >> 8) & 0xff;
    int b = _seg->colors[0] & 0xff;
    if (_seg_rt->aux_param == 0) {
        i = 0;
        _seg_rt->aux_param = 1;
        Adafruit_NeoPixel_fill(_seg->colors[1], _seg->start, _seg_len);
    }

    WS2812FX_copyPixels(_seg->start + 1, _seg->start, _seg_len - 1);

    r1 = r * rate[i] / 100;
    g1 = g * rate[i] / 100;
    b1 = b * rate[i] / 100;
    w1 = w * rate[i] / 100;

    if (i >= max_rate) {
        r1 = _seg->colors[1] >> 16;
        g1 = _seg->colors[1] >> 8;
        b1 = _seg->colors[1] >> 0;
    }
    WS2812FX_setPixelColor_rgbw(_seg->start, r1, g1, b1, w1);
    if (i < max_rate)
        i++;

    _seg_rt->counter_mode_step++;
    _seg_rt->counter_mode_step %= _seg_len - 1 + max_rate * 2;

    if (_seg_rt->counter_mode_step == 0) {
        i = 0;
    }

    return (_seg->speed);
}

uint32_t __fade_in__(uint32_t c, uint8_t *f)
{
    static float i = 1;
    uint8_t r, g, b;
    r = c >> 16;
    g = c >> 8;
    b = c >> 0;

    r = r * (i) / 255;
    g = g * (i) / 255;
    b = b * (i) / 255;
    if (i < 128)
        i = i * 2;
    else
        i = i * 1.2;
    *f = 0;
    if (i >= 255) {
        i = 1;
        *f = 1;
        return c;
    }

    c = r << 16 | g << 8 | b;
    return c;
}

/* 开幕式 ,支持多种颜色
1、慢慢渐亮到目标颜色
2、拉开帷幕
3、完成开幕后是全黑的
4、开始新的开幕，重复从“1”开始
*/
u16 led_strip_rgb_anim_open(void)
{
    uint8_t f;
    if (_seg_rt->counter_mode_step == 0) {
        Adafruit_NeoPixel_fill(
            __fade_in__(_seg->colors[_seg_rt->aux_param], &f), _seg->start,
            _seg_len);
        if (f == 0)
            return (_seg->speed);
    }

    WS2812FX_setPixelColor(_seg_len / 2 - _seg_rt->counter_mode_step, BLACK);
    WS2812FX_setPixelColor(_seg_len / 2 + _seg_rt->counter_mode_step, BLACK);

    _seg_rt->counter_mode_step++;
    _seg_rt->counter_mode_step %= _seg_len / 2;
    if (_seg_rt->counter_mode_step == 0) {
        _seg_rt->aux_param++;
        _seg_rt->aux_param %= _seg->c_n;
    }
    return (_seg->speed);
}

/* 闭幕式 ,支持多种颜色*/
u16 led_strip_rgb_anim_close(void)
{
    if (_seg_rt->counter_mode_step == 0) {
        _seg_rt->aux_param3++;
        if (_seg_rt->aux_param3 < 9) {

            WS2812FX_fade_out_targetColor(BLACK);
            return (_seg->speed);
        }
        _seg_rt->aux_param3 = 0;
        _seg_rt->aux_param++;
        _seg_rt->aux_param %= _seg->c_n;
    }
    WS2812FX_setPixelColor(_seg->start + _seg_rt->counter_mode_step,
                           _seg->colors[_seg_rt->aux_param]);
    WS2812FX_setPixelColor(_seg_len - _seg_rt->counter_mode_step - 1,
                           _seg->colors[_seg_rt->aux_param]);

    _seg_rt->counter_mode_step++;
    _seg_rt->counter_mode_step %= _seg_len / 2;

    return (_seg->speed);
}

/* 多个点跑马 ，点和点直接固定间隔5，支持每个点不同颜色，支持设置背景色*/
/* 多个点跑马 ，点和点直接固定间隔 1，支持每个点不同颜色，支持设置背景色*/
// _seg->colors[0] 马仔的颜色
// _seg->colors[1] 最后一个颜色为背景色
u16 led_strip_rgb_anim_multi_dot_running(void)
{
    uint32_t color;
    uint16_t i;
    /* 可配置间隔：GAP 表示点与点之间的背景像素数量（默认 1，即间隔一个点） */
    const uint8_t GAP = 1; /* 修改此值以改变间隔 */

    /* 前景色数量，最后一个颜色为背景色 */
    uint8_t fg_colors = (_seg->c_n > 0) ? (_seg->c_n - 1) : 0;

    for (i = 0; i < _seg_len; i++) // 遍历整个灯串
    {
        uint16_t pos;
        if (IS_REVERSE)
            pos = _seg->start + i;
        else
            pos = _seg->stop - i;

        /* 基于 (i + step) 的模式决定当前像素是点还是背景。
		   当 (i + step) % (GAP+1) == 0 时为点，否则为背景。
		   这样保证点之间至少有 GAP 个背景像素，避免点连在一起。 */
        if (((i + _seg_rt->counter_mode_step) % (GAP + 1)) == 0) {
            /* 计算这是第几个点，用来循环前景色 */
            uint32_t dot_index = (i + _seg_rt->counter_mode_step) / (GAP + 1);
            uint8_t color_idx = 0;
            if (fg_colors)
                color_idx = dot_index % fg_colors;
            color = _seg->colors[color_idx];
        } else {
            /* 背景色使用 colors 的最后一个颜色 */
            color = _seg->colors[(_seg->c_n > 0) ? (_seg->c_n - 1) : 0];
        }

        WS2812FX_setPixelColor(pos, color);
    }

    /* 推进步进以实现移动效果，保持与其他效果一致的计数器范围 */
    _seg_rt->counter_mode_step = (_seg_rt->counter_mode_step + 1) % _seg_len;

    return (_seg->speed);
}

u16 led_strip_rgb_anim_meteor_jump(void)
{
    const u8 max_rate = 12;
    static uint8_t i = 0;
    if (_seg_rt->counter_mode_step == 0) {
        i = 0;
        SET_CYCLE;
        // fc_effect.mode_cycle = 1;
    }

    //-------------------- 周期 ---------------------------------------------
    if ((get_effect_p() == 1) &&
        (fc_effect.mode_cycle == 1)) // 计时中 && 完成一个循环
    {
        return (_seg->speed);
    }
    // uint8_t size = (SIZE_OPTION << 1) + 1;

    uint32_t r1 = 0, g1 = 0, b1 = 0, w1 = 0; // 必须初始化
    int w = 0, r = 0, g = 0, c = 0, b = 0;   // 必须初始化

    // const uint8_t rate[MAX_RATE] = {100,50,40,30,20,10};  //亮度值 0-100
    const uint8_t rate[max_rate] = {100, 60, 45, 30, 20,
                                    15,  10, 7,  5,  4}; // 亮度值 0-100

    uint8_t n;

    //---------------------- 效果过程 --------------------------------------
    w = (_seg->colors[_seg_rt->aux_param] >> 24) & 0xff;
    r = (_seg->colors[_seg_rt->aux_param] >> 16) & 0xff;
    g = (_seg->colors[_seg_rt->aux_param] >> 8) & 0xff;
    b = _seg->colors[_seg_rt->aux_param] & 0xff;

    WS2812FX_copyPixels(_seg->start + 1, _seg->start, _seg_len - 1); // 颜色平移

    // 调节亮度
    if (i < max_rate) // 避免数组溢出
    {
        r1 = r * rate[i] / 100;
        g1 = g * rate[i] / 100;
        b1 = b * rate[i] / 100;
        w1 = w * rate[i] / 100;
        i++;
    }

    WS2812FX_setPixelColor_rgbw(_seg->start, r1, g1, b1, w1); // 颜色值
    // 整段变色
    //  n = _seg_rt->counter_mode_step;
    //  if((n % (fc_effect.led_num / _seg->c_n) )== 0)  //6是流星的长度，根据流星长度而变
    n = _seg_rt->counter_mode_step % (3);
    // n =  _seg_rt->counter_mode_step % (fc_effect.led_num / _seg->c_n);
    if (n == 0) {
        _seg_rt->aux_param++;
        _seg_rt->aux_param %= _seg->c_n; // 颜色数量

        for (uint8_t m = 0, i = 0; m < max_rate; m++, i++) {
            w = (_seg->colors[_seg_rt->aux_param] >> 24) & 0xff;
            r = (_seg->colors[_seg_rt->aux_param] >> 16) & 0xff;
            g = (_seg->colors[_seg_rt->aux_param] >> 8) & 0xff;
            b = _seg->colors[_seg_rt->aux_param] & 0xff;

            r1 = r * rate[i] / 100;
            g1 = g * rate[i] / 100;
            b1 = b * rate[i] / 100;
            w1 = w * rate[i] / 100;
            // if(_seg_rt->counter_mode_step < 15)
            WS2812FX_setPixelColor_rgbw(_seg_rt->counter_mode_step - m, r1, g1,
                                        b1, w1); // 颜色值
        }
    }

    //----------------- 结束条件 ------------------------------------------

    _seg_rt->counter_mode_step++;
    _seg_rt->counter_mode_step %= _seg_len + max_rate;
    if (_seg_rt->counter_mode_step == 0) {
        fc_effect.mode_cycle = 1;
    }

    return (_seg->speed);
}

/**
 * @brief 颜色渐变发生器
 * 		每次调用返回一个介于当前颜色对之间的过渡色，
 * 		周期性地从颜色池中轮转取色，实现多种颜色之间的平滑渐变
 *
 * @param add_step 每次递进的步长
 *
 */
u32 __get_next_gradual_color__(uint8_t add_step)
{
    static int gadual_step = 0;
    static uint8_t index;
    static uint32_t c0, c1;
    int lum = gadual_step;
    uint32_t color;
    if (lum > 255)
        lum = 511 - lum; // lum = 0 -> 255 -> 0
    if (_seg_rt->aux_param == 0) {
        _seg_rt->aux_param = 1;
        index = 0;
        c1 = _seg->colors[index];
        index++;
        c0 = _seg->colors[index];
    }
    // c1目标颜色
    color = WS2812FX_color_blend(c1, c0, lum);
    if (gadual_step == 256) {

        index++;
        index %= _seg->c_n; // 颜色池的颜色数量
        c1 = _seg->colors[index];
    }
    gadual_step += add_step; // 修改渐变速度
    if (gadual_step > 511) {

        gadual_step = 0;
        index++;
        index %= _seg->c_n; // 颜色池的颜色数量
        c0 = _seg->colors[index];
    }
    return color;
}

u16 led_strip_rgb_anim_meteor_gradual(void)
{
    const u8 max_rate = 12;
    static uint8_t i = 0;
    if (_seg_rt->counter_mode_step == 0) {
        i = 0;
        SET_CYCLE;
        // fc_effect.mode_cycle = 1;
        // gadual_step = 0; //新的周期
    }

    if ((get_effect_p() == 1) &&
        (fc_effect.mode_cycle == 1)) // 计时中 && 完成一个循环
    {
        return (_seg->speed);
    }

    uint32_t r1 = 0, g1 = 0, b1 = 0, w1 = 0; // 必须初始化
    int w = 0, r = 0, g = 0, c = 0, b = 0;   // 必须初始化
    // const uint8_t rate[MAX_RATE] = {100,50,40,30,20,10};  //亮度值 0-100
    const uint8_t rate[max_rate] = {100, 60, 45, 30, 20,
                                    15,  10, 7,  5,  4}; // 亮度值 0-100

    uint32_t color;
    // 获取颜色
    color = __get_next_gradual_color__(20);
    w = (color >> 24) & 0xff;
    r = (color >> 16) & 0xff;
    g = (color >> 8) & 0xff;
    b = color & 0xff;

    WS2812FX_copyPixels(_seg->start + 1, _seg->start,
                        _seg_len - 1); // 颜色平移  控制流星移动

    // 调节亮度
    if (i < max_rate) // 避免数组溢出
    {
        r1 = r * rate[i] / 100;
        g1 = g * rate[i] / 100;
        b1 = b * rate[i] / 100;
        w1 = w * rate[i] / 100;
        i++;
    }

    WS2812FX_setPixelColor_rgbw(_seg->start, r1, g1, b1, w1); // 颜色值
    if (_seg_rt->counter_mode_step > 5)
    //  if(_seg_rt->counter_mode_step > (fc_effect.led_num / _seg->c_n))
    {
        for (int n = 0, i = 0; n < max_rate; n++, i++) // i和n都是流星的长度
        {
            // color = __get_next_gradual_color__(5);
            w = (color >> 24) & 0xff;
            r = (color >> 16) & 0xff;
            g = (color >> 8) & 0xff;
            b = color & 0xff;

            r1 = r * rate[i] / 100;
            g1 = g * rate[i] / 100;
            b1 = b * rate[i] / 100;
            w1 = w * rate[i] / 100;
            WS2812FX_setPixelColor_rgbw(_seg_rt->counter_mode_step - n, r1, g1,
                                        b1, w1); // 颜色值
        }
    }

    // 控制灯的流动长度
    _seg_rt->counter_mode_step++;
    _seg_rt->counter_mode_step %= _seg_len + max_rate;

    if (_seg_rt->counter_mode_step == 0)
        fc_effect.mode_cycle = 1;

    return (_seg->speed);
}

/*
 * Cycles a rainbow over the entire string of LEDs.
 彩虹渐变颜色流水效果
 60 可修改彩虹颜色长度
 */
u16 led_strip_rgb_anim_rainbow_flow(void)
{
    for (uint16_t i = 0; i < 60; i++) {
        uint32_t color = WS2812FX_color_wheel(
            ((i * 256 / 60) + _seg_rt->counter_mode_step) & 0xFF);
        WS2812FX_setPixelColor(_seg->stop - i, color);
    }

    _seg_rt->counter_mode_step = (_seg_rt->counter_mode_step + 1) & 0xFF;

    if (_seg_rt->counter_mode_step == 0)
        SET_CYCLE;

    return (_seg->speed / 256);
}

// 调整RGB线序时，对应的动画效果
u16 led_strip_rgb_anim_when_adjust_sequence(void)
{
    u32 color = BLUE;
    switch (_seg_rt->counter_mode_step) {
    case 0:
        color = RED;
        break;
    case 1:
        color = GREEN;
        break;
    case 2:
        color = BLUE;
        break;
    }

    Adafruit_NeoPixel_fill(color, _seg->start, _seg_len);

    if (_seg_rt->counter_mode_step < 3) {
        _seg_rt->counter_mode_step++;
    }
    return 1000;
}

// 星空效果，由声控信号触发，更多星星，闪烁速度更快
u16 led_strip_rgb_anim_sound_control_star_random(void)
{
    static u16 sp_en;
    static u16 en_cnt;

    uint8_t j = 2;
    _seg->colors[0] = WS2812FX_color_wheel(WS2812FX_random8());

    uint8_t w = (_seg->colors[0] >> 24) & 0xFF;
    uint8_t r = (_seg->colors[0] >> 16) & 0xFF;
    uint8_t g = (_seg->colors[0] >> 8) & 0xFF;
    uint8_t b = (_seg->colors[0] & 0xFF);
    uint8_t lum = max(w, max(r, max(g, b))) / 2;

    if (__LED_STRIP_RGB_GET_SOUND_TRIGGER__()) {
        sp_en = get_max_speed();
        _seg_rt->aux_param++;
        if (_seg_rt->aux_param > 3) // 加速持续时间
        {
            _seg_rt->aux_param = 0;
        }
    } else {
        sp_en = 500;
    }

    en_cnt += 10;
    if (en_cnt >= sp_en) {
        en_cnt = 0;
        WS2812FX_fade_out();
        for (uint16_t i = 0; i <= j; i++) {
            int flicker = WS2812FX_random8_lim(lum);
            WS2812FX_setPixelColor_rgbw(
                WS2812FX_random16_lim(_seg_len), max(r - flicker, 0),
                max(g - flicker, 0), max(b - flicker, 0), max(w - flicker, 0));
        }
    }

    SET_CYCLE;
    return (10);
}

// 星空效果，由声控信号触发，更多星星，闪烁速度更快
u16 led_strip_rgb_anim_sound_control_star_white(void)
{
    static u16 sp_en;
    static u16 en_cnt;

    uint8_t j = 2;
    _seg->colors[0] = WHITE;

    uint8_t w = (_seg->colors[0] >> 24) & 0xFF;
    uint8_t r = (_seg->colors[0] >> 16) & 0xFF;
    uint8_t g = (_seg->colors[0] >> 8) & 0xFF;
    uint8_t b = (_seg->colors[0] & 0xFF);
    uint8_t lum = max(w, max(r, max(g, b))) / 2;

    if (__LED_STRIP_RGB_GET_SOUND_TRIGGER__()) {
        sp_en = get_max_speed();
        _seg_rt->aux_param++;
        if (_seg_rt->aux_param > 3) // 加速持续时间
        {
            _seg_rt->aux_param = 0;
        }
    } else {
        sp_en = 500;
    }

    en_cnt += 10;
    if (en_cnt >= sp_en) {
        en_cnt = 0;
        WS2812FX_fade_out();
        for (uint16_t i = 0; i <= j; i++) {
            int flicker = WS2812FX_random8_lim(lum);
            WS2812FX_setPixelColor_rgbw(
                WS2812FX_random16_lim(_seg_len), max(r - flicker, 0),
                max(g - flicker, 0), max(b - flicker, 0), max(w - flicker, 0));
        }
    }

    SET_CYCLE;
    return (10);
}

// 星空效果，由声控信号触发，更多星星，闪烁速度更快
u16 led_strip_rgb_anim_sound_control_star_blue(void)
{
    static u16 sp_en;
    static u16 en_cnt;

    uint8_t j = 2;
    _seg->colors[0] = BLUE;

    uint8_t w = (_seg->colors[0] >> 24) & 0xFF;
    uint8_t r = (_seg->colors[0] >> 16) & 0xFF;
    uint8_t g = (_seg->colors[0] >> 8) & 0xFF;
    uint8_t b = (_seg->colors[0] & 0xFF);
    uint8_t lum = max(w, max(r, max(g, b))) / 2;

    if (__LED_STRIP_RGB_GET_SOUND_TRIGGER__()) {
        sp_en = get_max_speed();
        _seg_rt->aux_param++;
        if (_seg_rt->aux_param > 3) // 加速持续时间
        {
            _seg_rt->aux_param = 0;
        }
    } else {
        sp_en = 500;
    }

    en_cnt += 10;
    if (en_cnt >= sp_en) {
        en_cnt = 0;
        WS2812FX_fade_out();
        for (uint16_t i = 0; i <= j; i++) {
            int flicker = WS2812FX_random8_lim(lum);
            WS2812FX_setPixelColor_rgbw(
                WS2812FX_random16_lim(_seg_len), max(r - flicker, 0),
                max(g - flicker, 0), max(b - flicker, 0), max(w - flicker, 0));
        }
    }

    SET_CYCLE;
    return (10);
}

uint16_t led_strip_rgb_anim_sound_control_open_and_close_both_sides(void)
{
    // 顺序上升是：从0 ~ 一半
    // 倒序上升是：最长 ~ 一半
    uint8_t i;

    if (music_open_close_state.target_len > _seg_len / 2) {
        music_open_close_state.target_len = _seg_len / 2;
    }

    /* 设置初值 */
    if (_seg_rt->aux_param == 0) {
        music_open_close_state.forward_pos = 0; //顺序的位置，从0开始
        music_open_close_state.reverse_pos =
            _seg_len - 1; //倒叙的位置，从最后一点开始
        _seg_rt->aux_param = 1;
    }

    for (i = 0; i < music_open_close_state.forward_pos; i++) {
        uint32_t color = WS2812FX_color_wheel(
            ((i * 256 / _seg_len) + _seg_rt->counter_mode_step) & 0xFF);
        WS2812FX_setPixelColor(i, color);
        WS2812FX_setPixelColor(_seg_len - 1 - i, color);
    }

    _seg_rt->counter_mode_step = (_seg_rt->counter_mode_step + 1) & 0xFF;
    if (_seg_rt->counter_mode_step == 0) {
        SET_CYCLE;
    }

    /* 上升处理 */
    if (music_open_close_state.phase == E_RISE) {
        if (music_open_close_state.forward_pos <
            music_open_close_state.target_len - 1) //顺序上升位置，堆积上升
        {
            music_open_close_state.forward_pos++;
        } else {
            music_open_close_state.phase = E_TOP;  //标记来到顶端
            music_open_close_state.target_len = 0; //清除触发长度，为下降准备
        }

        if (music_open_close_state.reverse_pos >
                _seg_len - music_open_close_state.target_len &&
            music_open_close_state.reverse_pos > 0) //倒序上升位置，
        {
            music_open_close_state.reverse_pos--;
        }
    } else {
        /* 下降处理 */
        WS2812FX_setPixelColor(music_open_close_state.forward_pos, 0);
        WS2812FX_setPixelColor(music_open_close_state.reverse_pos, 0);

        if (music_open_close_state.forward_pos > 0) //计算顺序下降位置
        {
            music_open_close_state.forward_pos--;
        }
        if (music_open_close_state.reverse_pos < _seg_len - 1) {
            music_open_close_state.reverse_pos++;
        }
    }

    if (music_open_close_state.forward_pos <
        music_open_close_state.target_len) {
        music_open_close_state.phase = E_RISE;
    }

    return (get_max_speed());
}

uint16_t led_strip_rgb_anim_sound_control_open_and_close_slide_to_center(void)
{
    uint8_t i, j;
    if (music_open_close_state.target_len > _seg_len / 2) {
        music_open_close_state.target_len = _seg_len / 2;
    }

    if (_seg_rt->aux_param == 0) {
        music_open_close_state.forward_pos = _seg_len / 2;
        music_open_close_state.reverse_pos = _seg_len / 2;
        _seg_rt->aux_param = 1;
    }

    for (i = _seg_len / 2; i < music_open_close_state.forward_pos; i++) {
        uint32_t color = WS2812FX_color_wheel(
            ((i * 256 / _seg_len) + _seg_rt->counter_mode_step) & 0xFF);
        WS2812FX_setPixelColor(i, color);
        WS2812FX_setPixelColor(_seg_len / 2 - (i - _seg_len / 2) - 1, color);
    }
    _seg_rt->counter_mode_step = (_seg_rt->counter_mode_step + 1) & 0xFF;
    if (_seg_rt->counter_mode_step == 0)
        SET_CYCLE;

    if (music_open_close_state.phase == E_RISE) {
        if (music_open_close_state.forward_pos <
            _seg_len / 2 + music_open_close_state.target_len) {
            music_open_close_state.forward_pos++;
        } else {
            music_open_close_state.phase = E_TOP;
            music_open_close_state.target_len = 0;
        }

        if (music_open_close_state.reverse_pos >
                _seg_len / 2 - music_open_close_state.target_len &&
            music_open_close_state.reverse_pos > 0) {
            music_open_close_state.reverse_pos--;
        }
    } else {
        WS2812FX_setPixelColor(music_open_close_state.forward_pos, 0);
        WS2812FX_setPixelColor(music_open_close_state.reverse_pos, 0);

        if (music_open_close_state.forward_pos > _seg_len / 2) {
            music_open_close_state.forward_pos--;
        }
        if (music_open_close_state.reverse_pos < _seg_len / 2) {
            music_open_close_state.reverse_pos++;
        }
    }

    if (music_open_close_state.forward_pos <
        _seg_len / 2 + music_open_close_state.target_len) {
        music_open_close_state.phase = E_RISE;
    }

    return (get_max_speed());
}

// 彩虹闪烁，有声音两彩虹，没声音灭
uint16_t led_strip_rgb_anim_sound_control_rainbow_flash(void)
{
    uint16_t i;
    // 有声控信号，添加超时时间，超时时间结束前都执行声控对应的动画
    static volatile u8 cnt = 0;
    uint32_t color;

    if (__LED_STRIP_RGB_GET_SOUND_TRIGGER__()) {
        // 有声控信号，接下来一段时间都执行声控对应的动画
        if (cnt < 10) {
            cnt++;
        }
    } else {
        // 没有声控信号，并且处理声控对应的操作已经超时
        if (0 == cnt) {
            Adafruit_NeoPixel_fill(GRAY, _seg->start, _seg_len);
        }
    }

    if (cnt) {
        cnt--;

        for (i = 0; i < _seg_len; i++) {
            color = WS2812FX_color_wheel(
                ((i * 256 / _seg_len) + _seg_rt->counter_mode_step) & 0xFF);
            WS2812FX_setPixelColor(i, color);
        }
    }

    _seg_rt->counter_mode_step = (_seg_rt->counter_mode_step + 2) & 0xFF;
    if (_seg_rt->counter_mode_step == 0) {
        SET_CYCLE;
    }
    return (get_max_speed());
}

void __led_strip_rgb_anim_sound_control_feq_rise_init__(void)
{
    m_fs.act = E_RISE;
    m_fs.rise_tag = 0; // 默认可能是40，需要结合实际情况测试
    m_fs.c_pos = 0;
    m_fs.fall_sp = 0;
    m_fs.top_pos = 0;
    m_fs.top_sp = 0;
    m_fs.bgc = GRAY;
}

void __led_strip_rgb_anim_sound_control_feq_rise_set__(u8 percent)
{
    m_fs.rise_tag = percent * _seg_len / 30;
}

void music_open_close_set_trigger_len(u8 percent)
{
    music_open_close_state.target_len = percent * _seg_len / 100;
}

// 频谱，生长效果，到达最高位置，顶点变成白色，慢慢下降
uint16_t led_strip_rgb_anim_sound_control_feq_rise(void)
{
    uint8_t i;
    uint32_t color_top, color;

    // if (0 == _seg_rt->counter_mode_call) {
    //     // 刚进入声控，初始化参数
    //     __led_strip_rgb_anim_sound_control_feq_rise_init__();
    // }

    if (m_fs.rise_tag > _seg_len) {
        m_fs.rise_tag = _seg_len;
    }

    for (i = 0; i < m_fs.c_pos; i++) // 上升点亮灯带
    {
        color = WS2812FX_color_wheel(
            ((i * 256 / _seg_len) + _seg_rt->counter_mode_step) & 0xFF);
        WS2812FX_setPixelColor(i, color);
        // if(m_fs.top_pos<i)    //点亮顶端白点
        // WS2812FX_setPixelColor( i+1, WHITE);
    }

    _seg_rt->counter_mode_step = (_seg_rt->counter_mode_step + 1) & 0xFF;
    if (_seg_rt->counter_mode_step == 0) {
        SET_CYCLE;
    }

    if (m_fs.act == E_RISE) {
        if (m_fs.c_pos < m_fs.rise_tag - 1) //-1是留一个点位置给顶端显示白点
        {
            m_fs.c_pos++;
        } else // 到达顶端
        {
            m_fs.act = E_TOP;
            if (m_fs.c_pos > m_fs.top_pos) // 刷新顶端值
            {
                m_fs.top_pos = m_fs.c_pos;
                color_top = color;
            }
            m_fs.rise_tag = 0;
        }
    } else {
        if (m_fs.fall_sp >= 3) {
            m_fs.fall_sp = 0;
            if (m_fs.c_pos > 0) {
                m_fs.c_pos--;
            }
            WS2812FX_setPixelColor(m_fs.c_pos, 0);
        }
        m_fs.fall_sp++;
    }

    if (m_fs.rise_tag >= m_fs.c_pos && m_fs.rise_tag > 0) {
        m_fs.act = E_RISE;
    } else {
        m_fs.act = E_TOP;
    }

    if (m_fs.top_pos >= m_fs.c_pos) {
        if (m_fs.top_sp >= 10) {
            WS2812FX_setPixelColor(m_fs.top_pos + 2, BLACK);
            WS2812FX_setPixelColor(m_fs.top_pos + 1, color_top);
            WS2812FX_setPixelColor(m_fs.top_pos, color_top);
            m_fs.top_sp = 0;
            m_fs.top_pos--;
        }
        m_fs.top_sp++;
    }

    return (get_max_speed());
}

uint16_t led_strip_rgb_anim_sound_control_feq_rise_bround_color(void)
{
    uint16_t i;
    if (m_fs.rise_tag > _seg_len)
        m_fs.rise_tag = _seg_len;
    Adafruit_NeoPixel_fill(m_fs.bgc, _seg->start, _seg_len);

    for (i = 0; i < m_fs.c_pos; i++) //上升点亮灯带
    {
        uint32_t color = WS2812FX_color_wheel(
            ((i * 256 / _seg_len) + _seg_rt->counter_mode_step) & 0xFF);
        WS2812FX_setPixelColor(i, color);
    }
    _seg_rt->counter_mode_step = (_seg_rt->counter_mode_step + 1) & 0xFF;
    if (_seg_rt->counter_mode_step == 0)
        SET_CYCLE;

    if (m_fs.act == E_RISE) {
        if (m_fs.c_pos < m_fs.rise_tag) // -1是留一个点位置给顶端显示白点
        {
            m_fs.c_pos++;

        } else //到达顶端
        {
            m_fs.act = E_TOP;
            if (m_fs.rise_tag > m_fs.top_pos) //刷新顶端值
            {
                m_fs.top_pos = m_fs.rise_tag - 1;
            }
            m_fs.rise_tag = 0;
        }
    } else {
        if (m_fs.fall_sp >= 3) {
            m_fs.fall_sp = 0;
            if (m_fs.c_pos > 0)
                m_fs.c_pos--;
            // WS2812FX_setPixelColor( m_fs.c_pos, m_fs.bgc);  //下降填充背景颜色
        }
        m_fs.fall_sp++;
    }

    if (m_fs.rise_tag > m_fs.c_pos && m_fs.rise_tag > 0) {
        m_fs.act = E_RISE;
    } else {
        m_fs.act = E_TOP;
    }

    return (get_max_speed());
}

uint16_t led_strip_rgb_anim_sound_control_feq_rise_green_and_blue(void)
{
    uint16_t i;
    static u8 ss;
    uint8_t size = 1 << SIZE_OPTION;
    uint8_t param;
    param = _seg_len;
    // if(_seg_len > 64)
    // {
    //   param = 64;
    // }

    // uint8_t sineIncr = max(1, (256 / param) * size);
    uint8_t sineIncr = max(1, (256 / param) * size);

    if (m_fs.rise_tag > _seg_len)
        m_fs.rise_tag = _seg_len;
    Adafruit_NeoPixel_fill(BLACK, _seg->start, _seg_len);

    for (i = 0; i < m_fs.c_pos; i++) //上升点亮灯带
    {
        int lum = (int)Adafruit_NeoPixel_sine8(
            ((i + _seg_rt->counter_mode_step) * sineIncr));
        uint32_t color = WS2812FX_color_blend(BLUE, GREEN, lum);
        // if(IS_REVERSE)
        {
            WS2812FX_setPixelColor(_seg->start + i, color);
        }
        // else
        // {
        //   WS2812FX_setPixelColor(_seg->stop - i,  color);
        // }
    }
    ss++;
    if (ss >= 3) {
        _seg_rt->counter_mode_step = (_seg_rt->counter_mode_step + 1) & 0xFF;
        if (_seg_rt->counter_mode_step == 0)
            SET_CYCLE;
        ss = 0;
    }

    if (m_fs.act == E_RISE) {
        if (m_fs.c_pos < m_fs.rise_tag) //-1是留一个点位置给顶端显示白点
        {
            m_fs.c_pos++;

        } else //到达顶端
        {
            m_fs.act = E_TOP;
            if (m_fs.rise_tag > m_fs.top_pos) //刷新顶端值
            {
                m_fs.top_pos = m_fs.rise_tag - 1;
            }
            m_fs.rise_tag = 0;
        }
        // voice_c%=voice_trg;

    } else {
        if (m_fs.fall_sp >= 3) {
            m_fs.fall_sp = 0;
            if (m_fs.c_pos > 0)
                m_fs.c_pos--;
            // WS2812FX_setPixelColor( m_fs.c_pos, m_fs.bgc);  //下降填充背景颜色
        }
        m_fs.fall_sp++;
    }

    if (m_fs.rise_tag > m_fs.c_pos && m_fs.rise_tag > 0) {
        m_fs.act = E_RISE;
    } else {
        m_fs.act = E_TOP;
    }

    return (get_max_speed());
}

// 彩虹色滚动，音乐触发加速
uint16_t led_strip_rgb_anim_sound_control_energy(void)
{
    static volatile uint16_t sp_en;
    static volatile uint16_t en_cnt;
    for (uint16_t i = 0; i < _seg_len; i++) {
        uint32_t color = WS2812FX_color_wheel(
            ((i * 256 / _seg_len) + _seg_rt->counter_mode_step) & 0xFF);
        WS2812FX_setPixelColor(_seg->stop - i, color);
    }

    if (__LED_STRIP_RGB_GET_SOUND_TRIGGER__()) {
        if (en_cnt < 10) {
            en_cnt++;
        }

        sp_en = get_max_speed();
        _seg_rt->counter_mode_step = (_seg_rt->counter_mode_step + 20) & 0xFF;
    } else {
        if (en_cnt > 0) {
            en_cnt--;
        }

        if (en_cnt == 0) {
            sp_en = 30;
            _seg_rt->counter_mode_step =
                (_seg_rt->counter_mode_step + 1) & 0xFF;
        }
    }

    return (sp_en);
}

// 多颜色流动效果：
// 颜色块持续向灯带尾部流动，同时不断有新的随机颜色从头部补进来；
// 声控触发时流动加快，并且立即换入新的颜色。
// 颜色块大小按灯带长度自适应（6 颗灯时为 2 颗灯一个颜色块）。
uint16_t led_strip_rgb_anim_sound_control_multi_color_flow(void)
{
#if 1
    /*
        每个颜色块占用的灯珠数量：
        6 颗灯时为 2 颗灯一个颜色块（共 3 个颜色块）。
        想让颜色块更小/更大，修改这里的计算即可。
    */
    uint8_t size = _seg_len / 3;
    uint8_t k;
    uint16_t pos;
    static volatile u8 is_triggered = 0;
    static volatile u16 sp_en = 0;
    static volatile u16 en_cnt = 0;
    static u8 wheel_index = 0; /* 当前颜色块在色环上的位置 */

    if (size < 1) {
        size = 1;
    } else if (size > 5) {
        size = 5; /* 长灯带沿用原来的 5 颗灯一个颜色块 */
    }

    /* 第一次进入本效果：用随机的颜色块铺满整条灯带 */
    if (0 == _seg_rt->counter_mode_step) {
        for (pos = 0; pos < _seg_len;) {
            /* 随机取色，且与上一个颜色块在色环上至少相差 42，颜色分明 */
            wheel_index = WS2812FX_get_random_wheel_index(wheel_index);
            uint32_t color = WS2812FX_color_wheel(wheel_index);

            for (k = 0; (k < size) && (pos < _seg_len); k++, pos++) {
                WS2812FX_setPixelColor(_seg->start + pos, color);
            }
        }

        _seg_rt->counter_mode_step = 1; /* 标记背景颜色已经构建完成 */
        _seg_rt->aux_param3 = 0;        /* 下一步立即换入新的颜色 */
        return (10);
    }

    if (__LED_STRIP_RGB_GET_SOUND_TRIGGER__() || is_triggered) {
        // 有声控信号，设置为最快速度
        sp_en = 10;
        is_triggered = 1;

        // 声控触发的下一步立即换入新的颜色，让声控的变化更明显
        _seg_rt->aux_param3 = 0;

        _seg_rt->aux_param++;
        if (_seg_rt->aux_param > 3) {
            //加速持续时间
            _seg_rt->aux_param = 0;
            is_triggered = 0;
        }
    } else {
        sp_en = 500;
    }

    en_cnt += 10;
    if (en_cnt >= sp_en) {
        en_cnt = 0;

        /*
            整段颜色向尾部平移一格（一次 memmove 完成），再往头部补入新的颜色。

            注意：这里必须用 copyPixels()，不能用 WS2812FX_move_forward()！
            因为 move_forward() 是一个像素一个像素往前拷贝的循环，
            每次拷贝的源就是上一次刚写过的像素，实际效果是
            “把头部灯珠的颜色涂满整条灯带”，颜色块会被整个抹掉
            （表现出来就是：只有第 1 颗灯变颜色，后面几颗灯都变成同一种颜色）。
        */
        WS2812FX_copyPixels(_seg->start + 1, _seg->start, _seg_len - 1);

        /* 每流动 size 步换一种随机颜色，形成颜色块 */
        if (0 == _seg_rt->aux_param3) {
            wheel_index = WS2812FX_get_random_wheel_index(wheel_index);
            _seg->colors[0] = WS2812FX_color_wheel(wheel_index);
            _seg_rt->aux_param3 = size;
        }
        _seg_rt->aux_param3--;

        /* 头部补入当前颜色块的颜色 */
        WS2812FX_setPixelColor(_seg->start, _seg->colors[0]);
    }

    return 10;

#else
    uint8_t size = 5;
    uint8_t j;
    // u8 reverse;
    static u16 en_cnt = 0;
    static u16 sp_en = 0;
    static u8 is_triggered = 0;

    if (is_triggered || __LED_STRIP_RGB_GET_SOUND_TRIGGER__()) {
        is_triggered = 1;
        sp_en = 10;
        _seg_rt->aux_param++;
        if (_seg_rt->aux_param > 3) {
            // 控制加速持续时间
            _seg_rt->aux_param = 0;
            is_triggered = 0;
        }
    } else {
        sp_en = 500;
    }

    en_cnt += 10;

    if (en_cnt >= sp_en) {
        en_cnt = 0;
        /* 构建背景颜色 */
        if (_seg_rt->counter_mode_step == 0) {
            while (_seg_rt->counter_mode_step <= _seg->stop) {
                _seg->colors[0] = WS2812FX_color_wheel(
                    WS2812FX_get_random_wheel_index(WS2812FX_random8()));
                for (j = 0;
                     (j < size) && (_seg_rt->counter_mode_step <= _seg->stop);
                     j++) {
                    WS2812FX_setPixelColor(_seg->start +
                                               _seg_rt->counter_mode_step,
                                           _seg->colors[0]);
                    _seg_rt->counter_mode_step++;
                }
            }
        } else {

            // if (reverse) //反向流水
            // {
            //     WS2812FX_move_reverse(_seg->start, _seg->stop);
            // } else {
            WS2812FX_move_forward(_seg->start, _seg->stop);
            // }
        }
    }

    return 10;
#endif
}

uint16_t led_strip_rgb_anim_sound_control_meteor(void)
{
    const u8 max_rate = 17;
    static uint8_t i = 0;
    uint32_t r1, g1, b1, w1;
    const uint8_t rate[max_rate] = {100, 75, 45, 30, 20, 15, 10, 7, 5,
                                    3,   2,  0,  0,  0,  0,  0,  0};
    int w = (_seg->colors[0] >> 24) & 0xff;
    int r = (_seg->colors[0] >> 16) & 0xff;
    int g = (_seg->colors[0] >> 8) & 0xff;
    int b = _seg->colors[0] & 0xff;
    if (__LED_STRIP_RGB_GET_SOUND_TRIGGER__()) {
        _seg->colors[0] = WS2812FX_color_wheel(_seg_rt->counter_mode_step);
        _seg_rt->counter_mode_step = _seg_rt->counter_mode_step + 33 & 0xff;
        if (i == max_rate - 1) {
            i = 0;
        }
    }

    WS2812FX_copyPixels(_seg->start + 1, _seg->start, _seg_len - 1);
    r1 = r * rate[i] / 100;
    g1 = g * rate[i] / 100;
    b1 = b * rate[i] / 100;
    w1 = w * rate[i] / 100;
    WS2812FX_setPixelColor_rgbw(_seg->start, r1, g1, b1, w1);

    if (i < max_rate - 1) {
        i++;
    }

    return (30);
}

// RGB 关闭时，对应的动画
u16 led_strip_rgb_anim_pwr_off(void)
{
    Adafruit_NeoPixel_fill(BLACK, _seg->start, _seg_len);
    return 100;
}

u16 led_strip_rgb_anim_single_superposition(void)
{
    int j;
    int w = 0, r = 0, g = 0, c = 0, b = 0; // 必须初始化
    static uint8_t ranks_n = 0;            // 记录已经堆积的高度
    static uint8_t full_screen = 0;
    w = (_seg->colors[_seg_rt->aux_param3] >> 24) & 0xff;
    r = (_seg->colors[_seg_rt->aux_param3] >> 16) & 0xff;
    g = (_seg->colors[_seg_rt->aux_param3] >> 8) & 0xff;
    b = _seg->colors[_seg_rt->aux_param3] & 0xff;

    if (IS_REVERSE) // 反向
    {
        for (j = 0; j < _seg_len; j++) // 行
        {
            if (j < _seg_rt->counter_mode_step)
                WS2812FX_setPixelColor_rgbw(_seg_len - j - 1, 0, 0, 0, 0);
            else if (j == _seg_rt->counter_mode_step)
                WS2812FX_setPixelColor_rgbw(_seg_len - j - 1, r, g, b, w);
        }
        _seg_rt->counter_mode_step++;
        if (_seg_rt->counter_mode_step >= (_seg_len - ranks_n)) {
            ranks_n++;
            if (ranks_n >= _seg_len) {
                ranks_n = 0;
                // full_screen=1;
                _seg_rt->aux_param3++;
                _seg_rt->aux_param3 %= _seg->c_n;
                Adafruit_NeoPixel_fill(BLACK, _seg->start,
                                       _seg_len); // 一次性全灭
            }
            _seg_rt->counter_mode_step = 0;
        }
    } else {
        // 实现跑动
        for (j = 0; j < _seg_len; j++) {

            if (j < _seg_rt->counter_mode_step)
                WS2812FX_setPixelColor_rgbw(j, 0, 0, 0, 0);
            else if (j == _seg_rt->counter_mode_step)
                WS2812FX_setPixelColor_rgbw(j, r, g, b, w);
        }
        _seg_rt->counter_mode_step++;
        if (_seg_rt->counter_mode_step >= (_seg_len - ranks_n)) {
            ranks_n++;
            if (ranks_n >= _seg_len) // 实现堆积
            {
                ranks_n = 0;
                _seg_rt->aux_param3++;
                _seg_rt->aux_param3 %= _seg->c_n;
                Adafruit_NeoPixel_fill(BLACK, _seg->start,
                                       _seg_len); // 一次性全灭
            }
            _seg_rt->counter_mode_step = 0;
        }
    }

    return (_seg->speed);
}

u16 led_strip_rgb_anim_single_superposition_with_background(void)
{
    int j;
    int w = 0, r = 0, g = 0, c = 0, b = 0; // 必须初始化
    int bg_w = 0, bg_r = 0, bg_g = 0, bg_b = 0;
    static uint8_t ranks_n = 0; // 记录已经堆积的高度
    static uint8_t full_screen = 0;
    w = (_seg->colors[_seg_rt->aux_param3] >> 24) & 0xff;
    r = (_seg->colors[_seg_rt->aux_param3] >> 16) & 0xff;
    g = (_seg->colors[_seg_rt->aux_param3] >> 8) & 0xff;
    b = _seg->colors[_seg_rt->aux_param3] & 0xff;

    bg_w = (_seg->colors[_seg->c_n - 1] >> 24) & 0xff;
    bg_r = (_seg->colors[_seg->c_n - 1] >> 16) & 0xff;
    bg_g = (_seg->colors[_seg->c_n - 1] >> 8) & 0xff;
    bg_b = _seg->colors[_seg->c_n - 1] & 0xff;

    if (IS_REVERSE) // 反向
    {

        for (j = 0; j < _seg_len; j++) // 行
        {

            if (j < _seg_rt->counter_mode_step)
                WS2812FX_setPixelColor_rgbw(_seg_len - j - 1, bg_r, bg_g, bg_b,
                                            bg_w);
            else if (j == _seg_rt->counter_mode_step)
                WS2812FX_setPixelColor_rgbw(_seg_len - j - 1, r, g, b, w);
        }
        _seg_rt->counter_mode_step++;
        if (_seg_rt->counter_mode_step >= (_seg_len - ranks_n)) {
            ranks_n++;
            if (ranks_n >= _seg_len) {
                ranks_n = 0;
                // full_screen=1;
                _seg_rt->aux_param3++;
                _seg_rt->aux_param3 %= _seg->c_n - 1;
                Adafruit_NeoPixel_fill(_seg->colors[_seg->c_n - 1], _seg->start,
                                       _seg_len); // 填色
            }
            _seg_rt->counter_mode_step = 0;
        }
    } else {
        // 实现跑动
        for (j = 0; j < _seg_len; j++) {

            if (j < _seg_rt->counter_mode_step)
                WS2812FX_setPixelColor_rgbw(j, bg_r, bg_g, bg_b,
                                            bg_w); // 跑动后的灯色
            else if (j == _seg_rt->counter_mode_step)
                WS2812FX_setPixelColor_rgbw(j, r, g, b, w);
        }
        _seg_rt->counter_mode_step++;
        if (_seg_rt->counter_mode_step >= (_seg_len - ranks_n)) {
            ranks_n++;
            if (ranks_n >= _seg_len) // 实现堆积
            {
                ranks_n = 0;
                // full_screen=1;
                _seg_rt->aux_param3++;
                _seg_rt->aux_param3 %= _seg->c_n - 1;
                Adafruit_NeoPixel_fill(_seg->colors[_seg->c_n - 1], _seg->start,
                                       _seg_len); // 填色
            }
            _seg_rt->counter_mode_step = 0;
        }
    }

    return (_seg->speed);
}

u16 led_strip_rgb_anim_multi_superposition(void)
{
    int j;
    int w = 0, r = 0, g = 0, c = 0, b = 0; // 必须初始化
    int bf_w = 0, bf_r = 0, bf_g = 0, bf_b = 0;
    static uint8_t ranks_n = 0; // 记录已经堆积的高度
    static uint8_t full_screen = 0;
    static uint8_t before_color = 0;
    w = (_seg->colors[_seg_rt->aux_param3] >> 24) & 0xff;
    r = (_seg->colors[_seg_rt->aux_param3] >> 16) & 0xff;
    g = (_seg->colors[_seg_rt->aux_param3] >> 8) & 0xff;
    b = _seg->colors[_seg_rt->aux_param3] & 0xff;

    if (_seg_rt->aux_param == 0) {
        bf_w = 0;
        bf_r = 0;
        bf_g = 0;
        bf_b = 0;
        before_color = 0;
    } else {
        if (_seg_rt->aux_param3 == 0) {
            before_color = _seg->c_n - 1;
        } else {
            before_color = _seg_rt->aux_param3 - 1;
        }

        bf_w = (_seg->colors[before_color] >> 24) & 0xff;
        bf_r = (_seg->colors[before_color] >> 16) & 0xff;
        bf_g = (_seg->colors[before_color] >> 8) & 0xff;
        bf_b = _seg->colors[before_color] & 0xff;
    }

    if (IS_REVERSE) // 反向
    {

        for (j = 0; j < _seg_len; j++) // 行
        {

            if (j < _seg_rt->counter_mode_step)
                WS2812FX_setPixelColor_rgbw(_seg_len - j - 1, bf_r, bf_g, bf_b,
                                            bf_w);
            else if (j == _seg_rt->counter_mode_step)
                WS2812FX_setPixelColor_rgbw(_seg_len - j - 1, r, g, b, w);
        }
        _seg_rt->counter_mode_step++;
        if (_seg_rt->counter_mode_step >= (_seg_len - ranks_n)) {

            // if(full_screen)
            // {
            //   ranks_n=0;
            //   full_screen=0;
            //   Adafruit_NeoPixel_fill(BLACK, _seg->start, _seg_len);  //逐个灭

            // }
            ranks_n++;
            if (ranks_n >= _seg_len) {
                ranks_n = 0;
                _seg_rt->aux_param = 1;
                Adafruit_NeoPixel_fill(_seg->colors[_seg_rt->aux_param3],
                                       _seg->start, _seg_len);

                // full_screen=1;
                _seg_rt->aux_param3++;
                _seg_rt->aux_param3 %= _seg->c_n;
            }
            _seg_rt->counter_mode_step = 0;
        }
    } else {
        // 实现跑动
        for (j = 0; j < _seg_len; j++) {

            if (j < _seg_rt->counter_mode_step)
                WS2812FX_setPixelColor_rgbw(j, bf_r, bf_g, bf_b, bf_w);
            else if (j == _seg_rt->counter_mode_step)
                WS2812FX_setPixelColor_rgbw(j, r, g, b, w);
        }
        _seg_rt->counter_mode_step++;
        if (_seg_rt->counter_mode_step >= (_seg_len - ranks_n)) {

            // if(full_screen)
            // {
            //   ranks_n=0;
            //   full_screen=0;
            //   Adafruit_NeoPixel_fill(BLACK, _seg->start, _seg_len);  //逐个灭

            // }
            ranks_n++;
            if (ranks_n >= _seg_len) // 实现堆积
            {
                ranks_n = 0;
                // full_screen=1;
                _seg_rt->aux_param = 1;
                Adafruit_NeoPixel_fill(_seg->colors[_seg_rt->aux_param3],
                                       _seg->start, _seg_len);

                _seg_rt->aux_param3++;
                _seg_rt->aux_param3 %= _seg->c_n;
            }
            _seg_rt->counter_mode_step = 0;
        }
    }

    return (_seg->speed);
}

u16 led_strip_rgb_anim_fireworks(void)
{
    uint32_t color = BLACK;
    do {
        // randomly choose a non-BLACK color from the colors array
        // 生成一个不是黑色的颜色
        color = _seg->colors[WS2812FX_random8_lim(MAX_NUM_COLORS)];
    } while (color == BLACK);

    WS2812FX_fade_out();

    // for better performance, manipulate the Adafruit_NeoPixels pixels[] array directly
    uint8_t *pixels = Adafruit_NeoPixel_getPixels();
    uint8_t bytesPerPixel =
        Adafruit_NeoPixel_getNumBytesPerPixel(); // 3=RGB, 4=RGBW
    uint16_t startPixel = _seg->start * bytesPerPixel + bytesPerPixel;
    uint16_t stopPixel = _seg->stop * bytesPerPixel;
    for (uint16_t i = startPixel; i < stopPixel; i++) {
        uint16_t tmpPixel = (pixels[i - bytesPerPixel] >> 2) + pixels[i] +
                            (pixels[i + bytesPerPixel] >> 2);
        pixels[i] = tmpPixel > 255 ? 255 : tmpPixel;
    }

    uint8_t size = 2 << SIZE_OPTION;
    if (!_triggered) {
        for (uint16_t i = 0; i < max(1, _seg_len / 20); i++) {
            if (WS2812FX_random8_lim(10) == 0) {
                uint16_t index =
                    _seg->start + WS2812FX_random16_lim(_seg_len - size + 1);
                Adafruit_NeoPixel_fill(color, index, size); //
                SET_CYCLE;
            }
        }
    } else {
        for (uint16_t i = 0; i < max(1, _seg_len / 10); i++) {
            uint16_t index =
                _seg->start + WS2812FX_random16_lim(_seg_len - size + 1);
            Adafruit_NeoPixel_fill(color, index, size); //
            SET_CYCLE;
        }
    }

    return (_seg->speed / _seg_len);
}
