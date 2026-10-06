// 幻彩灯「场景」动画实现（重构版）
// 说明见 led_strip_rgb_scene_anim.h 顶部的约定
#include "led_strip_rgb_scene_anim.h"

#include <string.h>

#include "Adafruit_NeoPixel.H"
#include "WS2812FX.H"
#include "led_strip_rgb_scene.h"
#include "led_strip_voice.h" // 声控信息（触发标志、声音强度）
#include "user_config.h"
#include "ws2812fx_tool.h" // WS2812FX_color_blend()

/* ------------------------------------------------------------------------------ */
/*                                   速度配置                                      */
/* ------------------------------------------------------------------------------ */

/*
	动画节奏配置
	================================================================================
	为了同时避开「一帧一帧跳变」和「快得看不清」，动画节奏拆成两部分：
		- 帧间隔 frame_ms：固定成较小的值，保证画面平滑（WS2812FX 一帧最快 10ms）；
		- 一轮时长 round_ms：由速度百分比在 [最慢, 最快] 之间插值，
		  再用「一轮的总步数」换算出每帧的步进。
	速度 0%   -> 一轮用 *_ROUND_SLOW_MS（最慢）
	速度 100% -> 一轮用 *_ROUND_FAST_MS（最快）

	需要调节手感的动画，只改下面几个宏即可。
	================================================================================
*/
#define ANIM_FRAME_MS 20 // 每帧间隔（ms）：越小越平滑

// 流水：颜色循环沿灯带流转「一轮」的时长
#define ANIM_WATER_ROUND_SLOW_MS 6000
#define ANIM_WATER_ROUND_FAST_MS 1500

// 渐变：相邻两色之间渐变「一轮」的时长
#define ANIM_GRADUAL_ROUND_SLOW_MS 6000
#define ANIM_GRADUAL_ROUND_FAST_MS 1200

// 每个颜色在「颜色循环」里占的相位长度（流水用）
#define ANIM_COLOR_UNIT 256

/* ------------------------------------------------------------------------------ */
/*                                   通用工具                                      */
/* ------------------------------------------------------------------------------ */

/*
	命名约定（本文件统一使用，方便阅读和维护）
		段：  leds_per_seg   每段多少个灯（就是 app 下发的「多少个灯为一组」）
		      seg_index     第几段（从 0 开始）
		颜色：color_count   颜色数量（不含底色）
		      color_index   颜色在颜色池中的下标
		      color_offset  当前配色相对颜色池的偏移
		      background_color 底色
		位置：led_pos       逻辑位置（0 = 控制板连接灯带的那一端）
		      led_index     实际灯珠下标（已按方向换算）
		时间：frame_interval_ms 帧间隔（ms）
		      cycle_time_ms     一个动作周期的时长（ms）
		      step_per_frame    每帧步进
		状态：breath_level  呼吸的亮度等级（0 ~ 255）
		      head_pos      头部位置
		      tail_len      拖尾长度
		      tail_offset   拖尾的第几个灯（0 = 头部）
		      lit_leds      已点亮的灯数
		      lit_len       亮区长度
*/

// 颜色结构体 -> WS2812FX 使用的 0xRRGGBB
static u32 anim_pack_color(color_t color)
{
    return ((u32)color.r << 16) | ((u32)color.g << 8) | color.b;
}

// 当前帧的间隔（ms），由场景层按速度百分比换算后写入段参数
static u16 anim_frame_interval_ms(void)
{
    return _seg->speed;
}

// 每段多少个灯（段大小）
static u8 anim_leds_per_seg(void)
{
    u16 leds_per_seg =
        led_strip_rgb_scene_get_leds_per_seg(); // 每段多少个灯（app 下发的 byte7）

    /*
        app 协议里 byte7 是「多少个灯为一组」：
            byte7 = 0        -> 所有灯为同一组
            byte7 > 灯带长度 -> 所有灯为同一组
        这两种情况都换算成整条灯带（真实的段长度 _seg_len）；
        其它情况只做「至少 1 个灯」的下限保护，上限就是灯带长度。
    */
    if ((0 == leds_per_seg) || (leds_per_seg > _seg_len)) {
        leds_per_seg = _seg_len;
    }
    if (leds_per_seg > 0xFF) {
        // 返回值是 u8，灯带很长时不能截断
        leds_per_seg = 0xFF;
    }
    if (0 == leds_per_seg) {
        leds_per_seg = 1;
    }

    return (u8)leds_per_seg;
}

/*
    开合动画的节奏
    ================================================================================
    开合是「从一端走到另一端」的动画，节奏必须跟着灯带长度走，
    否则 6 个灯时「一趟」只有几帧，速度开到最快就会变成整条灯带在闪烁：

        - 一趟（walk：从一端走到另一端）的时长由速度百分比在
          [ANIM_OPEN_CLOSE_WALK_SLOW_MS, ANIM_OPEN_CLOSE_WALK_FAST_MS] 之间插值；
        - 帧间隔 = 一趟时长 / 一趟的帧数：
              灯带短（本工程 6 灯）-> 每帧 1 个灯，帧间隔自然变大
                                    （6 灯 + 最快 = 133ms/帧，一趟 800ms，不会闪）；
              灯带很长             -> 帧数受 ANIM_OPEN_CLOSE_FRAME_MS_MIN(10ms) 限制，
                                    这时每帧多走几个灯，一趟的时长仍然不变；
        - 停留时长（全亮/全灭停一拍）= 一趟时长，跟着灯带长度自适应。

    结论：**不管灯带多长，一趟的时长都只由速度决定**，
    所以开到最快也只是 800ms 走完一趟，不会变成整条灯带闪烁。
    ================================================================================
*/
#define ANIM_OPEN_CLOSE_WALK_SLOW_MS 2400 // 速度 0%：一趟走 2400ms
#define ANIM_OPEN_CLOSE_WALK_FAST_MS 800  // 速度 100%：一趟走 800ms（再快就闪了）
#define ANIM_OPEN_CLOSE_FRAME_MS_MIN 10   // 帧间隔下限（WS2812FX 一帧最快 10ms）

/*
    跑动一组多少个灯（点宽）
    ================================================================================
    「点 + 间隔」为一组，一组里亮的灯数 = 点宽，点与点之间隔同样多的底色灯：

        byte7 = 0（app 没指定）-> 按灯带长度自动分组，灯带越长一组灯越多：
                                  本工程 6 灯 = 2 个灯一组，图案 ● ● ○ ○ ● ●
        byte7 非 0            -> 一组 = byte7 个灯

    一组最少 1 个灯、最多 灯带长度 / 2（超了就整条灯带都是点，看不出间隔）。
    ================================================================================
*/
#define ANIM_RUN_GROUP_DIV 3 // byte7 = 0 时：一组灯数 = 灯带长度 / 3（至少 1 个）

static u16 anim_run_group_len(void)
{
    u16 group_len = led_strip_rgb_scene_get_leds_per_seg(); // app 下发的 byte7
    u16 max_group_len = (u16)(_seg_len / 2); // 一组最多占灯带的一半

    if (max_group_len < 1) {
        max_group_len = 1;
    }

    if (0 == group_len) {
        // app 没指定：按灯带长度分组（灯带越长，一组灯越多）
        group_len = (u16)(_seg_len / ANIM_RUN_GROUP_DIV);
    }
    if (group_len < 1) {
        group_len = 1; // 最小 1 个灯一组
    }
    if (group_len > max_group_len) {
        group_len = max_group_len;
    }

    return group_len;
}

/*
    跑动一个「点 + 间隔」单元的宽度
    点与点之间隔同样多的底色灯，所以单元 = 点宽 * 2：
        6 灯（本工程，2 个灯一组）-> 单元 4：图案 ● ● ○ ○ ● ●
*/
static u16 anim_run_unit_len(u16 dot_width)
{
    u16 unit = (u16)(dot_width * 2); // 点 + 同样多的底色

    if (unit < 2) {
        unit = 2; // 兜底：一个点 + 一个底色
    }

    return unit;
}

// 颜色数量（至少 1）
static u8 anim_color_count(void)
{
    return led_strip_rgb_scene_get_color_num();
}

// 取颜色池里的第 color_index 个颜色（自动循环取色）
static u32 anim_pick_color(u8 color_index)
{
    return anim_pack_color(led_strip_rgb_scene_get_color(color_index));
}

// 底色
static u32 anim_background_color(void)
{
    return anim_pack_color(led_strip_rgb_scene_get_background());
}

// 随机取一个非黑的颜色下标（颜色池全是黑色时，就返回抽到的那个）
static u8 anim_random_color_index(void)
{
    u8 color_count = anim_color_count(); // 颜色数量（至少 1）
    u8 color_index =
        (u8)WS2812FX_random16_lim(color_count); // 随机抽到的颜色下标
    u8 offset;                                  // 往后找非黑颜色时用的偏移
    u8 probe_index;                             // 试探用的颜色下标

    if (0 != anim_pick_color(color_index)) {
        return color_index;
    }

    for (offset = 1; offset < color_count; offset++) {
        probe_index = (u8)((color_index + offset) % color_count);
        if (0 != anim_pick_color(probe_index)) {
            return probe_index;
        }
    }

    return color_index;
}

/*
    颜色的亮度（0 ~ 255，按人眼对 G 最敏感做的近似加权）
*/
static u8 anim_color_level(u32 color)
{
    u32 r = (color >> 16) & 0xFF; // 红
    u32 g = (color >> 8) & 0xFF;  // 绿
    u32 b = color & 0xFF;         // 蓝

    return (u8)(((r * 3) + (g * 6) + b) / 10);
}

/*
    流星拖尾的两个端点色（头 / 尾）
    ================================================================================
    流星是靠「亮度从头部往尾部递减」画出来的，拖尾必须和底色有明显反差，
    否则亮底（青底 / 粉底）上整条灯带都发白，看不出流星在哪：

        - 普通底色：头 = 流星色（流星色本身太暗就用白），尾 = 黑
              —— 尾巴在亮底上是黑的，一眼就能看出流星；
        - 底色本身很亮（接近白，例如白底）：反过来，头 = 黑、尾 = 白，
              否则白流星在白底上根本看不见。

    判据：底色亮度 >= ANIM_METEOR_BRIGHT_BG_LEVEL 就算「亮底」。
    现场觉得流星不够明显时，只调下面两个宏即可。
    ================================================================================
*/
#define ANIM_METEOR_BRIGHT_BG_LEVEL 224 // 底色亮度（0~255）超过它就算「亮底」
#define ANIM_METEOR_DARK_HEAD_LEVEL 24  // 流星色比它还暗时，头部改用白色
#define ANIM_METEOR_WHITE ((u32)0x00FFFFFF)
#define ANIM_METEOR_BLACK ((u32)0x000000)

typedef struct
{
    u32 head; // 流星头部（最亮处）的颜色
    u32 tail; // 流星尾部（最暗处）的颜色
} anim_meteor_trail_t;

// 算出一颗流星的拖尾端点色（头 / 尾）
static anim_meteor_trail_t anim_meteor_trail_colors(u32 meteor_color)
{
    u8 bg_level = anim_color_level(anim_background_color()); // 底色亮度
    u8 meteor_level = anim_color_level(meteor_color);        // 流星色亮度
    anim_meteor_trail_t trail;                               // 返回值

    if (bg_level < ANIM_METEOR_BRIGHT_BG_LEVEL) {
        // 普通底色：头 = 流星色（太暗就用白），尾 = 黑
        trail.head = (meteor_level < ANIM_METEOR_DARK_HEAD_LEVEL)
                         ? ANIM_METEOR_WHITE
                         : meteor_color;
        trail.tail = ANIM_METEOR_BLACK;
    } else {
        // 亮底（白底）：反过来说，头 = 黑、尾 = 白
        trail.head = ANIM_METEOR_BLACK;
        trail.tail = ANIM_METEOR_WHITE;
    }

    return trail;
}
 

/*
	逻辑位置 -> 实际灯珠下标
	逻辑位置 0 表示「控制板连接灯带的那一端」，
	动画只关心逻辑位置，方向统一在这里换算。
*/
static u16 anim_led_index_of(u16 led_pos)
{
    u16 led_count = _seg_len; // 当前段的灯珠数量

    /*
		段长度异常（理论上不会发生）时直接返回段的起始灯珠：
		u16 的 0 - 1 会下溢成 65535，会把像素写到灯带缓冲区外面去。
	*/
    if (0 == led_count) {
        return _seg->start;
    }

    if (led_pos >= led_count) {
        led_pos = (u16)(led_count - 1);
    }
    if (led_strip_rgb_scene_is_reverse()) {
        led_pos = (u16)(led_count - 1 - led_pos);
    }

    return (u16)(_seg->start + led_pos);
}

/*
    逻辑位置 -> 实际灯珠下标（指定方向）
    跑动集合里的子动画带自己的方向（可能和场景方向不同），所以单独给一个带方向的版本。
*/
static u16 anim_led_index_of_dir(u16 led_pos, u8 reverse)
{
    u16 led_count = _seg_len; // 当前段的灯珠数量

    if (0 == led_count) {
        return _seg->start;
    }

    if (led_pos >= led_count) {
        led_pos = (u16)(led_count - 1);
    }
    if (reverse) {
        led_pos = (u16)(led_count - 1 - led_pos);
    }

    return (u16)(_seg->start + led_pos);
}

// 整条灯带填充同一个颜色
static void anim_fill_strip(u32 color)
{
    Adafruit_NeoPixel_fill(color, _seg->start, _seg_len);
}

/*
	速度换算成「时长」
	slow_ms：速度 0% 时一个动作周期要用的时间
	fast_ms：速度 100% 时一个动作周期要用的时间
*/
static u32 anim_cycle_time_ms(u32 slow_ms, u32 fast_ms)
{
    u32 speed =
        led_strip_rgb_scene.speed; // app 下发的动画速度百分比（0 ~ 100）

    // 无符号运算，slow_ms < fast_ms 时会下溢成一个很大的数，这里先兜底
    if (slow_ms <= fast_ms) {
        return fast_ms;
    }

    if (speed > 100) {
        speed = 100;
    }

    return slow_ms - (slow_ms - fast_ms) * speed / 100;
}

/*
	把「一个周期共 total_steps 步、目标时长 cycle_time_ms」换算成每帧要走多少步，
	这样速度改变时，动画快慢会跟着变，而不是固定帧数。
*/
static u16 anim_step_per_frame(u16 total_steps, u32 cycle_time_ms)
{
    u32 frame_interval_ms = anim_frame_interval_ms(); // 当前帧间隔（ms）
    u32 step_per_frame;                               // 每帧要走多少步

    if (0 == cycle_time_ms) {
        cycle_time_ms = 1;
    }

    step_per_frame =
        ((u32)total_steps * frame_interval_ms + cycle_time_ms - 1) /
        cycle_time_ms;
    if (step_per_frame < 1) {
        step_per_frame = 1;
    }
    if (step_per_frame > 64) {
        step_per_frame = 64;
    }

    return (u16)step_per_frame;
}

/*
	把「一轮共 total_steps 步、目标时长 round_ms、帧间隔 frame_ms」换算成每帧的步进
	（走固定节奏的动画用：帧间隔固定，速度快慢只影响一轮时长）
*/
static u16 anim_step_of_round(u16 total_steps, u32 round_ms, u16 frame_ms)
{
    u32 step; // 每帧的步进

    if (0 == round_ms) {
        round_ms = 1;
    }
    if (0 == frame_ms) {
        frame_ms = ANIM_FRAME_MS;
    }

    step = ((u32)total_steps * frame_ms + round_ms - 1) / round_ms;
    if (step < 1) {
        step = 1;
    }
    if (step > total_steps) {
        step = total_steps;
    }

    return (u16)step;
}

/*
	是否本模式刚切进来的「第一帧」（取完就清标志）
	星空、星云、流星这类叠加型动画，需要先把上一轮动画残留的画面清掉
*/
static u8 anim_first_frame = 1;
static u8 anim_take_first_frame(void)
{
    u8 first = anim_first_frame; // 本帧是不是切换模式后的第一帧（取完就清标志）

    anim_first_frame = 0;
    return first;
}

// 整条灯带位移一个灯（方向由场景决定），空出来的位置由调用者填色
static void anim_shift_by_one_led(void)
{
    if (_seg_len < 2) {
        return;
    }

    if (led_strip_rgb_scene_is_reverse()) {
        // 反向：把后面的灯往前推
        WS2812FX_copyPixels(_seg->start, _seg->start + 1, _seg_len - 1);
    } else {
        WS2812FX_copyPixels(_seg->start + 1, _seg->start, _seg_len - 1);
    }
}

/* ------------------------------------------------------------------------------ */
/*                                   动画实现                                      */
/* ------------------------------------------------------------------------------ */

// 静态色
u16 led_strip_rgb_scene_anim_static(void)
{
    anim_fill_strip(anim_pick_color(0));
    return 1000;
}

/*
	渐变：参考重构前的 led_strip_rgb_anim_mutil_fade
	================================================================================
	- 灯带按「段大小」分段，第 k 段在「颜色池里的第 k 个颜色」和「第 k+1 个颜色」
	  之间做渐变（最后一个颜色会渐回第一个颜色）；
	- 渐变进度到 100% 时，配色整体在颜色池里前进一位：
	  此时画面正好是这个颜色对的「结束色」，也就是下一对的「起始色」，
	  所以切换是无缝的，不会跳色；
	- 帧间隔固定成 ANIM_FRAME_MS（小值），一轮时长由速度在
	  [ANIM_GRADUAL_ROUND_SLOW_MS, ANIM_GRADUAL_ROUND_FAST_MS] 之间插值
	  —— 速度快慢只影响渐变节奏，不会出现「一帧一帧跳变」。
	- 颜色池只有一种颜色时凑不出「一对颜色」，另一种颜色按本色取一个反差的：
	  本色不是黑色 -> 补黑色（「本色 <-> 黑」，像熄灭式呼吸）；
	  本色是黑色 -> 补白色（「黑 <-> 白」，否则整条灯带一直是黑的看不见）。
	================================================================================
*/

/*
	颜色池只有一种颜色时，渐变补上的「另一种颜色」（0x00RRGGBB，与场景颜色池写法一致）：
		- 本色不是黑色 -> 补黑色：画面在「本色 <-> 黑」之间渐变（像熄灭式呼吸）；
		- 本色是黑色   -> 补白色：画面在「黑 <-> 白」之间渐变（否则全黑看不见）。
*/
#define ANIM_GRADUAL_WHITE ((u32)0x00FFFFFF)
#define ANIM_GRADUAL_BLACK ((u32)0x000000)

u16 led_strip_rgb_scene_anim_gradual(void)
{
    /*
		颜色池只有一种颜色时，凑不出「一对颜色」，另一种颜色按本色取一个反差的
		（见 ANIM_GRADUAL_WHITE / ANIM_GRADUAL_BLACK）：本色不是黑色就补黑色，
		本色是黑色就补白色，画面在两者之间来回渐变，看起来像呼吸。
		other_first 是「方向标志」（0 = 本色在前，1 = 另一种颜色在前）：
		这一轮的结束色就是下一轮的起始色，所以接起来依然无缝、不会跳色。
	*/
    u8 leds_per_seg = anim_leds_per_seg(); // 每段多少个灯
    u8 color_count = anim_color_count();   // 颜色数量（至少 1）
    u8 color_offset =
        (u8)_seg_rt->aux_param; // 多色时：当前配色在颜色池里的偏移
    u8 single_color =
        (1 == color_count); // 1 = 颜色池只有一种颜色（需要补一对颜色）
    u32 single_color_value = anim_pick_color(0); // 单色时颜色池里唯一那个颜色
    u32 single_other_color =                     // 单色时补的「另一种颜色」：
        (0 == single_color_value) ? ANIM_GRADUAL_WHITE : ANIM_GRADUAL_BLACK;
    u8 other_first =
        (u8)(_seg_rt->aux_param3 &
             1); // 单色的方向标志：0 = 本色在前，1 = 另一种颜色在前
    u16 level = (u16)_seg_rt->counter_mode_step; // 渐变进度 0 ~ 255
    u16 frame_ms = ANIM_FRAME_MS;                // 帧间隔（ms）
    u16 step =                                   // 每帧的渐变步进
        anim_step_of_round(256,
                           anim_cycle_time_ms(ANIM_GRADUAL_ROUND_SLOW_MS,
                                              ANIM_GRADUAL_ROUND_FAST_MS),
                           frame_ms);
    u16 led_pos;    // 逻辑灯位置（0 = 控制板接灯带那一端）
    u8 seg_index;   // 该灯属于第几段（多色时按段取色）
    u32 color_from; // 这一帧该灯渐变的起始颜色
    u32 color_to;   // 这一帧该灯渐变的结束颜色

    // 兜底：运行状态被外部改动过时先夹回合法范围
    if (level > 255) {
        level = 0;
    }

    for (led_pos = 0; led_pos < _seg_len; led_pos++) {
        if (single_color) {
            // 单色：一对颜色 = {本色, 另一种颜色}，两轮之间起点/终点互换
            color_from = other_first ? single_other_color : single_color_value;
            color_to = other_first ? single_color_value : single_other_color;
        } else {
            seg_index = (u8)(led_pos / leds_per_seg);

            color_from = anim_pick_color((u8)(seg_index + color_offset));
            color_to = anim_pick_color((u8)(seg_index + color_offset + 1));
        }

        WS2812FX_setPixelColor(
            anim_led_index_of(led_pos),
            WS2812FX_color_blend(color_from, color_to, (u8)level));
    }

    level = (u16)(level + step);
    if (level >= 255) {
        // 渐变到 100%：换到下一组颜色（此时画面无缝衔接）
        level = 0;
        if (single_color) {
            // 单色：这一轮走到「另一种颜色」了，下一轮从它渐回本色，接起来不跳色
            _seg_rt->aux_param3 = (u16)((_seg_rt->aux_param3 + 1) & 1);
        } else {
            _seg_rt->aux_param = (u8)((color_offset + 1) % color_count);
        }
        SET_CYCLE;
    }
    _seg_rt->counter_mode_step = level;

    return frame_ms;
}

/*
	跳变：按「段」跳变，每段一个颜色，颜色池依次循环
	- 段大小 = 1、且颜色池有多种颜色时，就是「每隔一个灯用颜色池里的一种颜色」；
	- 段大小凑不满整条灯带时，剩余灯珠接着下一段继续排色，不会留黑：
	  例如 6 个灯、段大小 = 5：前 5 个灯用颜色 0，第 6 个灯用颜色 1；
	- 与呼吸不同：跳变每一轮都会换色（每 4 帧配色偏移 +1），
	  所以同一个灯的颜色会不断轮换，形成「跳变」。
*/
u16 led_strip_rgb_scene_anim_jump(void)
{
    u8 leds_per_seg = anim_leds_per_seg();    // 每段多少个灯
    u8 color_offset = (u8)_seg_rt->aux_param; // 当前配色在颜色池里的偏移
    u16 led_pos;  // 逻辑灯位置（0 = 控制板接灯带那一端）
    u8 seg_index; // 该灯属于第几段

    for (led_pos = 0; led_pos < _seg_len; led_pos++) {
        seg_index = (u8)(led_pos / leds_per_seg); // 第几段

        /*
			取色下标会对颜色数量取模，所以段序号超过颜色数量时，
			颜色会从头循环，不会把剩余的灯珠留黑。
		*/
        WS2812FX_setPixelColor(anim_led_index_of(led_pos),
                               anim_pick_color((u8)(seg_index + color_offset)));
    }

    // 每 4 帧换一次颜色组合
    if (++_seg_rt->counter_mode_step >= 4) {
        _seg_rt->counter_mode_step = 0;
        _seg_rt->aux_param = (u8)((color_offset + 1) % anim_color_count());
    }

    return anim_frame_interval_ms();
}

/*
	呼吸：每个「段」用颜色池里的一种颜色呼吸
	- 每个灯的颜色由「位置」固定决定：第 n 段用颜色池里的第 n 个颜色；
	  一个呼吸周期结束后配色不会改变（这一点与跳变不同，跳变每轮会换色）；
	- 段大小 = 1、且颜色池有多种颜色时，就是「每隔一个灯用颜色池里的一种颜色来填充」：
	  第 0 个灯用颜色 0，第 1 个灯用颜色 1 …… 用完一遍再从头循环；
	- 段大小凑不满整条灯带时，剩下的灯珠接着下一段继续排色，不会留黑：
	  例如 6 个灯、段大小 = 5：前 5 个灯用颜色 0，第 6 个灯用颜色 1。
*/
u16 led_strip_rgb_scene_anim_breath(void)
{
    u8 leds_per_seg = anim_leds_per_seg(); // 每段多少个灯
    u16 breath_pos =
        (u16)_seg_rt
            ->counter_mode_step; // 呼吸位置 0 ~ 511（0~255 变亮，255~511 变暗）
    u16 step_per_frame =         // 每帧的呼吸步进（一个周期共 512 步）
        anim_step_per_frame(512, anim_cycle_time_ms(6000, 1000));
    u8 breath_level; // 这一帧的亮度等级 0 ~ 255
    u16 led_pos;     // 逻辑灯位置（0 = 控制板接灯带那一端）
    u8 seg_index;    // 该灯属于第几段
    u32 color;       // 该灯这一帧的颜色（底色 -> 该段的颜色）

    // 兜底：运行状态被外部改动过时，先夹回合法范围，避免 511 - breath_pos 下溢
    if (breath_pos > 511) {
        breath_pos = 0;
    }

    // 0 -> 255 -> 0 的三角波，作为呼吸的亮度包络
    if (breath_pos > 255) {
        breath_level = (u8)(511 - breath_pos);
    } else {
        breath_level = (u8)breath_pos;
    }

    for (led_pos = 0; led_pos < _seg_len; led_pos++) {
        seg_index = (u8)(led_pos / leds_per_seg); // 该灯属于第几段

        /*
			每段取颜色池里对应的颜色（按位置固定，不随周期变化），从底色渐变到该颜色。
			取色下标会对颜色数量取模，所以段序号超过颜色数量时，
			颜色会从头循环，不会把剩余的灯珠留黑。
		*/
        color = WS2812FX_color_blend(anim_background_color(),
                                     anim_pick_color(seg_index), breath_level);
        WS2812FX_setPixelColor(anim_led_index_of(led_pos), color);
    }

    // 一个呼吸周期结束：配色不变（每个灯的颜色由位置固定决定），只标记一轮完成
    breath_pos = (u16)(breath_pos + step_per_frame);
    if (breath_pos > 511) {
        breath_pos = 0;
        SET_CYCLE;
    }
    _seg_rt->counter_mode_step = breath_pos;

    return anim_frame_interval_ms();
}

/*
	流水：参考重构前的 WS2812FX_mode_running_lights
	================================================================================
	- 把「一个颜色循环」按灯串长度均分到每个灯上：颜色池里相邻两个颜色之间做
	  混色渐变，第 0 色 -> 混色 -> 第 1 色 -> 混色 -> …… -> 最后一色 -> 混回第 0 色；
	  例如六个灯 + 红蓝两色：红-红蓝-蓝-蓝红-红蓝-蓝（红蓝各占一半）；
	- 怎么分段只由「颜色数量」决定，app 下发的「多少个灯为一组」不参与，
	  这样每种颜色分到的灯珠数一定是均匀的（否则红分到的灯珠会很少）；
	- 整段颜色循环沿灯带正向流动（每帧前进一点相位），就是「流水」；
	- 帧间隔固定成 ANIM_FRAME_MS，一轮（相位走完一个完整颜色循环）的时长由速度在
	  [ANIM_WATER_ROUND_SLOW_MS, ANIM_WATER_ROUND_FAST_MS] 之间插值；
	  重构前是「一帧挪一个灯」，六个灯时 6 帧就流完一圈，快得看不清。
	================================================================================
*/
u16 led_strip_rgb_scene_anim_running_water(void)
{
    u8 color_count = anim_color_count(); // 颜色数量（至少 1）
    u16 led_count =
        (_seg_len > 0) ? (u16)_seg_len : 1; // 灯带长度（按它均分一个颜色循环）
    u16 cycle_units =                       // 一个完整颜色循环的总相位
        (u16)((u16)color_count * ANIM_COLOR_UNIT);
    u16 frame_ms = ANIM_FRAME_MS; // 帧间隔（ms）
    u16 step =                    // 每帧流过的相位
        anim_step_of_round(cycle_units,
                           anim_cycle_time_ms(ANIM_WATER_ROUND_SLOW_MS,
                                              ANIM_WATER_ROUND_FAST_MS),
                           frame_ms);
    u16 flow = // 当前流到哪个相位（0 ~ cycle_units-1）
        (u16)(_seg_rt->counter_mode_step % cycle_units);
    u16 led_pos;    // 逻辑灯位置（0 = 控制板接灯带那一端）
    u32 phase;      // 该灯在颜色循环里的相位
    u8 color_index; // 相位落在第几个颜色上
    u8 level;       // 相邻两色之间的混色比例 0 ~ 255

    for (led_pos = 0; led_pos < _seg_len; led_pos++) {
        phase = ((u32)led_pos * cycle_units) / led_count;

        /*
			相位随 flow 递减，颜色循环才会朝着灯带末端流动；
			用加法会朝着灯带起点流动（方向是反的）。
		*/
        phase = (phase + cycle_units - flow) % cycle_units;
        color_index = (u8)(phase / ANIM_COLOR_UNIT);
        level = (u8)(phase % ANIM_COLOR_UNIT);

        WS2812FX_setPixelColor(
            anim_led_index_of(led_pos),
            WS2812FX_color_blend(anim_pick_color(color_index),
                                 anim_pick_color((u8)(color_index + 1)),
                                 level));
    }

    flow = (u16)(flow + step);
    if (flow >= cycle_units) {
        flow = (u16)(flow - cycle_units);
        SET_CYCLE;
    }
    _seg_rt->counter_mode_step = flow;

    return frame_ms;
}

/*
    堆积一次点亮多少个灯（一组多少个灯）
    ================================================================================
    直接用 app 下发的 byte7（原始值），但堆积的语义和别的不一样：
        byte7 = 0        -> 1 个灯一组：逐个灯堆积
                            （如果按「所有灯为同一组」处理，就是整条灯带一起亮、
                              一起灭，看起来只是整条灯带在闪）
        byte7 > 灯带长度 -> 整条灯带一组（协议里的「所有灯为同一组」）
        其它             -> 取原值（≥ 1）
    ================================================================================
*/
static u16 anim_accumulation_group_len(void)
{
    u16 group_len =
        led_strip_rgb_scene_get_leds_per_seg(); // app 下发的 byte7（原始值）

    if (0 == group_len) {
        return 1; // byte7 = 0：一个灯一组（逐灯堆积）
    }
    if (group_len > _seg_len) {
        group_len = _seg_len; // 超过灯带长度：整条灯带一组
    }
    if (0 == group_len) {
        group_len = 1; // 灯带长度为 0 时的兜底
    }

    return group_len;
}

/*
    堆积的节奏
    ================================================================================
    堆积是「一组一组点亮」，节奏不能直接跟 app 的帧间隔走（最快 10ms 一帧），
    否则 6 个灯上 60ms 就铺满，看起来就是「一下就亮完了」：

        - 「点亮整条灯带」的时长由速度百分比在
          [ANIM_ACCUM_ROUND_SLOW_MS, ANIM_ACCUM_ROUND_FAST_MS] 之间插值；
        - steps    = ceil(灯带长度 / 一组灯数)（一共要点几步）
        - frame_ms = round_ms / steps（≥ ANIM_ACCUM_FRAME_MS_MIN）（每帧点一组）
        - 点满后停留 steps 帧（= 一个 round_ms），再换下一种颜色重新堆积

    灯带再长，也只是 steps 变多、frame_ms 变小（下限 10ms），
    点亮整条灯带的时长始终保持 900ms(最快) ~ 3000ms(最慢)。
    ================================================================================
*/
#define ANIM_ACCUM_ROUND_SLOW_MS 3000 // 速度 0%：点亮整条灯带用 3000ms
#define ANIM_ACCUM_ROUND_FAST_MS 900  // 速度 100%：点亮整条灯带用 900ms
#define ANIM_ACCUM_FRAME_MS_MIN  10   // 帧间隔下限（WS2812FX 一帧最快 10ms）

// 堆积：灯珠逐个点亮
u16 led_strip_rgb_scene_anim_accumulation(void)
{
    u16 group_len =
        anim_accumulation_group_len(); // 一次点亮一组，一组多少个灯
    u32 round_ms = // 点亮整条灯带的目标时长
        anim_cycle_time_ms(ANIM_ACCUM_ROUND_SLOW_MS, ANIM_ACCUM_ROUND_FAST_MS);
    u16 steps = // 一共要点几步（一步 = 一组）
        (u16)((_seg_len + group_len - 1) / group_len);
    u16 frame_ms;                            // 每帧点一组，帧间隔由它换算
    u8 color_index = (u8)_seg_rt->aux_param; // 当前堆积用的颜色下标
    u16 lit_leds = (u16)_seg_rt->counter_mode_step; // 已经点亮的灯数
    u16 led_pos; // 逻辑灯位置（0 = 控制板接灯带那一端）

    if (steps < 1) {
        steps = 1;
    }
    frame_ms = (u16)(round_ms / steps);
    if (frame_ms < ANIM_ACCUM_FRAME_MS_MIN) {
        frame_ms = ANIM_ACCUM_FRAME_MS_MIN;
    }

    for (led_pos = 0; led_pos < _seg_len; led_pos++) {
        if (led_pos < lit_leds) {
            WS2812FX_setPixelColor(anim_led_index_of(led_pos),
                                   anim_pick_color(color_index));
        } else {
            WS2812FX_setPixelColor(anim_led_index_of(led_pos),
                                   anim_background_color());
        }
    }

    if (lit_leds >= _seg_len) {
        // 全部点亮后停留一拍（steps 帧 = 一个 round_ms），
        // 再换下一种颜色重新堆积（aux_param3 在这里用作「停留帧数」的计数器）
        if (++_seg_rt->aux_param3 >= steps) {
            _seg_rt->aux_param3 = 0;
            _seg_rt->counter_mode_step = 0;
            _seg_rt->aux_param = (u8)((color_index + 1) % anim_color_count());
            SET_CYCLE;
        }
    } else {
        // 每帧点亮一组（一组 = group_len 个灯）
        _seg_rt->aux_param3 = 0;
        lit_leds = (u16)(lit_leds + group_len);
        if (lit_leds > _seg_len) {
            lit_leds = _seg_len;
        }
        _seg_rt->counter_mode_step = lit_leds;
    }

    return frame_ms;
}

// 星空：底色上单灯随机闪
u16 led_strip_rgb_scene_anim_starry_sky(void)
{
    u16 led_pos = (u16)_seg_rt->aux_param3;  // 随机亮点所在的位置（逻辑位置）
    u8 color_index = (u8)_seg_rt->aux_param; // 随机亮点用的颜色下标

    // 刚进入星空：上一轮动画的画面还留在灯带上（余晖是慢慢淡的），先清干净
    if (anim_take_first_frame()) {
        anim_fill_strip(anim_background_color());
    }

    // 让上一帧的亮点留下余晖（FADE_* 由模式表写在段选项里）
    WS2812FX_fade_out();

    // 每 4 帧换一个随机位置和随机颜色
    // （位置用 aux_param3：u16，灯珠多的时候也不会被截断）
    if (++_seg_rt->counter_mode_step >= 4) {
        _seg_rt->counter_mode_step = 0;
        _seg_rt->aux_param3 = WS2812FX_random16_lim(_seg_len);
        _seg_rt->aux_param = (u8)WS2812FX_random16_lim(anim_color_count());
        led_pos = (u16)_seg_rt->aux_param3;
        color_index = (u8)_seg_rt->aux_param;
    }

    WS2812FX_setPixelColor(anim_led_index_of(led_pos),
                           anim_pick_color(color_index));

    return anim_frame_interval_ms();
}

/*
	星云：按灯带长度同时跑若干个「星空团」（自行设计）
	================================================================================
	- 一个「星空团」= 一小块灯珠：位置随机、颜色随机（非黑），
	  块大小 = app 下发的「多少个灯为一组」，上限是灯带长度的一半；
	- 每个团的亮度按 0 -> 最亮 -> 0 的三角包络呼吸，亮完一轮就换个位置、
	  换个颜色重新亮，所以整条灯带上是「几个颜色不同的亮团此起彼伏」；
	- 同时亮的团数 = 灯带能放下几个块（灯带越长，同时闪的星空团越多），
	  最多 ANIM_NEBULA_MAX 个；
	- 团内不是整个块一个亮度，而是中间最亮、往两侧递减，看起来才像一团云；
	- 每帧都先把整条灯带重新铺回底色，不会像渐灭类动画那样在中间积出一块固定颜色。
	================================================================================
*/
#define ANIM_NEBULA_MAX          8
#define ANIM_NEBULA_ENVELOPE_MAX 510 // 包络 0 -> 255 -> 0，共 510 步
#define ANIM_NEBULA_STEP_MIN     12  // 每帧包络步进（越小闪得越慢）
#define ANIM_NEBULA_STEP_RANGE   24  // 步进的随机范围

static u16 anim_nebula_envelope[ANIM_NEBULA_MAX]; // 亮度包络 0 ~ 510
static u16 anim_nebula_pos[ANIM_NEBULA_MAX];      // 星空团的起始灯珠
static u8 anim_nebula_color[ANIM_NEBULA_MAX];     // 颜色下标
static u8 anim_nebula_step[ANIM_NEBULA_MAX];      // 每帧的包络步进
static u8 anim_nebula_count;                      // 当前星空团数量
static u8 anim_nebula_inited;

// 让一个星空团在随机位置、随机颜色重新开始呼吸
static void anim_nebula_spawn(u8 cluster_index, u16 block_len)
{
    anim_nebula_pos[cluster_index] =
        (u16)WS2812FX_random16_lim((u16)(_seg_len - block_len + 1));
    anim_nebula_color[cluster_index] = anim_random_color_index();
    anim_nebula_step[cluster_index] =
        (u8)(ANIM_NEBULA_STEP_MIN +
             WS2812FX_random16_lim(ANIM_NEBULA_STEP_RANGE));
    anim_nebula_envelope[cluster_index] = 0;
}

u16 led_strip_rgb_scene_anim_nebula(void)
{
    u16 block_len = anim_leds_per_seg(); // 星空团大小（多少个灯为一组）
    u32 background_color = anim_background_color(); // app 下发的底色
    u8 cluster_count;                               // 同时闪烁的星空团数量
    u8 cluster_index;                               // 当前正在处理第几个星空团
    u16 envelope;     // 该团的亮度包络 0 ~ 510（0~255 亮起，255~510 灭下）
    u16 level;        // 由包络换算出的亮度等级 0 ~ 255
    u8 color_index;   // 该团用的颜色下标
    u16 cluster_pos;  // 该团的起始灯珠位置
    u16 offset;       // 团内第几个灯（0 = 团头）
    u16 led_pos;      // 该灯的逻辑位置
    u16 twice_offset; // offset * 2（算离团中心多远用）
    u16 center;       // 团中心对应的 2 倍偏移
    u16 distance;     // 该灯离团中心的距离（2 倍偏移）
    u16 weight;       // 该灯的亮度权重（中间最亮、两侧递减）
    u8 led_level;     // 该灯最终的亮度等级

    if (0 == _seg_len) {
        return anim_frame_interval_ms();
    }

    // 团大小上限 = 灯带长度的一半
    if (block_len > (u16)(_seg_len / 2)) {
        block_len = (u16)(_seg_len / 2);
    }
    if (0 == block_len) {
        block_len = 1;
    }

    // 同时亮的团数 = 灯带能放下几个块（灯带越长，星空团越多）
    cluster_count = (u8)(_seg_len / block_len);
    if (cluster_count > ANIM_NEBULA_MAX) {
        cluster_count = ANIM_NEBULA_MAX;
    }
    if (0 == cluster_count) {
        cluster_count = 1;
    }

    // 刚进入星云（或团大小/灯带长度变了）：重新铺一遍星空团
    if ((0 == anim_nebula_inited) || (anim_nebula_count != cluster_count)) {
        anim_nebula_inited = 1;
        anim_nebula_count = cluster_count;
        for (cluster_index = 0; cluster_index < cluster_count;
             cluster_index++) {
            anim_nebula_spawn(cluster_index, block_len);
            // 起始包络错开，几个团不会一起亮、一起灭
            anim_nebula_envelope[cluster_index] =
                (u16)WS2812FX_random16_lim(ANIM_NEBULA_ENVELOPE_MAX);
        }
    }

    // 整条灯带先回到底色，再把各个星空团按亮度画上去
    anim_fill_strip(background_color);

    for (cluster_index = 0; cluster_index < cluster_count; cluster_index++) {
        envelope = anim_nebula_envelope[cluster_index];
        level = (envelope > 255) ? (u16)(ANIM_NEBULA_ENVELOPE_MAX - envelope)
                                 : envelope;
        color_index = anim_nebula_color[cluster_index];
        cluster_pos = anim_nebula_pos[cluster_index];

        if (0 == level) {
            continue; // 这个团正好灭着
        }

        for (offset = 0; offset < block_len; offset++) {
            led_pos = (u16)(cluster_pos + offset);
            twice_offset = (u16)(offset * 2);
            center = (u16)(block_len - 1);
            distance = (twice_offset > center) ? (u16)(twice_offset - center)
                                               : (u16)(center - twice_offset);
            // 中间最亮、两侧递减，看起来才像一团云
            weight = (u16)(255 - ((u32)distance * 255) / block_len);
            led_level = (u8)((u32)level * weight / 255);

            if ((led_pos >= _seg_len) || (0 == led_level)) {
                continue; // 团一部分在灯带外，或者这一刻刚亮/刚灭
            }
            WS2812FX_setPixelColor(
                anim_led_index_of(led_pos),
                WS2812FX_color_blend(background_color,
                                     anim_pick_color(color_index), led_level));
        }
    }

    // 包络往前走，走完一个「亮 -> 灭」就换个位置、换个颜色重新开始
    for (cluster_index = 0; cluster_index < cluster_count; cluster_index++) {
        anim_nebula_envelope[cluster_index] =
            (u16)(anim_nebula_envelope[cluster_index] +
                  anim_nebula_step[cluster_index]);
        if (anim_nebula_envelope[cluster_index] >= ANIM_NEBULA_ENVELOPE_MAX) {
            anim_nebula_spawn(cluster_index, block_len);
            SET_CYCLE;
        }
    }

    return anim_frame_interval_ms();
}

/*
	流星亮度百分比表（参考重构前的 led_strip_rgb_anim_background_meteor）
	----------------------------------------------------------------------------
	每帧只往头部写一个像素，亮度按这张表一档一档往下降：
	越晚写进去的越靠近头部、越亮，先写进去的已经被推到身后、更暗，
	拖尾自然就是「头部最亮、越往后越暗」。
*/
static const u8 anim_meteor_rate[] = {100, 70, 45, 30, 20, 15,
                                      10,  7,  5,  4,  3,  2};
#define ANIM_METEOR_RATE_MAX                                                   \
    (sizeof(anim_meteor_rate) / sizeof(anim_meteor_rate[0]))

static u8 anim_meteor_rate_index; // 流星当前写到亮度表的第几档

/*
	流星：参考重构前的 led_strip_rgb_anim_background_meteor
	================================================================================
	- 进入时先把整条灯带刷成底色；
	- 每帧整条平移一个灯，只往头部写一个像素，亮度依次取亮度表
	  （100% -> 70% -> 45% -> …… -> 2%）；
	- 亮度表的 12 档走完之后，头部写的就是底色了，流星尾部会顺着灯带排出去、
	  灯带回到全底色（旧动画也是这样收尾的），不会出现「只跑一轮、整条灯带一直亮着」；
	- 拖尾两端色见 anim_meteor_trail_colors()：普通底色是「流星色 -> 黑」，
	  亮底（白底）反过来是「黑 -> 白」，所以青底 / 粉底 / 白底上都能看清流星；
	- 一帧一步，跑完 灯带长度 - 1 + 12*2 帧算一轮，配色前进一位（多色流星）。
	================================================================================
*/
u16 led_strip_rgb_scene_anim_meteor(void)
{
    u8 color_index = (u8)_seg_rt->aux_param;         // 当前流星用的颜色下标
    u32 meteor_color = anim_pick_color(color_index); // 流星颜色
    anim_meteor_trail_t trail =
        anim_meteor_trail_colors(meteor_color);     // 拖尾两端色（和底色反差）
    u32 background_color = anim_background_color(); // app 下发的底色
    u16 frame_count = (u16)_seg_rt->counter_mode_step; // 这一轮已经跑了多少帧
    u16 cycle_len = // 一轮的总帧数（跑道 + 尾部排出去）
        (u16)(((_seg_len > 0) ? (_seg_len - 1) : 0) + ANIM_METEOR_RATE_MAX * 2);
    u32 color; // 这一帧要写到流星头部的颜色

    // 刚进入流星：先把上一轮动画残留的画面清掉，并铺上底色
    if (anim_take_first_frame()) {
        anim_fill_strip(background_color);
        anim_meteor_rate_index = 0;
        frame_count = 0;
    }

    anim_shift_by_one_led();

    if (anim_meteor_rate_index < ANIM_METEOR_RATE_MAX) {
        // 亮度表还没走完：头部写「拖尾末端色 -> 流星头部色」的混色
        color = WS2812FX_color_blend(
            trail.tail, trail.head,
            (u8)((u16)anim_meteor_rate[anim_meteor_rate_index] * 255 / 100));
        anim_meteor_rate_index++;
    } else {
        // 亮度表走完了：头部写底色，把流星尾部顺着灯带排出去
        color = background_color;
    }

    if (led_strip_rgb_scene_is_reverse()) {
        WS2812FX_setPixelColor(_seg->stop, color);
    } else {
        WS2812FX_setPixelColor(_seg->start, color);
    }

    // 一帧一步，跑完整条跑道 = 一轮
    frame_count++;
    if (frame_count >= cycle_len) {
        frame_count = 0;
        anim_meteor_rate_index = 0;
        _seg_rt->aux_param = (u8)((color_index + 1) % anim_color_count());
        SET_CYCLE;
    }
    _seg_rt->counter_mode_step = frame_count;

    return anim_frame_interval_ms();
}

/*
	流星雨的拖尾亮度表：直接沿用重构前 led_strip_rgb_anim_meteor_signle_color 的
	rate 表（按「离流星头部的距离」排列，头部 100%，尾部为 0 表示不点亮）。
*/
#define ANIM_METEOR_SHOWER_RATE_MAX 17
static const u8 anim_meteor_shower_rate[ANIM_METEOR_SHOWER_RATE_MAX] = {
    100, 75, 45, 30, 20, 15, 10, 7, 5, 3, 2, 0, 0, 0, 0, 0, 0};

/*
	流星雨的节奏：比流星动画更高（流星跑得更频繁）
	----------------------------------------------------------------------------
	帧间隔取流星动画的 1/ANIM_METEOR_SHOWER_FRAME_DIV；
	最快 10ms（见 WS2812FX_service 里的 SPEED_MIN）。
*/
#define ANIM_METEOR_SHOWER_FRAME_DIV    3
#define ANIM_METEOR_SHOWER_FRAME_MS_MIN 10

static u16 anim_meteor_shower_frame_ms(void)
{
    u16 frame_ms =
        anim_frame_interval_ms() /
        ANIM_METEOR_SHOWER_FRAME_DIV; // 流星雨的帧间隔（流星动画的 1/3）

    if (frame_ms < ANIM_METEOR_SHOWER_FRAME_MS_MIN) {
        frame_ms = ANIM_METEOR_SHOWER_FRAME_MS_MIN;
    }

    return frame_ms;
}

/* 流星雨：多颗流星（每颗一种颜色；最多支持到协议的颜色上限） */
#define ANIM_METEOR_MAX APP_MSG_COLOR_NUM_MAX
static u16
    anim_meteor_head_pos[ANIM_METEOR_MAX]; // 每颗流星头部的位置（可超出灯带）
static u8 anim_meteor_count;               // 当前同时在跑的流星颗数
static u8
    anim_meteor_color_index; // 一次只跑一颗时，当前跑的是颜色池里的第几个颜色
static u8 anim_meteor_inited;

/*
	流星雨：和流星动画同一套拖尾（亮度一档比一档暗、最后尾部顺着灯带排出去），
	          只是同时跑多段流星，并且跑得更频繁
	================================================================================
	- 每颗流星都是一条「头部最亮、越往后越暗」的拖尾，亮度查 anim_meteor_shower_rate[]；
	- 拖尾长度固定 = 灯带长度（和只有一个颜色时的拖尾完全一致）：
	  拖尾不能随颜色数量变短，否则颜色越多、流星看着跑得越快；
	- 跑道长度 = 灯带长度 + 拖尾长度；跑道放得下所有流星（每颗之间至少隔一个拖尾长度）
	  就几颗不同颜色的流星同时跑（最多 ANIM_METEOR_MAX = 协议的颜色数量上限）；
	  放不下就一次只跑一颗：等这一颗跑完整条跑道，再换下一个颜色接着跑，
	  所以 7 段、8 段不同颜色都能跑出来，而且速度不会因为颜色变多而变快；
	- 所有流星速度一致、初始位置在跑道上等分，几段始终错开；
	- 帧间隔比流星动画短（见 anim_meteor_shower_frame_ms），所以流星出现得更频繁；
	- 拖尾两端色也走 anim_meteor_trail_colors()（普通底「流星色 -> 黑」、亮底「黑 -> 白」），
	  每颗流星按**自己的颜色**算，几颗颜色不同也都有明显拖尾。
	================================================================================
*/
u16 led_strip_rgb_scene_anim_meteor_shower(void)
{
    u8 color_count = anim_color_count(); // 颜色数量（每颗流星一种颜色）
    u8 meteor_count;                     // 同时在跑的流星颗数
    u8 sequential;                       // 1 = 一次只跑一颗，颜色轮着来
    u16 tail_len;                        // 流星长度（拖尾长度）
    u16 track_len;                       // 跑道长度 = 灯带长度 + 流星长度
    u32 background_color;                // app 下发的底色（铺在流星之外）
    u8 meteor_index;                     // 当前处理第几颗流星
    u16 head_pos;                        // 这颗流星头部的位置（可超出灯带）
    u8 color_index;                      // 这颗流星用的颜色下标
    u32 meteor_color;                    // 这颗流星的颜色
    anim_meteor_trail_t trail;           // 这颗流星拖尾的两端色
    u16 tail_offset;                     // 拖尾上第几个灯（0 = 头部）
    u16 led_pos;                         // 该灯的逻辑位置
    u8 tail_rate;                        // 该灯对应的拖尾亮度百分比

    if (0 == _seg_len) {
        return anim_meteor_shower_frame_ms();
    }

    if (color_count > ANIM_METEOR_MAX) {
        color_count = ANIM_METEOR_MAX;
    }
    if (0 == color_count) {
        color_count = 1;
    }

    /*
		流星长度固定 = 灯带长度：拖尾必须和「只有一个颜色」时完全一致，
		不能随颜色数量变短（否则颜色越多，流星看着跑得越快）。
		灯带很长时不超过亮度表长度（17 档）。
	*/
    tail_len = _seg_len;
    if (tail_len > ANIM_METEOR_SHOWER_RATE_MAX) {
        tail_len = ANIM_METEOR_SHOWER_RATE_MAX;
    }
    track_len = (u16)(_seg_len + tail_len);

    /*
		跑道放得下所有流星（每颗之间至少隔一个拖尾长度）就同时跑，
		放不下就一次只跑一颗：等这一颗跑完整条跑道，再换下一个颜色接着跑。
		这样颜色变多时只是换颜色接着跑，速度不会跟着变快。
	*/
    if ((u32)color_count * tail_len <= (u32)track_len) {
        meteor_count = color_count;
        sequential = 0;
    } else {
        meteor_count = 1;
        sequential = 1;
    }
    // 跑道至少要有「每颗流星一个位置」那么长，流星多了才不会挤在同一格
    if (track_len < (u16)meteor_count) {
        track_len = (u16)meteor_count;
    }

    background_color = anim_background_color(); // app 下发的底色（铺在流星之外）

    // 初始位置在跑道上等分，几颗流星始终错开
    if ((0 == anim_meteor_inited) || (anim_meteor_count != meteor_count)) {
        anim_meteor_inited = 1;
        anim_meteor_count = meteor_count;
        for (meteor_index = 0; meteor_index < meteor_count; meteor_index++) {
            anim_meteor_head_pos[meteor_index] =
                (u16)((u32)track_len * meteor_index / meteor_count);
        }
    }

    anim_fill_strip(background_color);

    for (meteor_index = 0; meteor_index < meteor_count; meteor_index++) {
        head_pos = anim_meteor_head_pos[meteor_index];
        // 几颗一起跑时每颗一种颜色；一次只跑一颗时，跑的是「当前颜色」
        color_index = sequential ? anim_meteor_color_index : meteor_index;
        meteor_color = anim_pick_color(color_index);
        // 拖尾两端色按这颗流星自己的颜色算（见 anim_meteor_trail_colors）
        trail = anim_meteor_trail_colors(meteor_color);

        for (tail_offset = 0; tail_offset < tail_len; tail_offset++) {
            if (tail_offset > head_pos) {
                break; // 拖尾还没全部进入灯带
            }
            led_pos = (u16)(head_pos - tail_offset);
            if (led_pos >= _seg_len) {
                continue; // 这一段还在灯带外面
            }

            // 亮度查表：头部最亮，越往后越暗（表尾的 0 表示不点亮）
            tail_rate = anim_meteor_shower_rate[tail_offset];
            if (0 == tail_rate) {
                continue;
            }
            WS2812FX_setPixelColor(
                anim_led_index_of(led_pos),
                WS2812FX_color_blend(trail.tail, trail.head,
                                     (u8)((u16)tail_rate * 255 / 100)));
        }
    }

    /*
		推进：所有流星速度一致（一帧一个灯），跑到跑道尽头就从起点重新进来，
		这样几段流星的位置始终等分错开，不会跑着跑着挤成一团。
	*/
    for (meteor_index = 0; meteor_index < meteor_count; meteor_index++) {
        if (anim_meteor_head_pos[meteor_index] >= track_len) {
            // 这一颗跑完整条跑道了：从起点重新进来
            anim_meteor_head_pos[meteor_index] = 0;
            // 一次只跑一颗时，等这颗跑完再换下一个颜色接着跑
            if (sequential) {
                anim_meteor_color_index =
                    (u8)((anim_meteor_color_index + 1) % color_count);
            }
            SET_CYCLE;
        } else {
            anim_meteor_head_pos[meteor_index]++;
        }
    }

    return anim_meteor_shower_frame_ms();
}

/*
    开合：dir = 0 开幕，dir = 1 闭幕
    ================================================================================
    协议只有 dir 能区分开/闭，就按下面的效果来做：

        开幕（dir = 0）：从一端「流水」到另一端（逐个点亮），整条点亮后停一拍，
                         然后整条熄灭，换下一种颜色，再从同一端重新点亮，循环；
        闭幕（dir = 1）：整条点亮后从尾部（远离控制板那一端）往回逐渐熄灭，
                         全灭停一拍，换下一种颜色后重新整条点亮，循环。

    - 方向位(dir)在本模式下只用来区分开幕/闭幕，所以亮区按逻辑位置画
      （用 anim_led_index_of_dir(pos, 0)，不跟方向翻转）：
          开幕：亮区 = [0, progress)，从逻辑 0 端往另一端生长；
          闭幕：亮区 = [0, _seg_len - progress)，熄灭端在尾部。
    - 节奏见上面「开合动画的节奏」：一趟的时长只跟速度有关，
      帧间隔和每帧推进的灯数都由「一趟时长 / 灯带长度」换算，灯带长短都不会闪；
    - 停留时长 = 一趟时长（跟着灯带长度自适应）；
    - **不用 app 下发的「多少个灯为一组」**：亮区整条同色，跑完一轮换下一种颜色。
    ================================================================================
*/
u16 led_strip_rgb_scene_anim_open_close(void)
{
    u8 color_count = anim_color_count();     // 颜色数量（至少 1）
    u8 color_index = (u8)_seg_rt->aux_param; // 这一轮用的颜色下标
    u32 walk_ms =                            // 一趟（一端走到另一端）的目标时长
        anim_cycle_time_ms(ANIM_OPEN_CLOSE_WALK_SLOW_MS,
                           ANIM_OPEN_CLOSE_WALK_FAST_MS);
    u32 frames_max = // 一趟最多多少帧（再快就超过帧间隔下限了）
        walk_ms / ANIM_OPEN_CLOSE_FRAME_MS_MIN;
    u16 step;     // 每帧推进多少个灯（灯带很长时一帧多走几个）
    u16 frames;   // 一趟的帧数（= 全亮/全灭的停留帧数）
    u16 frame_ms; // 帧间隔（ms）= 一趟时长 / 一趟帧数
    u16 progress =
        (u16)_seg_rt->counter_mode_step; // 已经推进的灯数（0 ~ _seg_len）
    u16 lit_len;                         // 亮区长度（多少个灯亮着）
    u16 led_pos;                         // 逻辑灯位置（0 = 控制板接灯带那一端）
    u32 color = anim_pick_color(color_index); // 这一轮的颜色（整条同色）

    if (0 == frames_max) {
        frames_max = 1;
    }
    /*
        每帧推进几个灯：灯带短就是 1 个灯，灯带很长才多走几个（一趟最多 frames_max 帧），
        这样「一趟的时长」不会被灯带长度拖长，也不会因为帧数太少而变成闪烁。
    */
    step = (u16)((_seg_len + frames_max - 1) / frames_max);
    if (step < 1) {
        step = 1;
    }
    frames = (u16)((_seg_len + step - 1) / step);
    if (frames < 1) {
        frames = 1;
    }
    frame_ms = (u16)(walk_ms / frames);
    if (frame_ms < ANIM_OPEN_CLOSE_FRAME_MS_MIN) {
        frame_ms = ANIM_OPEN_CLOSE_FRAME_MS_MIN;
    }

    // 兜底：运行状态被外部改动过时先夹回合法范围
    if (progress > _seg_len) {
        progress = 0;
    }

    // 开幕：亮区从逻辑 0 端生长；闭幕：从尾部灭过来（已灭掉 progress 个灯）
    lit_len = led_strip_rgb_scene_is_reverse() ? (u16)(_seg_len - progress)
                                               : progress;

    anim_fill_strip(anim_background_color());

    for (led_pos = 0; led_pos < lit_len; led_pos++) {
        // 固定按逻辑位置画（dir 已用于区分开/闭，这里不再跟方向翻转）
        WS2812FX_setPixelColor(anim_led_index_of_dir(led_pos, 0), color);
    }

    /*
        推进：一趟走完（亮区满 / 亮区灭光）后停 frames 帧（= 一趟的时长），
        然后换下一种颜色、从头开始下一轮：
            开幕 -> progress 归 0 就是「整条熄灭」那一帧，下一帧重新点亮；
            闭幕 -> progress 归 0 就是「整条点亮」，下一帧重新从尾部熄灭。
    */
    if (progress < _seg_len) {
        progress = (u16)(progress + step);
        if (progress > _seg_len) {
            progress = _seg_len;
        }
    } else if (++_seg_rt->aux_param3 >= frames) {
        _seg_rt->aux_param3 = 0;
        progress = 0;
        _seg_rt->aux_param = (u8)((color_index + 1) % color_count);
        SET_CYCLE;
    }
    _seg_rt->counter_mode_step = progress;

    return frame_ms;
}

/*
    跑动内核（app 的「跑动」和内置的「跑动集合」共用）
    ================================================================================
    - 「点 + 间隔」为一组：一组 = 点宽个灯亮 + 同样多的底色灯
      （点宽见 anim_run_group_len()：byte7 = 0 时按灯带长度自动分组，
        本工程 6 灯 = 2 个灯一组，图案 ● ● ○ ○ ● ●）；
    - 点的颜色从颜色池里循环取，而且每跑完一圈颜色偏移 +1（见 anim_run()）：
          只有 1 种颜色 -> 单色点跑动（例如红色跑动）；
          有多种颜色 -> 多个颜色的点同时跑，且颜色按颜色池依次流动
                        （每跑完一圈换下一种颜色，贴近旧 multi_dot_running）；
    - 每帧整条灯带按「点 + 间隔」的图案重画，并把图案整体沿方向前进若干个灯，
      灯带很短（例如 6 个灯）时也能看清楚点在跑；
    - 状态：图案相位放在 _seg_rt->counter_mode_step，颜色偏移放在 _seg_rt->aux_param；
      「跑动集合」额外用 _seg_rt->aux_param 记住当前是第几个子动画。
    ================================================================================
*/
typedef struct
{
    const u32 *colors; // 颜色池（0xRRGGBB）
    u8 color_count;    // 颜色数量（至少 1）
    u8 reverse;        // 方向：0 正向，1 反向
    u16 dot_width;     // 点宽（一个点占多少个灯，至少 1）
    u8 color_offset;   // 点的颜色偏移（每跑完一圈 +1，颜色就依次流动起来）
} anim_run_cfg_t;

/*
    跑动的节奏（跑动 0x0C / 跑动集合 0x0D 共用）
    ================================================================================
    跑动是「图案沿灯带移动」，节奏不能只跟着 app 下发的帧间隔走，
    否则速度开到最快（10ms 一帧）时，6 个灯上「点 + 间隔」一个周期只有 2 帧 = 20ms，
    肉眼看就是常亮 / 闪：

        - 「跑完整条灯带」的时长由速度百分比在
          [ANIM_RUN_ROUND_SLOW_MS, ANIM_RUN_ROUND_FAST_MS] 之间插值；
        - 帧间隔 = 这个时长 / 跑完整条灯带的帧数：
              灯带短（本工程 6 灯）-> 每帧 1 个灯，帧间隔自然变大
                                    （6 灯 + 最快 = 83ms/格，看得清在跑）；
              灯带很长             -> 帧数受 ANIM_RUN_FRAME_MS_MIN(10ms) 限制，
                                    这时每帧多走几个灯，跑的时长仍然不变。

    调用者再把每帧步进夹到「一个点 + 间隔」以内（否则点会被整个跳过去）。
    ================================================================================
*/
#define ANIM_RUN_ROUND_SLOW_MS 2000 // 速度 0%：跑完整条灯带要 2000ms
#define ANIM_RUN_ROUND_FAST_MS 500  // 速度 100%：跑完整条灯带要 500ms
#define ANIM_RUN_FRAME_MS_MIN  10   // 帧间隔下限（WS2812FX 一帧最快 10ms）

typedef struct
{
    u16 step;     // 每帧推进多少个灯（图案相位步进）
    u16 frame_ms; // 帧间隔（ms）
} anim_run_timing_t;

// 按「跑完整条灯带的时长」算出每帧步进和帧间隔（跟灯带长度自适应）
static anim_run_timing_t anim_run_timing(void)
{
    u32 round_ms = // 跑完整条灯带的目标时长
        anim_cycle_time_ms(ANIM_RUN_ROUND_SLOW_MS, ANIM_RUN_ROUND_FAST_MS);
    u32 frames_max = // 跑完整条灯带最多多少帧（再快就超过帧间隔下限了）
        round_ms / ANIM_RUN_FRAME_MS_MIN;
    u16 step;                 // 每帧推进多少个灯
    u16 frames;               // 跑完整条灯带的帧数
    anim_run_timing_t timing; // 返回值

    if (0 == frames_max) {
        frames_max = 1;
    }
    step = (u16)((_seg_len + frames_max - 1) / frames_max);
    if (step < 1) {
        step = 1;
    }
    frames = (u16)((_seg_len + step - 1) / step);
    if (frames < 1) {
        frames = 1;
    }
    timing.step = step;
    timing.frame_ms = (u16)(round_ms / frames);
    if (timing.frame_ms < ANIM_RUN_FRAME_MS_MIN) {
        timing.frame_ms = ANIM_RUN_FRAME_MS_MIN;
    }

    return timing;
}

// 按图案相位画一帧跑动图案；phase 是图案已经走过的灯珠数
static void anim_run_draw(const anim_run_cfg_t *cfg, u16 phase)
{
    u16 dot_width = cfg->dot_width;    // 点宽（一个点占多少个灯）
    u8 color_count = cfg->color_count; // 颜色数量（至少 1）
    u16 unit;                          // 一个「点 + 间隔」的宽度（= 点宽 * 2）
    u32 pattern_base; // 一个完整配色循环的宽度（= 点宽 * 2 * 颜色数量）
    u32 background_color = anim_background_color(); // 底色
    u16 led_pos;                                    // 逻辑灯位置
    u32 color;                                      // 该灯这一帧的颜色
    u32 pattern_index;                              // 图案走到该灯时的下标
    u8 color_index; // 落在点上时用颜色池里的哪个颜色

    if (0 == _seg_len) {
        return;
    }

    if (0 == color_count) {
        color_count = 1;
    }

    unit = anim_run_unit_len(dot_width);
    pattern_base = (u32)color_count * unit;

    for (led_pos = 0; led_pos < _seg_len; led_pos++) {
        color = background_color;
        /*
            图案整体沿灯带前进 phase 个灯珠：
            先用一个完整配色循环的长度把下标抬到正数，避免出现负数下标
        */
        pattern_index =
            (u32)led_pos + pattern_base - (u32)(phase % pattern_base);

        if ((pattern_index % unit) < dot_width) {
            // 落在点上：第几个点（加上颜色偏移）决定用颜色池里的哪个颜色
            color_index =
                (u8)(((pattern_index / unit) + cfg->color_offset) % color_count);

            color = cfg->colors[color_index];
        }

        WS2812FX_setPixelColor(anim_led_index_of_dir(led_pos, cfg->reverse),
                               color);
    }
}

/*
    跑动（app 模式 0x0C）
    ================================================================================
    颜色池里的颜色形成「点」在底色上跑动：
        1 种颜色 -> 单色跑动；
        多种颜色 -> 多个颜色的点同时跑（红/绿/蓝跑动）。
    一个「点 + 间隔」的周期跑完就算一轮（SET_CYCLE）。
    节奏见上面「跑动的节奏」：帧间隔由速度换算，长短灯带都不会变成常亮 / 闪。
    ================================================================================
*/
u16 led_strip_rgb_scene_anim_run(void)
{
    static u32 run_colors
        [LED_STRIP_RGB_SCENE_COLOR_MAX]; // 本帧从颜色池拷出来的颜色（0x00RRGGBB）
    anim_run_cfg_t cfg;                  // 给 anim_run_draw() 的跑动参数
    anim_run_timing_t timing;            // 跑动节奏（每帧步进 / 帧间隔）
    u8 color_index;                              // 拷贝颜色池时的下标
    u16 unit;                                    // 一个「点 + 间隔」的宽度
    u16 phase = (u16)_seg_rt->counter_mode_step; // 图案已经跑过的灯珠数
    u8 color_count = anim_color_count();         // 颜色数量（至少 1）

    for (color_index = 0; color_index < color_count; color_index++) {
        run_colors[color_index] = anim_pick_color(color_index);
    }

    cfg.colors = run_colors;
    cfg.color_count = color_count;
    cfg.reverse = led_strip_rgb_scene_is_reverse();
    cfg.dot_width = anim_run_group_len(); // 一组多少个灯（点宽）
    // 点的颜色偏移：每跑完一圈 +1，颜色就依次流动起来（aux_param 存偏移）
    cfg.color_offset = (u8)_seg_rt->aux_param;

    if (0 == _seg_len) {
        return anim_frame_interval_ms();
    }

    unit = anim_run_unit_len(cfg.dot_width);
    timing = anim_run_timing();
    if (timing.step > unit) {
        // 一帧最多走一个「点 + 间隔」，否则点会被整个跳过去
        timing.step = unit;
    }
    // 兜底：运行状态被外部改动过时先夹回一个周期内
    if (phase >= unit) {
        phase = 0;
    }

    anim_run_draw(&cfg, phase);

    phase = (u16)(phase + timing.step);
    if (phase >= unit) {
        phase = (u16)(phase - unit);
        // 跑完一圈：点的颜色前进一位（颜色按颜色池依次流动）
        _seg_rt->aux_param = (u8)((cfg.color_offset + 1) % color_count);
        SET_CYCLE;
    }
    _seg_rt->counter_mode_step = phase;

    return timing.frame_ms;
}

/*
    跑动集合（app 模式 0x0D）
    ================================================================================
    需求：单独跑一种动画，循环执行 —— 由下面 20 个「跑动」子动画组成，
    每个子动画跑完一圈（图案沿灯带跑过整条灯带）就切到下一个，
    20 个全部跑完再从第一个重新开始，一直循环。

    子动画顺序：
        红色跑动、绿色跑动、蓝色跑动、黄色跑动、紫色跑动、青色跑动、白色跑动、
        红绿蓝跑动、紫青黄跑动、七彩跑动、
        反向七彩跑动、反向紫青黄跑动、反向红绿蓝跑动、反向白色跑动、
        反向青色跑动、反向紫色跑动、反向黄色跑动、反向蓝色跑动、
        反向绿色跑动、反向红色跑动

    配色和方向都写死在下表里（app 下发的「跑动集合」指令不带颜色），
    速度/亮度仍然跟着 app 下发的动画参数走。
    ================================================================================
*/

// 单色 / 组合色 调色板（0xRRGGBB）
static const u32 anim_run_color_red[] = {0x00FF0000};
static const u32 anim_run_color_green[] = {0x0000FF00};
static const u32 anim_run_color_blue[] = {0x000000FF};
static const u32 anim_run_color_yellow[] = {0x00FFFF00};
static const u32 anim_run_color_purple[] = {0x00FF00FF};
static const u32 anim_run_color_cyan[] = {0x0000FFFF};
static const u32 anim_run_color_white[] = {0x00FFFFFF};
static const u32 anim_run_color_rgb[] = {0x00FF0000, 0x0000FF00, 0x000000FF};
static const u32 anim_run_color_pcy[] = {0x00FF00FF, 0x0000FFFF, 0x00FFFF00};
static const u32 anim_run_color_rainbow[] = {
    0x00FF0000, 0x0000FF00, 0x000000FF, 0x00FFFF00,
    0x0000FFFF, 0x00FF00FF, 0x00FFFFFF,
};

#define ANIM_RUN_COLOR_NUM(color_array)                                        \
    ((u8)(sizeof(color_array) / sizeof((color_array)[0])))

typedef struct
{
    const u32 *colors; // 颜色池
    u8 color_count;    // 颜色数量
    u8 reverse;        // 方向：0 正向，1 反向
} anim_run_collection_entry_t;

static const anim_run_collection_entry_t anim_run_collection_table[] = {
    {anim_run_color_red, ANIM_RUN_COLOR_NUM(anim_run_color_red), 0}, // 红色跑动
    {anim_run_color_green, ANIM_RUN_COLOR_NUM(anim_run_color_green),
     0}, // 绿色跑动
    {anim_run_color_blue, ANIM_RUN_COLOR_NUM(anim_run_color_blue),
     0}, // 蓝色跑动
    {anim_run_color_yellow, ANIM_RUN_COLOR_NUM(anim_run_color_yellow),
     0}, // 黄色跑动
    {anim_run_color_purple, ANIM_RUN_COLOR_NUM(anim_run_color_purple),
     0}, // 紫色跑动
    {anim_run_color_cyan, ANIM_RUN_COLOR_NUM(anim_run_color_cyan),
     0}, // 青色跑动
    {anim_run_color_white, ANIM_RUN_COLOR_NUM(anim_run_color_white),
     0}, // 白色跑动
    {anim_run_color_rgb, ANIM_RUN_COLOR_NUM(anim_run_color_rgb),
     0}, // 红绿蓝跑动
    {anim_run_color_pcy, ANIM_RUN_COLOR_NUM(anim_run_color_pcy),
     0}, // 紫青黄跑动
    {anim_run_color_rainbow, ANIM_RUN_COLOR_NUM(anim_run_color_rainbow),
     0}, // 七彩跑动
    {anim_run_color_rainbow, ANIM_RUN_COLOR_NUM(anim_run_color_rainbow),
     1}, // 反向七彩跑动
    {anim_run_color_pcy, ANIM_RUN_COLOR_NUM(anim_run_color_pcy),
     1}, // 反向紫青黄跑动
    {anim_run_color_rgb, ANIM_RUN_COLOR_NUM(anim_run_color_rgb),
     1}, // 反向红绿蓝跑动
    {anim_run_color_white, ANIM_RUN_COLOR_NUM(anim_run_color_white),
     1}, // 反向白色跑动
    {anim_run_color_cyan, ANIM_RUN_COLOR_NUM(anim_run_color_cyan),
     1}, // 反向青色跑动
    {anim_run_color_purple, ANIM_RUN_COLOR_NUM(anim_run_color_purple),
     1}, // 反向紫色跑动
    {anim_run_color_yellow, ANIM_RUN_COLOR_NUM(anim_run_color_yellow),
     1}, // 反向黄色跑动
    {anim_run_color_blue, ANIM_RUN_COLOR_NUM(anim_run_color_blue),
     1}, // 反向蓝色跑动
    {anim_run_color_green, ANIM_RUN_COLOR_NUM(anim_run_color_green),
     1}, // 反向绿色跑动
    {anim_run_color_red, ANIM_RUN_COLOR_NUM(anim_run_color_red),
     1}, // 反向红色跑动
};

#define ANIM_RUN_COLLECTION_NUM                                                \
    (sizeof(anim_run_collection_table) / sizeof(anim_run_collection_table[0]))

/*
    跑动集合的点宽和跑动一致（一组 = 点宽个灯亮 + 同样多的底色灯，见 anim_run_group_len()），
    每个子动画都是「底色的底 + 几组点在跑」，不会变成整条灯带一起亮、一起灭。
*/

u16 led_strip_rgb_scene_anim_run_collection(void)
{
    u8 sub_index = (u8)_seg_rt->aux_param;    // 当前跑第几个子动画
    const anim_run_collection_entry_t *entry; // 该子动画的配色/方向表项
    anim_run_cfg_t cfg;                       // 给 anim_run_draw() 的跑动参数
    anim_run_timing_t timing;                 // 跑动节奏（每帧步进 / 帧间隔）
    u16 unit;                                 // 一个「点 + 间隔」的宽度
    u16 phase = (u16)_seg_rt->counter_mode_step; // 图案已经跑过的灯珠数

    if (sub_index >= ANIM_RUN_COLLECTION_NUM) {
        sub_index = 0;
    }
    entry = &anim_run_collection_table[sub_index];

    cfg.colors = entry->colors;
    cfg.color_count = entry->color_count;
    cfg.reverse = entry->reverse;
    cfg.dot_width = anim_run_group_len(); // 一组多少个灯（点宽，和跑动一致）
    cfg.color_offset = 0; // 配色固定（跑完一圈就换下一个子动画，不需要颜色流动）

    if (0 == _seg_len) {
        return anim_frame_interval_ms();
    }

    unit = anim_run_unit_len(cfg.dot_width);
    timing = anim_run_timing();
    if (timing.step > unit) {
        // 一帧最多走一个「点 + 间隔」，否则点会被整个跳过去
        timing.step = unit;
    }
    // 兜底：运行状态被外部改动过时先夹回整条灯带内
    if (phase >= _seg_len) {
        phase = 0;
    }

    anim_run_draw(&cfg, phase);

    phase = (u16)(phase + timing.step);
    if (phase >= _seg_len) {
        // 这个子动画跑完一圈（图案跑过整条灯带）：切到下一个子动画
        phase = 0;
        sub_index = (u8)((sub_index + 1) % ANIM_RUN_COLLECTION_NUM);
        _seg_rt->aux_param = sub_index;
        SET_CYCLE;
    }
    _seg_rt->counter_mode_step = phase;

    return timing.frame_ms;
}

/* ------------------------------------------------------------------------------ */
/*                                   声控动画                                      */
/* ------------------------------------------------------------------------------ */
/*
	声控动画（能量 / 节奏 / 频谱 / 滚动，对应 app 下发的声控模式索引 0 ~ 3）
	================================================================================
	1. 声控动画要跟得上声音，所以帧间隔固定成 ANIM_SOUND_FRAME_MS，不跟 app 下发的
	   动画速度走（声控模式下 app 给的速度常常是最慢，跟着走就变成一两秒才动一下，
	   完全看不出是声控）；个别要慢一点的动画（例如节奏）用自己的帧间隔，见各动画的说明；
	2. 声音信息来自声控模块（见 led_strip_voice.c）：
		 get_sound_triggered_by_led_strip_rgb()：这一帧有没有检测到声音
											   （读后自动清零，每帧只能读一次）
		 led_strip_voice_get_level()：           声音强度 0 ~ 100（快起慢落）
	3. 所有长度/位置都按当前段的真实长度 _seg_len 计算并夹紧，
	   灯串数量少的时候也不会画到段外面（即不会超过灯串数量）；
	   灯珠位置统一用 anim_led_index_of() 换算，方向由场景决定。
	================================================================================
*/

/*
	声控动画的配色：随机彩色（不读 app 的颜色池）
	================================================================================
	声控模式是「听着音乐看」的效果，用随机出来的彩色比 app 下发的固定颜色好看，
	所以这里约定：
		- 颜色不从 app 的颜色池里取，而是自己维护一张随机调色板；
		- 调色板里的颜色取自色环上的随机位置，相邻两种至少相差 ANIM_SOUND_HUE_GAP，
		  保证几种颜色分得开；
		- 调色板会隔一段时间重新随机一次（有声音时换得勤，没声音时换得慢），
		  同一帧内颜色是稳定的，所以看起来是「换色」而不是「闪」。
	================================================================================
*/
#define ANIM_SOUND_COLOR_NUM 3   // 随机调色板的颜色数量
#define ANIM_SOUND_HUE_GAP   85  // 相邻颜色在色环上的最小间隔（0 ~ 255）
#define ANIM_SOUND_HUE_EXTRA 171 // 在最小间隔之上再随机的范围（256 - HUE_GAP）

// 有声音 / 没有声音时，多少帧重新随机一次调色板
#define ANIM_SOUND_PALETTE_FAST_FRAMES 20
#define ANIM_SOUND_PALETTE_SLOW_FRAMES 100

static u32 anim_sound_palette[ANIM_SOUND_COLOR_NUM];
static u8 anim_sound_palette_frames; // 当前调色板已经用了多少帧

// 重新随机一张调色板
static void anim_sound_palette_refresh(void)
{
    u8 hue = WS2812FX_random8(); // 色环上的起始色相
    u8 color_index;              // 调色板里第几个颜色

    for (color_index = 0; color_index < ANIM_SOUND_COLOR_NUM; color_index++) {
        anim_sound_palette[color_index] = WS2812FX_color_wheel(hue);
        // 下一个颜色在色环上和当前颜色至少隔 ANIM_SOUND_HUE_GAP
        hue = (u8)(hue + ANIM_SOUND_HUE_GAP +
                   WS2812FX_random8_lim((u8)ANIM_SOUND_HUE_EXTRA));
    }
}

// 声控动画使用的颜色数量（随机调色板的颜色数，与 app 的颜色池无关）
static u8 anim_sound_color_count(void)
{
    return (u8)ANIM_SOUND_COLOR_NUM;
}

// 声控动画取色（随机调色板里的彩色，自动循环取色）
static u32 anim_sound_color(u16 color_index)
{
    return anim_sound_palette[color_index % ANIM_SOUND_COLOR_NUM];
}

// 声控动画的底色：固定黑色，不读 app 下发的底色（声控模式是独立的一套配色）
static u32 anim_sound_off_color(void)
{
    return (u32)BLACK;
}

/*
	每帧调用一次：过一段时间就重新随机一次调色板
	（有声音时换得勤一点，同一帧内不会重复随机，颜色是稳定的）
*/
static void anim_sound_palette_tick(u8 triggered)
{
    u8 refresh_frames =
        triggered
            ? (u8)ANIM_SOUND_PALETTE_FAST_FRAMES
            : (u8)ANIM_SOUND_PALETTE_SLOW_FRAMES; // 多少帧重新随机一次调色板

    if (++anim_sound_palette_frames >= refresh_frames) {
        anim_sound_palette_frames = 0;
        anim_sound_palette_refresh();
    }
}

/*
	声音强度 -> 0 ~ 255（满格校准）
	================================================================================
	led_strip_voice_get_level() 是「本次采集比长期平均值大多少」再乘灵敏度，
	实测把音量开到最大也只有 60 左右（不可能到 100），
	如果按 0 ~ 100 直接线性映射成 0 ~ 255：
		- 能量条最多只能亮到「灯带长度 * 60%」，最后几个灯永远点不亮；
		- 频谱的柱子也跳不高。
	所以这里约定：强度到 ANIM_SOUND_LEVEL_FULL(%) 就算满格。
	现场觉得太灵敏/太迟钝时，只调这一个宏即可（调大 = 更迟钝）。
	================================================================================
*/
#define ANIM_SOUND_LEVEL_FULL 60

static u8 anim_sound_level_255(void)
{
    u32 level = led_strip_voice_get_level();

    if (level >= ANIM_SOUND_LEVEL_FULL) {
        return 255;
    }

    return (u8)(level * 255 / ANIM_SOUND_LEVEL_FULL);
}

/*
	声控·能量：声音越大，从灯带头部亮起的「能量条」越长；声音停下后平滑回落
	================================================================================
	- 能量 = 声音强度（见 anim_sound_level_255()，满格校准过）换算成 0 ~ 255；
	- 一个灯满亮算 255 份能量，所以亮起的灯数 = 能量 * 灯带长度 / 255，
	  最后一个灯按剩余能量渐变 —— 灯带短（例如 6 个灯）时，
	  音量变化也能看出长度和边界亮度的变化；亮灯数不会超过灯带长度；
	- 颜色按「段」取色（多少个灯为一组），颜色池有多种颜色时能量条会有层次。
	================================================================================
*/
u16 led_strip_rgb_scene_anim_sound_energy(void)
{
    u8 leds_per_seg = anim_leds_per_seg(); // 每段多少个灯（能量条按段取色）
    u16 energy =
        (u16)_seg_rt->counter_mode_step;      // 当前能量 0 ~ 255（快起慢落）
    u16 target = (u16)anim_sound_level_255(); // 声音强度对应的目标能量 0 ~ 255
    u32 total;                                // 能量总量（一个灯满亮算 255 份）
    u16 full_leds;                            // 满亮的灯数
    u16 part_level;                           // 能量条边界那个灯的亮度
    u16 led_pos;    // 逻辑灯位置（0 = 控制板接灯带那一端）
    u8 color_index; // 该灯用的颜色下标（按段取色）
    u8 led_level;   // 该灯这一帧的亮度

    // 兜底：运行状态被外部改动过时先夹回合法范围
    if (energy > 255) {
        energy = 0;
    }

    // 随机调色板：有声音时换色勤一点（每帧只随机一次）
    anim_sound_palette_tick(get_sound_triggered_by_led_strip_rgb());

    // 起：立刻跟上声音（打击感）；落：每帧回落一部分（慢落）
    if (target > energy) {
        energy = target;
    } else {
        energy = (u16)(energy - (u16)((energy - target) / 4 + 1));
    }

    total = (u32)energy * _seg_len;
    full_leds = (u16)(total / 255);
    part_level = (u16)(total % 255);
    if (full_leds > _seg_len) {
        full_leds = _seg_len;
    }

    for (led_pos = 0; led_pos < _seg_len; led_pos++) {
        color_index = (u8)(led_pos / leds_per_seg);

        if (led_pos < full_leds) {
            led_level = 255; // 满亮
        } else if (led_pos == full_leds) {
            led_level = (u8)part_level; // 能量条的边界：按剩余能量渐变
        } else {
            led_level = 0; // 还没充到
        }

        WS2812FX_setPixelColor(
            anim_led_index_of(led_pos),
            WS2812FX_color_blend(anim_sound_off_color(),
                                 anim_sound_color(color_index), led_level));
    }

    _seg_rt->counter_mode_step = energy;

    return ANIM_SOUND_FRAME_MS;
}

/*
	声控·节奏：每检测到一次声音（一拍），灯光就从灯带一端「打」到另一端
	================================================================================
	- 拍到的那一帧从头部重新开始，之后每帧往前推进一点（见 ANIM_SOUND_RHYTHM_SPAN），
	  头部最亮，身后的灯按拖尾渐暗（拖尾长度 = 灯带长度的一半，至少 1 个灯）；
	- 灯光扫过整条灯带后就回到底色，等下一拍 ——
	  拍子感就是「亮 -> 灭」，灯珠数量和灯带长度一致，不会跑出灯带；
	- 每一拍换一种颜色（随机调色板轮流）；
	- 节奏自己的帧间隔是 ANIM_SOUND_RHYTHM_FRAME_MS（比其它声控动画慢），
	  灯光「打」过去才不会一闪而过；
	- 声音密的时候不能每一帧都重新起拍（不然灯光会一直停在起点、看起来像不亮），
	  所以两次起拍之间至少隔 ANIM_SOUND_RHYTHM_MIN_FRAMES 帧；
	- 完全没有声音时，隔 ANIM_SOUND_RHYTHM_IDLE_FRAMES 帧也会自己起一拍，
	  灯带不会一直黑着（只是比有声时慢很多）。

	节奏快慢就调下面三个宏：FRAME_MS 调大 = 整拍都变慢；SPAN 调大 = 一次「打」
	过去用的帧数变多（更慢）；MIN_FRAMES 调大 = 拍子之间拉得更开。
	================================================================================
*/
#define ANIM_SOUND_RHYTHM_FRAME_MS    40 // 节奏自己的帧间隔（ms），越大越慢
#define ANIM_SOUND_RHYTHM_SPAN        8  // 整条灯带分几帧打完（越大越慢）
#define ANIM_SOUND_RHYTHM_MIN_FRAMES  6  // 两拍之间至少隔几帧
#define ANIM_SOUND_RHYTHM_IDLE_FRAMES 15 // 没有声音时的自动起拍间隔（帧）

u16 led_strip_rgb_scene_anim_sound_rhythm(void)
{
    u8 color_count = anim_sound_color_count(); // 随机调色板的颜色数量
    u8 color_index = (u8)_seg_rt->aux_param;   // 当前这一拍用的颜色下标
    u16 trail_len = (_seg_len > 1) ? (u16)(_seg_len / 2)
                                   : 1; // 拖尾长度（灯带一半，至少 1）
    /*
		每帧往前推进多少：按灯带长度自适应（长灯带一帧多走几个灯，
		短灯带一帧就走 1 个灯，动作看得清楚、不会一闪而过）
	*/
    u16 step = (_seg_len >= ANIM_SOUND_RHYTHM_SPAN)
                   ? (u16)(_seg_len / ANIM_SOUND_RHYTHM_SPAN)
                   : 1; // 每帧往前推进的灯数
    u16 wave_end;       // 这一拍要打到的终点（灯带长度 + 拖尾长度）
    u16 wave_pos = (u16)_seg_rt->counter_mode_step; // 这一拍已经打到第几个灯
    u8 since_beat = (u8)_seg_rt->aux_param3;        // 距离上一次起拍过了几帧
    u8 triggered;                                   // 这一帧有没有检测到声音
    u16 led_pos;  // 逻辑灯位置（0 = 控制板接灯带那一端）
    u32 color;    // 该灯这一帧的颜色
    u16 distance; // 该灯离「拍到的地方」多远
    u8 level;     // 该灯的亮度（头部最亮，往后渐暗）

    if (0 == trail_len) {
        trail_len = 1;
    }
    if (0 == step) {
        step = 1;
    }
    wave_end = (u16)(_seg_len + trail_len);

    // 刚进入这个模式：先隔一拍，让第一帧就自己起一拍（马上就能看到效果）
    if (anim_take_first_frame()) {
        wave_pos = wave_end;
        since_beat = ANIM_SOUND_RHYTHM_IDLE_FRAMES;
    }
    if (since_beat < 255) {
        since_beat++;
    }

    triggered = get_sound_triggered_by_led_strip_rgb();
    anim_sound_palette_tick(triggered);

    /*
		起拍的条件：
			有声音，并且（上一拍已经打完，或者离上一拍够久了）；或者
			一直没有声音，但已经隔得够久了（自动起一拍，灯带不会一直黑着）
	*/
    if ((triggered && ((wave_pos >= wave_end) ||
                       (since_beat >= ANIM_SOUND_RHYTHM_MIN_FRAMES))) ||
        (since_beat >= ANIM_SOUND_RHYTHM_IDLE_FRAMES)) {
        wave_pos = 0;
        since_beat = 0;
        color_index = (u8)((color_index + 1) % color_count);
        _seg_rt->aux_param = color_index;
    }

    for (led_pos = 0; led_pos < _seg_len; led_pos++) {
        color = anim_sound_off_color();

        if (led_pos <= wave_pos) {
            distance = (u16)(wave_pos - led_pos); // 离「拍到的地方」多远

            if (distance < trail_len) {
                // 头部最亮，越往后越暗
                level = (u8)(255 - ((u32)distance * 255) / trail_len);
                color =
                    WS2812FX_color_blend(anim_sound_off_color(),
                                         anim_sound_color(color_index), level);
            }
        }
        WS2812FX_setPixelColor(anim_led_index_of(led_pos), color);
    }

    // 往前推进；整条灯带（含拖尾）跑完就停在末尾，等下一拍
    if (wave_pos < wave_end) {
        wave_pos = (u16)(wave_pos + step);
    }
    _seg_rt->counter_mode_step = wave_pos;
    _seg_rt->aux_param3 = since_beat;

    return ANIM_SOUND_RHYTHM_FRAME_MS;
}

/*
	声控·频谱：每颗灯当成一根「频谱柱」，柱高用亮度表示
	================================================================================
	- 柱高按「频段」分组：灯珠位置 0 ~ _seg_len-1 依次落在 ANIM_SOUND_SPECTRUM_BANDS
	  个频段上。灯带只有 6 个灯时，每颗灯就是一根独立的柱子；灯带更长时同一频段的灯
	  高低一致，看起来仍然像频谱；
	- 每次检测到声音，各个频段以这一次的声音强度为基准上下浮动取一个高度
	  （[强度/2, 强度] 之间随机），所以柱子有高有低，像频谱在跳；
	- 没有声音时每帧往下掉一点，掉到底就保留一点「底噪」亮度，灯带不会一片漆黑；
	- 颜色按「段」取色（多少个灯为一组），和其它效果保持一致。
	================================================================================
*/
#define ANIM_SOUND_SPECTRUM_BANDS 8  // 频段数量
#define ANIM_SOUND_SPECTRUM_DECAY 6  // 每帧下降的柱高
#define ANIM_SOUND_SPECTRUM_IDLE  12 // 没有声音时保留的底噪亮度

static u8 anim_spectrum_level[ANIM_SOUND_SPECTRUM_BANDS];

u16 led_strip_rgb_scene_anim_sound_spectrum(void)
{
    u8 leds_per_seg = anim_leds_per_seg(); // 每段多少个灯（柱子按段取色）
    u8 base = anim_sound_level_255(); // 这一次声音强度对应的 0 ~ 255 基准高度
    u8 band;                          // 当前处理第几个频段
    u16 led_pos;                      // 逻辑灯位置（0 = 控制板接灯带那一端）
    u8 triggered;                     // 这一帧有没有检测到声音
    u8 half;        // 基准高度的一半（柱子取 [half, base] 之间的随机高度）
    u8 level;       // 这次给频段取到的高度 0 ~ 255
    u8 band_index;  // 该灯属于第几个频段
    u8 color_index; // 该灯用的颜色下标（按段取色）
    u8 led_level;   // 该灯最终的亮度（柱高）

    triggered = get_sound_triggered_by_led_strip_rgb();
    anim_sound_palette_tick(triggered);

    // 有声音：各个频段重新取高度（以声音强度为基准上下浮动）
    if (triggered) {
        half = (u8)(base / 2);

        for (band = 0; band < ANIM_SOUND_SPECTRUM_BANDS; band++) {
            // [base/2, base] 之间随机：柱子有高有低
            level = (u8)(half + WS2812FX_random8_lim((u8)(half + 1)));

            if (level > anim_spectrum_level[band]) {
                anim_spectrum_level[band] = level;
            }
        }
    }

    // 每帧往下掉一点
    for (band = 0; band < ANIM_SOUND_SPECTRUM_BANDS; band++) {
        if (anim_spectrum_level[band] > ANIM_SOUND_SPECTRUM_DECAY) {
            anim_spectrum_level[band] -= ANIM_SOUND_SPECTRUM_DECAY;
        } else {
            anim_spectrum_level[band] = 0;
        }
    }

    for (led_pos = 0; led_pos < _seg_len; led_pos++) {
        band_index = (u8)(led_pos % ANIM_SOUND_SPECTRUM_BANDS);
        color_index = (u8)(led_pos / leds_per_seg);
        led_level = anim_spectrum_level[band_index];

        if (led_level < ANIM_SOUND_SPECTRUM_IDLE) {
            led_level = ANIM_SOUND_SPECTRUM_IDLE; // 底噪
        }

        WS2812FX_setPixelColor(
            anim_led_index_of(led_pos),
            WS2812FX_color_blend(anim_sound_off_color(),
                                 anim_sound_color(color_index), led_level));
    }

    return ANIM_SOUND_FRAME_MS;
}

/*
	声控·滚动：颜色块沿着灯带滚动，有声音时加速滚动一段时间
	================================================================================
	- 颜色块大小 = app 下发的「多少个灯为一组」（leds_per_seg，按灯带长度夹紧）；
	  如果一组只有 1 个灯（app 的默认值），色块滚动就退化成「每个灯都在变颜色」，
	  看起来就是不停闪烁，所以这时改成按颜色数量把灯带均分成几块
	  （例如 6 个灯 + 红绿蓝：每 2 个灯一块，共 3 块）；
	- 颜色块按颜色池循环取色；
	- 没有声音时很慢（每 ANIM_SOUND_SCROLL_SLOW_MS 滚一格）；检测到声音后
	  加快（每 ANIM_SOUND_SCROLL_FAST_MS 滚一格）并保持 ANIM_SOUND_SCROLL_BOOST_FRAMES 帧，
	  声音一停就慢慢降回原速；
	- 滚完「颜色数量 * 块大小」格算一轮；滚动方向由场景的方向决定。
	================================================================================
*/
#define ANIM_SOUND_SCROLL_SLOW_MS      120 // 没有声音：每 120ms 滚一格
#define ANIM_SOUND_SCROLL_FAST_MS      30  // 有声音：每 30ms 滚一格
#define ANIM_SOUND_SCROLL_BOOST_FRAMES 12  // 一次「一拍」最多加速这么多帧

u16 led_strip_rgb_scene_anim_sound_scroll(void)
{
    u8 color_count = anim_sound_color_count(); // 随机调色板的颜色数量
    u16 block_len = anim_leds_per_seg();       // 颜色块大小（多少个灯一块）
    u16 cycle_len; // 滚一轮共多少格（颜色数量 * 块大小）
    u16 flow = (u16)_seg_rt->counter_mode_step; // 已经滚了多少格
    u8 boost = (u8)_seg_rt->aux_param;          // 还剩几帧加速滚动
    u32 scroll;      // 这一帧实际的滚动量（反向时取反）
    u16 led_pos;     // 逻辑灯位置（0 = 控制板接灯带那一端）
    u8 triggered;    // 这一帧有没有检测到声音
    u16 auto_len;    // 一组只有 1 个灯时，按颜色数量自动分块
    u16 block_index; // 该灯落在第几个颜色块上

    // 一组只有 1 个灯时，按颜色数量均分，免得每个灯都在闪
    if (block_len < 2) {
        auto_len = (u16)(_seg_len / color_count); // 每种颜色分到几个灯

        if (auto_len > block_len) {
            block_len = auto_len;
        }
    }
    if (0 == block_len) {
        block_len = 1;
    }
    if (block_len > _seg_len) {
        block_len = (_seg_len > 0) ? (u16)_seg_len : 1;
    }

    cycle_len = (u16)((u16)color_count * block_len); // 滚一轮的格数
    // 兜底：运行状态被外部改动过时先夹回合法范围
    if (flow >= cycle_len) {
        flow = 0;
    }

    // 有声音：接下来一段时间加速滚动；没有声音就慢慢把加速用掉
    triggered = get_sound_triggered_by_led_strip_rgb();
    anim_sound_palette_tick(triggered);
    if (triggered) {
        boost = ANIM_SOUND_SCROLL_BOOST_FRAMES;
    } else if (boost > 0) {
        boost--;
    }

    // 反向：把滚动量反过来，色块就是朝另一个方向滚
    scroll =
        led_strip_rgb_scene_is_reverse() ? (u32)(cycle_len - flow) : (u32)flow;

    for (led_pos = 0; led_pos < _seg_len; led_pos++) {
        // 颜色块沿着灯带滚动：块序号 = (灯珠位置 + 滚动量) / 块大小
        block_index = (u16)(((u32)led_pos + scroll) / block_len);

        WS2812FX_setPixelColor(anim_led_index_of(led_pos),
                               anim_sound_color(block_index));
    }

    // 一次只滚一格：格数多了就变成整块颜色在跳，看起来就是闪烁
    flow++;
    if (flow >= cycle_len) {
        flow = (u16)(flow - cycle_len);
        SET_CYCLE; // 滚完一轮
    }
    _seg_rt->aux_param = boost;
    _seg_rt->counter_mode_step = flow;

    return (boost > 0) ? ANIM_SOUND_SCROLL_FAST_MS : ANIM_SOUND_SCROLL_SLOW_MS;
}

// 关灯动画：渐灭后熄灭
u16 led_strip_rgb_scene_anim_power_off(void)
{
    u16 fade_frames =
        (u16)_seg_rt->counter_mode_step; // 已经渐灭了多少帧（最多 50 帧）

    WS2812FX_fade_out();

    fade_frames++;
    if (fade_frames > 50) {
        // 渐变结束，保持熄灭
        fade_frames = 50;
        WS2812FX_strip_off();
        _seg_rt->counter_mode_step = fade_frames;
        return 500;
    }
    _seg_rt->counter_mode_step = fade_frames;

    return 30;
}

void led_strip_rgb_scene_anim_reset(void)
{
    memset(anim_meteor_head_pos, 0, sizeof(anim_meteor_head_pos));
    anim_meteor_count = 0;
    anim_meteor_color_index = 0;
    anim_meteor_inited = 0;

    // 切模式：下一帧算「第一帧」，并把星云/流星/频谱的内部状态复位
    anim_nebula_inited = 0;
    anim_meteor_rate_index = 0;
    anim_first_frame = 1;
    memset(anim_spectrum_level, 0, sizeof(anim_spectrum_level));

    // 声控动画用的是随机调色板：进来先随机一张，第一帧就是彩色的
    anim_sound_palette_frames = 0;
    anim_sound_palette_refresh();
}
