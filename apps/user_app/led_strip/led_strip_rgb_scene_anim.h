#ifndef __LED_STRIP_RGB_SCENE_ANIM_H__
#define __LED_STRIP_RGB_SCENE_ANIM_H__

#include "typedef.h"

/*
	幻彩灯「场景」动画实现（重构版）
	================================================================================
	这里存放适配 app 下发的动画指令的新动画实现。
	旧的动画（led_strip_rgb_anim.c 里的 led_strip_rgb_anim_xxx）保持原样不使用，
	需要改造某个效果时，直接在本文件里改，不会影响旧代码。

	为什么另外写一份而不是改旧的？
		1. 旧动画用 options 里的 SIZE_SMALL / SIZE_MEDIUM / SIZE_LARGE 决定「段大小」，
		   而 app 下发的段大小是「多少个灯为一组」的任意值（1 ~ N），
		   两者对不上，且有的旧动画还会直接读 fc_effect.dream_scene.seg_size，
		   同一个参数存在两套来源，非常容易出错；
		2. 旧动画会原地旋转 _seg->colors 颜色池（memmove），
		   颜色池既是「app 下发的数据」又是「动画的运行状态」，
		   重复下发同一条指令时颜色会漂；
		3. 旧动画把段数量、底色写死在代码里，做不到「随 app 指令变化」。

	新动画的约定（写新动画时请遵守）：
		1. 函数签名与 WS2812FX 的 mode_ptr 一致：u16 (*)(void)，
		   返回值是「下一帧的间隔(ms)」，返回 10 表示 10ms 后跑下一帧；
		2. 所有可调参数从 led_strip_rgb_scene 读取，不再读 fc_effect.dream_scene：
			   led_strip_rgb_scene_get_leds_per_seg() 多少个灯为一组
			   led_strip_rgb_scene_is_reverse()      方向
			   led_strip_rgb_scene_get_color(i)      第 i 个颜色（自动循环取色）
			   led_strip_rgb_scene_get_background()  底色
			   速度由 _seg->speed（ms/帧）决定，亮度由 WS2812FX 的全局亮度决定；
		3. 运行状态只放在 _seg_rt 的 counter_mode_step / aux_param / aux_param3 上，
		   不要用 _seg_rt->aux_param2（它被 WS2812FX 的 FRAME/CYCLE 标志占用），
		   也不要使用 _seg->colors（颜色池由场景层维护）；
		4. 不要使用 options 里的 SIZE_*，灯珠分组统一由场景的 leds_per_seg 决定。
	================================================================================
*/

// 静态色：整条灯带显示第一个颜色
u16 led_strip_rgb_scene_anim_static(void);

// 渐变：整条灯带在颜色池之间平滑过渡
u16 led_strip_rgb_scene_anim_gradual(void);

// 跳变：按「段」跳变，每段一个颜色，颜色池循环
u16 led_strip_rgb_scene_anim_jump(void);

// 呼吸：颜色池里的颜色轮流在「底色 -> 颜色 -> 底色」之间呼吸
u16 led_strip_rgb_scene_anim_breath(void);

// 流水：颜色池按相邻两色混色拼接成一条循环，按灯串长度均分后沿灯带流动
// （只由颜色数量决定分段，app 下发的「多少个灯为一组」不参与）
u16 led_strip_rgb_scene_anim_running_water(void);

// 堆积：灯珠逐个点亮（颜色池循环），点亮整条后停留一会再换色
u16 led_strip_rgb_scene_anim_accumulation(void);

// 星空：底色上单灯随机闪
u16 led_strip_rgb_scene_anim_starry_sky(void);

// 星云：按灯带长度同时跑若干个随机闪烁的「星空团」，每个团位置/颜色都随机、
//       亮度按 0 -> 最亮 -> 0 呼吸，块大小 = 多少个灯为一组（上限为灯带长度的一半）
u16 led_strip_rgb_scene_anim_nebula(void);

// 流星：参考旧版带底色的流星，头部最亮、尾部顺灯带排出去，底色与流星色相同时会自动取反差分明的底色
u16 led_strip_rgb_scene_anim_meteor(void);

// 流星雨：与流星同一套拖尾（尾部会顺灯带排出去），拖尾长度固定 = 灯带长度，
//         所以颜色变多不会让流星变快；跑道放得下就几颗不同颜色同时跑，
//         放不下就一次只跑一颗、等这颗跑完再换下一个颜色接着跑（支持 ≥ 7 段颜色）
u16 led_strip_rgb_scene_anim_meteor_shower(void);

// 开合：dir = 0 开幕（从一端流水点亮到另一端 -> 全亮停一拍 -> 整条熄灭），
//       dir = 1 闭幕（整条点亮 -> 从尾部往回逐渐熄灭）；
//       整条同色，每跑完一轮换颜色池里的下一种颜色（不按「多少个灯为一组」分组）；
//       一趟的时长只由速度决定（帧间隔按灯带长度换算），长短灯带都不会闪
//       （见 .c 里的 ANIM_OPEN_CLOSE_WALK_*_MS）
u16 led_strip_rgb_scene_anim_open_close(void);

// 跑动：颜色池里的颜色形成「点」在底色上跑动
// （只有 1 种颜色时是单色跑动，多种颜色时多个颜色的点同时跑）
u16 led_strip_rgb_scene_anim_run(void);

// 跑动集合：单独跑一种动画并循环执行 —— 内置 20 个「跑动」子动画
// （红/绿/蓝/黄/紫/青/白、红绿蓝、紫青黄、七彩，以及它们的反向版本），
// 每个子动画跑完一圈就切到下一个，全部跑完再从第一个重新开始
// 配色和方向写在动画层的内置表里，速度/亮度仍然跟 app 下发的动画参数
u16 led_strip_rgb_scene_anim_run_collection(void);

/*
	声控动画（4 种，对应 app 下发的声控模式索引 0 ~ 3）
	================================================================================
	都由声控模块的声音信息驱动（见 led_strip_voice.c）：
		get_sound_triggered_by_led_strip_rgb() 这一帧有没有声音
		led_strip_voice_get_level()            声音强度 0 ~ 100
	所有长度/位置都按当前段的真实长度 _seg_len 计算并夹紧，不会超过灯串数量。
	================================================================================
*/

// 声控响应要快，所以声控动画的帧间隔固定成这个值（ms），不跟 app 下发的动画速度
#define ANIM_SOUND_FRAME_MS 20

// 声控·能量：声音越大，从灯带头部亮起的能量条越长，声音停下后平滑回落
u16 led_strip_rgb_scene_anim_sound_energy(void);

// 声控·节奏：每检测到一次声音（一拍），灯光就从灯带一端打到另一端，颜色每拍换一种
u16 led_strip_rgb_scene_anim_sound_rhythm(void);

// 声控·频谱：每颗灯当成一根频谱柱（柱高 = 亮度），有声音时各自跳到不同高度再逐帧回落
u16 led_strip_rgb_scene_anim_sound_spectrum(void);

// 声控·滚动：颜色块沿灯带滚动，有声音时加速滚动一段时间
u16 led_strip_rgb_scene_anim_sound_scroll(void);

// 关灯动画（设备关机时使用）
u16 led_strip_rgb_scene_anim_power_off(void);

// 复位动画内部状态（切换模式时由场景调度调用）
void led_strip_rgb_scene_anim_reset(void);

#endif
