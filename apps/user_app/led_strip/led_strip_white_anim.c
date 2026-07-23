#include "led_strip_white_anim.h"
#include "led_strip_white_schedule.h"
#include "WS2812FX.H"

/**
 * @brief 单色灯带渐变灭灯,做流星效果   兼容正反方向
 *          动画固定使用最大亮度值
 *
 * @return uint16_t
 */
uint16_t led_strip_white_anim_comet_1_whit_max_brightness(void)
{
	// 补丁：
	if (0 == _seg_rt->counter_mode_call)
	{
		// 刚进入，清除之前的数据残留
		Adafruit_NeoPixel_fill(BLACK, _seg->start, _seg_len);
	}

	WS2812FX_fade_out_with_max_brightness();
	u8 offset;
	offset = 13;
	if (IS_REVERSE)
	{
		if ((_seg->stop - _seg->start) >= _seg_rt->counter_mode_step)
		{
			WS2812FX_setPixelColor_with_max_brightness(_seg->stop - _seg_rt->counter_mode_step, _seg->colors[0]);
		}
	}
	else
	{
		if (_seg_rt->counter_mode_step < _seg->stop + 1)
		{
			WS2812FX_setPixelColor_with_max_brightness(_seg->start + _seg_rt->counter_mode_step, _seg->colors[0]);
		}
	}

	_seg_rt->counter_mode_step = (_seg_rt->counter_mode_step + 1) % (_seg_len + offset);
	if (_seg_rt->counter_mode_step == 0)
	{
		SET_CYCLE;
		return led_strip_white.period_cnt;
	}

	return (_seg->speed);
}

/**
 * @brief 堆积流水   兼容正反方向
 *          动画固定使用最大亮度值
 * @return uint16_t
 */
uint16_t led_strip_white_anim_comet_4_with_max_brightness(void)
{
	// 补丁：
	if (0 == _seg_rt->counter_mode_call)
	{
		// 刚进入，清除之前的数据残留
		Adafruit_NeoPixel_fill(BLACK, _seg->start, _seg_len);
	}

	if (IS_REVERSE)
	{

		if (_seg_rt->counter_mode_step < _seg_len)
		{
			WS2812FX_setPixelColor_with_max_brightness(_seg->stop - _seg_rt->counter_mode_step, WHITE);
		}
		else
		{
			WS2812FX_setPixelColor_with_max_brightness(2 * _seg_len - _seg_rt->counter_mode_step, BLACK);
		}
	}
	else
	{
		if (_seg_rt->counter_mode_step < _seg_len)
		{
			WS2812FX_setPixelColor_with_max_brightness(_seg->start + _seg_rt->counter_mode_step, WHITE);
		}
		else
		{
			WS2812FX_setPixelColor_with_max_brightness(_seg->start + _seg_rt->counter_mode_step - _seg_len, BLACK);
		}
	}

	_seg_rt->counter_mode_step++;
	_seg_rt->counter_mode_step %= _seg_len * 2;
	if (_seg_rt->counter_mode_step == 0)
	{
		SET_CYCLE;
		return led_strip_white.period_cnt; // 控制一轮动画之间的时间间隔
	}
	return (_seg->speed); // 返回计数器结果
}

u16 led_strip_white_anim_chase_with_max_brightness(void)
{
	// 动画步骤
	u16 meteor_animation_step = 0;
	// 返回值（每一动画步骤的时间间隔）
	u16 return_value = 0;

	Adafruit_NeoPixel_fill(BLACK, _seg->start, _seg_len); // 全段填黑色，灭灯

	meteor_animation_step = _seg_len + 6 + 8; // 动画长度：+第一段流星灯尾焰长度（6）+第二段流星灯+尾焰长度（6）
	/*
		外部传入的speed在30~330，这里将样机的3s动画时间分成330份，再根据传入的speed细分动画时间
	*/
	return_value = (u32)3000 * _seg->speed / 330 / meteor_animation_step;

	u8 brightness_levels_buff[12] = {0};
	// 根据尾焰长度，自动划分亮度等级：
	for (u8 i = 0; i < ARRAY_SIZE(brightness_levels_buff); i++)
	{
		brightness_levels_buff[i] = 255 - ((u32)i * 255 / 6);
	}

	if (IS_REVERSE) // 反向流星
	{
		// 要用带符号的数据类型，可能会计算出负数 (begin_index 和 cur_index 都需要是带符号的)
		int32_t begin_index = _seg->stop - _seg_rt->counter_mode_step; // 当前流星灯的头部
		int32_t cur_index = begin_index;							   // 当前要绘制的流星灯索引
		for (u8 i = 0; i < 6; i++)									   // 根据流星灯尾焰长度进行绘制
		{
			if (cur_index <= (int32_t)_seg->stop)
			{
				cur_index++;

				// 防止越界：
				if (cur_index >= (int32_t)_seg->start && cur_index <= (int32_t)_seg->stop)
				{
					u8 brightness = brightness_levels_buff[i];
					u32 color = WS2812FX_color_blend(BLACK, WHITE, brightness);
					WS2812FX_setPixelColor_with_max_brightness(cur_index, color);
				}
			}
		}

		if (_seg_rt->counter_mode_step > 7) // 如果动画步骤以及到了第 8 步，说明第一段流星头部+尾焰（6个灯）都已经绘制完毕
		{
			// 绘制第二段流星
			begin_index = _seg->stop - _seg_rt->counter_mode_step + 7;
			cur_index = begin_index;

			for (u8 i = 0; i < 6; i++) // 根据流星灯尾焰长度进行绘制
			{
				if (cur_index <= (int32_t)_seg->stop)
				{
					cur_index++;

					// 防止越界：
					if (cur_index >= (int32_t)_seg->start && cur_index <= (int32_t)_seg->stop)
					{
						u8 brightness = brightness_levels_buff[i];
						u32 color = WS2812FX_color_blend(BLACK, WHITE, brightness);
						WS2812FX_setPixelColor_with_max_brightness(cur_index, color);
					}
				}
			}
		}
	}
	else // 正向流星
	{
		u16 begin_index = _seg->start + _seg_rt->counter_mode_step; // 当前流星灯的头部
		u16 cur_index = begin_index;								// 当前要绘制的流星灯索引
		for (u8 i = 0; i < 6; i++)									// 根据流星灯尾焰长度进行绘制
		{
			if (cur_index > _seg->start)
			{
				cur_index--;

				// 防止越界：
				if (cur_index >= (int32_t)_seg->start && cur_index <= (int32_t)_seg->stop)
				{
					u8 brightness = brightness_levels_buff[i];
					u32 color = WS2812FX_color_blend(BLACK, WHITE, brightness);
					WS2812FX_setPixelColor_with_max_brightness(cur_index, color);
				}
			}
		}

		if (_seg_rt->counter_mode_step > 7) // 如果动画步骤以及到了第 8 步，说明第一段流星头部+尾焰（6个灯）都已经绘制完毕
		{
			// 绘制第二段流星
			begin_index = _seg->start + _seg_rt->counter_mode_step - 7;
			cur_index = begin_index;

			for (u8 i = 0; i < 6; i++)
			{
				if (cur_index > _seg->start)
				{
					cur_index--;

					// 防止越界：
					if (cur_index >= (int32_t)_seg->start && cur_index <= (int32_t)_seg->stop)
					{
						u8 brightness = brightness_levels_buff[i];
						u32 color = WS2812FX_color_blend(BLACK, WHITE, brightness);
						WS2812FX_setPixelColor_with_max_brightness(cur_index, color);
					}
				}
			}
		}
	}

	_seg_rt->counter_mode_step = (_seg_rt->counter_mode_step + 1) % (meteor_animation_step);
	if (_seg_rt->counter_mode_step == 0)
	{
		SET_CYCLE;
		return led_strip_white.period_cnt; // 返回周期值，控制一轮动画之间的时间间隔
	}

	return return_value;
}

/**
 * @brief 12个流星灯(假设坐标是0~11)，从中间做分隔，
 *          先在一半的范围内流星(0~5)，时间间隔结束后，再从另一半开始流星(6~11)
 *
 * @return u16
 */
u16 led_strip_white_anim_half_flow_with_max_brightness(void)
{
	// 补丁：
	if (0 == _seg_rt->counter_mode_call)
	{
		// 刚进入，清除之前的数据残留
		Adafruit_NeoPixel_fill(BLACK, _seg->start, _seg_len);
		_seg_rt->aux_param = 0; // 控制前半段流星和后半段流星
	}

	WS2812FX_fade_out_with_max_brightness();

	if (IS_REVERSE)
	{
		if (_seg_rt->aux_param == 0)
		{
			if (_seg_rt->counter_mode_step < _seg_len / 2)
			{
				WS2812FX_setPixelColor_with_max_brightness(_seg->stop - _seg_rt->counter_mode_step, _seg->colors[0]);
			}
		}
		else
		{
			if (_seg_rt->counter_mode_step < _seg_len / 2)
			{
				WS2812FX_setPixelColor_with_max_brightness(_seg->stop - _seg_rt->counter_mode_step - _seg_len / 2, _seg->colors[0]);
			}
		}
	}
	else
	{
		if (_seg_rt->aux_param == 0)
		{
			if (_seg_rt->counter_mode_step < _seg_len / 2)
			{
				WS2812FX_setPixelColor_with_max_brightness(_seg->start + _seg_rt->counter_mode_step, _seg->colors[0]);
			}
		}
		else
		{
			if (_seg_rt->counter_mode_step < _seg_len / 2)
			{
				WS2812FX_setPixelColor_with_max_brightness(_seg_len / 2 + 1 + _seg_rt->counter_mode_step, _seg->colors[0]);
			}
		}
	}

	_seg_rt->counter_mode_step = (_seg_rt->counter_mode_step + 1) % (_seg_len * 2);
	if (_seg_rt->counter_mode_step == 0)
	{
		SET_CYCLE;
		_seg_rt->aux_param = !_seg_rt->aux_param;
		return led_strip_white.period_cnt;
	}

	return (_seg->speed);
}

// 流星灯单点流水，最后4个灯堆积
// 只适用于七彩灯+流星灯，并且流星灯数量为12，不方便移植
u16 led_strip_white_anim_single_flow_and_stack_with_max_brightness(void)
{
	const u8 stack_num = 4; // 堆积流水灯数量（不能大于灯的长度，否则后续的计算会错误）

	Adafruit_NeoPixel_fill(BLACK, _seg->start, _seg_len);

	if (IS_REVERSE)
	{
		if (_seg_rt->counter_mode_step < _seg_len - stack_num)
		{
			// 如果动画还没有跑到要堆积的位置
			WS2812FX_setPixelColor_with_max_brightness(_seg->stop - _seg_rt->counter_mode_step, WHITE);
		}
		else
		{
			// 如果动画跑到了要堆积的位置，动画相当于一边跑一边增加了尾部的流水灯
			for (u8 i = 0; i < stack_num; i++)
			{
				int32_t index = _seg->stop - _seg_rt->counter_mode_step; // 记录当前要绘制的灯珠头部的索引（头部为靠近_seg->start的一侧）
				if (index >= _seg->start &&
					index + i < _seg->start + stack_num)
				{
					WS2812FX_setPixelColor_with_max_brightness(index + i, WHITE);
				}
			}
		}
	}
	else
	{
		if (_seg_rt->counter_mode_step < _seg_len - stack_num)
		{
			// 如果动画还没有跑到要堆积的位置
			WS2812FX_setPixelColor_with_max_brightness(_seg->start + _seg_rt->counter_mode_step, WHITE);
		}
		else
		{
			// 如果动画跑到了要堆积的位置，动画相当于一边跑一边增加了尾部的流水灯
			for (u8 i = 0; i < stack_num; i++)
			{
				u32 index = (_seg->start + _seg_rt->counter_mode_step); // 记录当前要绘制的灯珠头部的索引（头部为靠近_seg->stop的一侧）
				// if (index - i > _seg->start + _seg->stop - stack_num &&
				if (index - i > _seg->stop - stack_num &&
					index <= _seg->stop) // 堆积的索引值范围
				{
					WS2812FX_setPixelColor_with_max_brightness(index - i, WHITE);
				}
			}
		}
	}

	_seg_rt->counter_mode_step++;
	_seg_rt->counter_mode_step %= _seg_len + 1; // 动画长度，包括点亮最后一个灯，再结束

	if (_seg_rt->counter_mode_step == 0)
	{
		SET_CYCLE;
		return led_strip_white.period_cnt; // 控制一轮动画之间的时间间隔
	}

	return (_seg->speed); // 返回计数器结果
}

// 流星灯堆积流水
u16 led_strip_white_anim_stack_flow_with_max_brightness(void)
{
	// 补丁：
	if (0 == _seg_rt->counter_mode_call)
	{
		// 刚进入，清除之前的数据残留
		Adafruit_NeoPixel_fill(BLACK, _seg->start, _seg_len);
	}

	if (IS_REVERSE)
	{
		if (_seg_rt->counter_mode_step < _seg_len)
		{
			WS2812FX_setPixelColor_with_max_brightness(_seg->stop - _seg_rt->counter_mode_step, WHITE);
		}
	}
	else
	{
		if (_seg_rt->counter_mode_step < _seg_len)
		{
			WS2812FX_setPixelColor_with_max_brightness(_seg->start + _seg_rt->counter_mode_step, WHITE);
		}
	}

	_seg_rt->counter_mode_step++;
	_seg_rt->counter_mode_step %= _seg_len + 1; // 动画长度，包括堆积流水到点亮最后一个灯，再结束
	if (_seg_rt->counter_mode_step == 0)
	{
		SET_CYCLE;
		Adafruit_NeoPixel_fill(BLACK, _seg->start, _seg_len); // 全段填黑色，灭灯

		// 客户给的样机中，堆积流水没有时间间隔
		// return led_strip_white.period_cnt; // 控制一轮动画之间的时间间隔
	}

	return (_seg->speed); // 返回计数器结果
}

// 流星灯堆积流水（正向）+流星灯堆积流水（反向）
u16 led_strip_white_anim_stack_flow_plus_reverse_with_max_brightness(void)
{
	// _seg_rt->aux_param // 控制堆积流水的方向

	// 补丁：
	if (0 == _seg_rt->counter_mode_call)
	{
		// 刚进入，清除之前数据残留
		Adafruit_NeoPixel_fill(BLACK, _seg->start, _seg_len);
		_seg_rt->aux_param = 0; // 控制堆积流水的方向
	}

	if (_seg_rt->aux_param == 0) // 正向堆积流水
	{
		if (_seg_rt->counter_mode_step < _seg_len)
		{
			WS2812FX_setPixelColor_with_max_brightness(_seg->start + _seg_rt->counter_mode_step, WHITE);
		}
	}
	else
	{
		if (_seg_rt->counter_mode_step < _seg_len)
		{
			WS2812FX_setPixelColor_with_max_brightness(_seg->stop - _seg_rt->counter_mode_step, WHITE);
		}
	}

	_seg_rt->counter_mode_step++;
	_seg_rt->counter_mode_step %= _seg_len + 1; // 动画长度，包括堆积流水到点亮最后一个灯，再结束
	if (_seg_rt->counter_mode_step == 0)
	{
		SET_CYCLE;
		Adafruit_NeoPixel_fill(BLACK, _seg->start, _seg_len); // 全段填黑色，灭灯
		_seg_rt->aux_param = !_seg_rt->aux_param;

		// 客户给的样机中，堆积流水没有时间间隔
		// return led_strip_white.period_cnt; // 控制一轮动画之间的时间间隔
	}

	return (_seg->speed); // 返回计数器结果
}

/*
	流星发射，声音触发，不支持连续发射，等上个流星发射完成再发射第二个
	动画固定使用最大亮度值
*/
uint16_t led_strip_white_anim_sound_control_meteor_with_max_brightness(void)
{
	const u8 max_rate = 8;
	static uint8_t i = 0, trg;
	uint32_t r1, g1, b1, w1;
	const uint8_t rate[max_rate] = {100, 88, 75, 55, 30, 10, 0, 0};
	int w = (_seg->colors[0] >> 24) & 0xff;
	int r = (_seg->colors[0] >> 16) & 0xff;
	int g = (_seg->colors[0] >> 8) & 0xff;
	int b = _seg->colors[0] & 0xff;

	// 补丁：
	if (0 == _seg_rt->counter_mode_call)
	{
		// 刚进入，清除之前的数据残留
		Adafruit_NeoPixel_fill(BLACK, _seg->start, _seg_len);
	}

	if (__LED_STRIP_WHITE_GET_SOUND_TRIGGER__())
	{
		trg = 1;
	}

	if (trg)
	{
		WS2812FX_copyPixels(_seg->start + 1, _seg->start, _seg_len - 1);

		r1 = r * rate[i] / 100;
		g1 = g * rate[i] / 100;
		b1 = b * rate[i] / 100;
		w1 = w * rate[i] / 100;

		WS2812FX_setPixelColor_rgbw_with_max_brightness(_seg->start, r1, g1, b1, w1);
		if (i < max_rate - 1)
		{
			i++;
		}

		_seg_rt->counter_mode_step++;
		if (_seg_rt->counter_mode_step >= _seg_len + 8)
		{
			trg = 0;
			_seg_rt->counter_mode_step = 0;
			i = 0;
		}
	}

	return 30;
}

/*
	流星发射，声音触发，可以连续发射
	动画固定使用最大亮度值
*/
uint16_t led_strip_white_anim_sound_control_meteor3_with_max_brightness(void)
{
	const u8 max_rate = 8;
	static uint8_t i = 0, trg;
	uint32_t r1, g1, b1, w1;
	const uint8_t rate[max_rate] = {100, 75, 50, 25, 10, 0, 0, 0};
	int w = (_seg->colors[0] >> 24) & 0xff;
	int r = (_seg->colors[0] >> 16) & 0xff;
	int g = (_seg->colors[0] >> 8) & 0xff;
	int b = _seg->colors[0] & 0xff;

	// 补丁：
	if (0 == _seg_rt->counter_mode_call)
	{
		// 刚进入，清除之前的数据残留
		Adafruit_NeoPixel_fill(BLACK, _seg->start, _seg_len);
	}

	if (__LED_STRIP_WHITE_GET_SOUND_TRIGGER__())
	{
		if (i == max_rate - 1)
		{
			i = 0;
		}
	}
	r1 = r * rate[i] / 100;
	g1 = g * rate[i] / 100;
	b1 = b * rate[i] / 100;
	w1 = w * rate[i] / 100;

	WS2812FX_copyPixels(_seg->start, _seg->start + 1, _seg->stop - _seg->start);

	/* 把第一个颜色，补到最后一个位置 */
	WS2812FX_setPixelColor_rgbw_with_max_brightness(_seg->stop, r1, g1, b1, w1);

	if (i < max_rate - 1)
		i++;

	return (30);
}

u16 led_strip_white_anim_pwr_off(void)
{
	Adafruit_NeoPixel_fill(BLACK, _seg->start, _seg_len);
	return 100;
}
