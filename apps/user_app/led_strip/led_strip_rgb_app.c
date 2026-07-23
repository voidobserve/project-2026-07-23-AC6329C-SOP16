#include "led_strip_rgb_app.h"
#include "led_strip_rgb_schedule.h"

#include "led_strand_effect.h" // fc_effect 定义

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
	if (fc_effect.dream_scene.speed < get_max_sp())
	{
		fc_effect.dream_scene.speed = get_max_sp();
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