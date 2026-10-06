# 幻彩灯「场景」重构说明（led_strip_rgb_scene）

> 对应需求：`待修改功能.md`
> 重构目标：把 app 下发的动画指令（见 `与app通信使用到的指令.md`）与灯带底层解耦，
> 用一个新的、字段直观的结构体管理「模式 / 方向 / 亮度 / 速度 / 多少个灯为一组 / 底色 / 颜色」，
> 并重写动画实现，让这些参数都能随 app 指令变化。

---

## 1. 新增文件（旧文件不动）

| 文件 | 作用 |
| --- | --- |
| `apps/user_app/led_strip/led_strip_rgb_scene.h/.c` | 场景数据层：结构体定义、范围校验、参数换算、与旧 `fc_effect` 互相兼容 |
| `apps/user_app/led_strip/led_strip_rgb_scene_anim.h/.c` | **重构后的动画实现**（13 种 app 模式 + 4 种声控 + 关灯动画）<br>每个动画的参数 / 运行状态 / 实际效果见 [`子动画参数与效果说明.md`](./子动画参数与效果说明.md) |
| `apps/user_app/led_strip/led_strip_rgb_scene_schedule.h/.c` | 场景调度：`模式索引 -> 动画 + 段选项` 映射表、写 WS2812FX 段参数、上电启动、10ms 心跳 |

旧的 `led_strip_rgb_anim.c`、`led_strip_rgb_schedule.c`、`led_strip_rgb_app.c`、`fc_effect` **全部保留**，
只是 app 下发的动画指令不再走它们（旧动画「直接不使用」，需要改效果时改新文件即可）。

**被改动的旧文件（只加了少量接线代码）**

| 文件 | 改动 |
| --- | --- |
| `apps/user_app/protocol/app_msg_handle.c` | 动画指令 / 开关灯 / 静态色 / 亮度 / 速度指令改为调用场景层；同步指令改为上报「场景」的真实状态；**app 下发的动画参数统一存到全局变量 `app_msg_anim_info`**（解析 / 组装用模块级缓冲区，不占任务栈，写入有临界区保护）——见第 7 节 |
| `apps/user_app/protocol/app_msg_typedef.h` | 新增 `APP_MSG_MOD_IDX_RUN_COLLECTION`（0x0D 跑动集合） |
| `apps/user_app/user_main.c` | 上电调用 `led_strip_rgb_scene_start()`；10ms 心跳里调用 `led_strip_rgb_scene_tick_10ms()` |
| `apps/spp_and_le/board/bd19/AC632N_spp_and_le.cbp` | 把 3 个新 `.c` 加入 CodeBlocks 工程 |
| `apps/user_app/led_strip/led_strip_voice.h/.c` | 新增声音强度接口 `led_strip_voice_get_level()`（0 ~ 100，快起慢落），供声控动画使用；原有的声控触发标志不动 |
| `apps/user_app/led_strip/led_strip_rgb_schedule.c` | 声控模式的显示改为转发给场景层（`led_strip_rgb_scene_sound_apply()`），旧的 12 种声控动画不再使用 |

---

## 2. 数据流

```
app 指令（7F FF ...）
   │
   ▼
app_msg_handle.c        解析到「模块级缓冲区」→ 校验 → 发布到全局 app_msg_anim_info（临界区保护）
   │                    （app_msg_anim_info 就是其他模块直接读取的那份缓存，也是存 flash 的那份）
   ▼
led_strip_rgb_scene.c   校验 / 钳位 / 换算  ->  led_strip_rgb_scene（场景）
   │
   ▼
led_strip_rgb_scene_schedule.c   查模式表 -> 写 WS2812FX 段（效果函数/速度/方向/段选项）+ 全局亮度
   │
   ▼
led_strip_rgb_scene_anim.c       每 10ms 被 WS2812FX_service() 调用一次，只读场景
```

> 场景层只保存「已经过校验/换算」的参数，动画层再按**真实的段长度 `_seg_len`** 做最后一步夹紧
> （场景层拿不到真实灯数，不能在那里换算，见第 3 节「多少个灯为一组」）。

---

## 3. 场景结构体

```c
typedef struct
{
    u8 pwr_sta;         // 开关（DEVICE_ON / DEVICE_OFF）
    u8 mode_idx;        // 当前模式索引（APP_MSG_MOD_IDX_xxx）
    u8 dir;             // 动画方向：0 正向，1 反向
    u8 brightness;      // 动画亮度（百分比 0~100）
    u8 speed;           // 动画速度（百分比 0~100）
    u8 byte_reserved;   // 保留字节（app 协议 byte6，未使用）
    u8 leds_per_seg;    // 多少个灯为一组（app 协议 byte7）
    color_t background; // 动画底色（默认黑色）
    u8 color_num;       // 动画使用的颜色数量（不含底色）
    color_t colors[LED_STRIP_RGB_SCENE_COLOR_MAX];
} led_strip_rgb_scene_t;
```

参数换算规则（都在 `led_strip_rgb_scene.c` 里，改一处即可全局生效）：

| 参数 | 规则 |
| --- | --- |
| 亮度百分比 | `percent*(255-25)/100 + 25`，最低 25，避免 app 调到 0 之后灯带全黑；直接写 WS2812FX 全局亮度 |
| 速度百分比 | 帧间隔 `ms = 500 - 500*percent/100`，再钳到 `get_max_speed()`（`WS2812FX_service` 最快 10ms） |
| 方向 | 场景只存 0/1；段选项里叠加 `REVERSE`，动画内部再用 `anim_led_index_of()` 统一换算实际灯珠下标 |
| 流星拖尾对比 | 流星拖尾不再以底色为基准，而是 `anim_meteor_trail_colors()` 算出的「头 / 尾」两个极端色：**普通底色 = 头（流星色，太暗就用白）→ 尾（黑）**；**底色很亮（亮度 ≥ `ANIM_METEOR_BRIGHT_BG_LEVEL` 224，例如白底）= 反过来「黑头 → 白尾」**。这样青底 / 粉底上尾巴是黑的、一眼看得出流星，白底上则用黑头。app 下发的底色本身不改（只用来判断亮 / 暗） |
| 多少个灯为一组 | 直接用 app 下发的 byte7，**不再使用 options 里的 `SIZE_*`**。**场景层原样保存（不做任何夹紧）**，0 或「超过设备灯数」都表示「**所有灯为同一组**」，换算统一在动画层 `anim_leds_per_seg()`：`0` 或 `> _seg_len` → `_seg_len`，其余只做「≥ 1」保护。<br>配色类动画（渐变/跳变/呼吸/堆积/跑动/声控能量·频谱）按它分组、颜色池循环排色（不足一组的剩余灯珠接着下一组排色）；星云拿它当星空团大小（再夹到灯带一半）；**流水、流星雨、静态、星空、流星、开合不用它**；跑动另有点宽上限（见第 4 节）；**堆积是例外：`0` 按「1 个灯一组」处理（逐灯堆积）、`> 灯带长度` 才是整条一组**，见 `anim_accumulation_group_len()` |
| 保留字节 | app 协议 byte6，接收时忽略、上报时填 0，场景里只存一个 `byte_reserved` 占位 |
| 颜色池 | 上限 `APP_MSG_COLOR_NUM_MAX`（当前 8，与协议一致）；`fc_effect` 的颜色池也是 8，镜像时自动截断 |

### 命名约定（新增/修改动画时请遵守）

- **数量类参数沿用 app 协议的用词**：`byte_reserved`（byte6 保留）、`leds_per_seg`（byte7 多少个灯为一组）、
  `color_num`（颜色数量）、`speed` / `brightness`（百分比），这样和协议文档、`app_msg_anim_info_t` 一一对应，便于查协议；
- **位置、索引、状态、时间用描述性名字**：`led_pos`（逻辑位置）、`led_index`（实际灯珠下标）、
  `seg_index`（第几组）、`color_index` / `color_offset`（颜色下标 / 配色偏移）、`leds_per_seg`（多少个灯为一组）、
  `color_count`（颜色数量的可读叫法）、`head_pos`（头部位置）、`tail_len` / `tail_offset`（拖尾长度 / 拖尾第几个）、
  `lit_leds` / `lit_len`（已点亮灯数 / 亮区长度）、`breath_pos` / `breath_level`（呼吸进度 / 亮度等级）、
  `blend_pos`（渐变进度）、`frame_interval_ms`（帧间隔）、`cycle_time_ms`（动作周期时长）、`step_per_frame`（每帧步进）；
- `_seg_rt->counter_mode_step / aux_param / aux_param3` 是 WS2812FX 的运行状态，不要用 `aux_param2`（被 FRAME/CYCLE 标志占用）。

### app 协议最新字段定义（byte6 / byte7）

`与app通信使用到的指令.md` 已更新为：

| 字节 | 含义 |
| --- | --- |
| byte 6 | **保留**，app 尚未使用该功能（接收时不处理，上报时填 0） |
| byte 7 | 协议里叫「段数量」，**实际含义是「多少个灯为一组」**；`0x00` 或超过设备灯数都表示「**所有灯为同一组**」 |

对应处理：

| 位置 | 做法 |
| --- | --- |
| `app_msg_typedef.h` | 字段为 `byte_reserved`（byte6，保留）+ `leds_per_seg`（byte7，多少个灯为一组） |
| 解析（RX） | `byte_reserved = payload[6]`（不处理）、`leds_per_seg = payload[7]` |
| 上报（TX） | `user_ble_notify_anim_info()` 按结构体内存原样发出：byte6 填 0、byte7 回填「多少个灯为一组」 |
| 场景层 | `led_strip_rgb_scene.leds_per_seg = info->leds_per_seg`（多少个灯为一组）；`byte_reserved` 只存 0 |

---

## 4. 模式映射表

在 `led_strip_rgb_scene_schedule.c`：

```c
static const led_strip_rgb_scene_mode_entry_t led_strip_rgb_scene_mode_table[] = {
    {APP_MSG_MOD_IDX_STATIC,      led_strip_rgb_scene_anim_static,        NO_OPTIONS},
    {APP_MSG_MOD_IDX_GRADUAL,     led_strip_rgb_scene_anim_gradual,       NO_OPTIONS},
    ...
};
```

| app 模式 | 动画实现 | 说明 |
| --- | --- | --- |
| 静态色 0x01 | `anim_static` | 第一个颜色铺满 |
| 渐变 0x02 | `anim_gradual` | 参考旧 `led_strip_rgb_anim_mutil_fade`：**按段在相邻两色之间平滑渐变**，进度满 100% 时配色前进一位（无缝衔接）；帧间隔固定 `ANIM_FRAME_MS`，一轮时长由 `ANIM_GRADUAL_ROUND_*_MS` 限制；**颜色池只有一种颜色时由代码补上「另一种颜色」**：本色不是黑色补 `ANIM_GRADUAL_BLACK`（「本色 ↔ 黑」，熄灭式呼吸），本色是黑色补 `ANIM_GRADUAL_WHITE`（「黑 ↔ 白」），两轮起点/终点互换，依然是无缝、不跳色 |
| 跳变 0x03 | `anim_jump` | 按「段」跳变，**每轮都会换色**（每 4 帧配色偏移 +1，同一个灯的颜色不断轮换）；段大小凑不满整条灯带时，剩余灯珠接着下一段继续排色 |
| 呼吸 0x04 | `anim_breath` | **每个「段」用颜色池里的一种颜色呼吸，配色按位置固定、每轮结束不换色**；段大小=1 且多色时就是「每隔一个灯用一种颜色」；段大小凑不满整条灯带时，剩下的灯珠接着下一段排色（如 6 灯 / 段大小 5 → 第 6 个灯用第二种颜色） |
| 流水 0x05 | `anim_running_water` | 相邻两色混色拼成一条**颜色循环**，再按**灯串长度均分**到每个灯上（如 6 灯 + 红蓝两色：红-红蓝-蓝-蓝红-红蓝-蓝）；分段只由颜色数量决定，**app 下发的「多少个灯为一组」不参与**；相位随流动递减，所以是朝灯带末端流（方向修正过） |
| 堆积 0x06 | `anim_accumulation` | 逐灯点亮。**byte7 用原值**（`anim_accumulation_group_len()`）：`0` = 1 个灯一组（逐灯堆积，否则整条一起亮灭、看着像闪）、`≥ 灯带长度` = 整条一组。<br>**节奏：点亮整条灯带的时长只由速度决定**（`ANIM_ACCUM_ROUND_SLOW_MS` 3000ms ~ `..._FAST_MS` 900ms），帧间隔 = 时长 / 步数（≥ 10ms，6 灯最快 = 150ms/组），点满后停一拍（= 一个时长）再换色 |
| 星空 0x07 | `anim_starry_sky` | 随机单灯闪（`FADE_XSLOW` 余晖）；**刚进入时先清屏，不会残留上一个动画的颜色** |
| 星云 0x08 | `anim_nebula` | **按灯带长度同时跑若干个「星空团」**（灯带越长越多，最多 4 个）：每个团位置随机、颜色随机（非黑），亮度按 `0 -> 最亮 -> 0` 的三角包络呼吸，亮完一轮就换个位置、换个颜色重新亮；团内中间最亮、两侧递减；每帧整条重铺底色，不会在灯带中间积出一块固定颜色。块大小 = 「多少个灯为一组」，上限灯带长度的一半 |
| 流星 0x09 | `anim_meteor` | 参考旧 `led_strip_rgb_anim_background_meteor`：进入时先整条刷底色，之后每帧平移一个灯、只往头部写一个像素，亮度依次取 `anim_meteor_rate[]`（100/70/45/30/20/15/10/7/5/4/3/2%）；**12 档走完后头部写底色**，尾部顺灯带排出去（不会「只跑一轮、整条灯带一直亮」）；一轮 = `灯带长度-1 + 12*2` 帧。<br>**拖尾对比**（`anim_meteor_trail_colors()`）：普通底色是「流星色 → 黑」，**亮底（白底）反过来是「黑 → 白」**，所以青底 / 粉底 / 白底上流星都看得清 |
| 流星雨 0x0A | `anim_meteor_shower` | **与流星动画同一套拖尾**（亮度一档比一档暗、尾部顺着灯带排出去）；**拖尾长度固定 = 灯带长度**（和只有一个颜色时完全一致），所以**颜色变多不会让流星变快**；跑道 = 灯带长度 + 拖尾长度，放得下所有流星（每颗之间至少隔一个拖尾长度）就几颗不同颜色同时跑（上限 = 协议颜色数量上限 8），放不下就**一次只跑一颗、等这颗跑完整条跑道再换下一个颜色接着跑**（7 段以上不同颜色都能跑到）；所有流星等速、初始位置等分；**帧间隔 = 流星动画的 1/3**（最快 10ms）；每颗流星按**自己的颜色**算拖尾端点色（青底 / 粉底上尾巴是黑的、看得清） |
| 开合 0x0B | `anim_open_close` | **dir=0 开幕，dir=1 闭幕**（协议只有 dir 能区分开/闭）。**不按「多少个灯一组」**，整条同色：<br>**开幕** = 从逻辑 0 端流水式逐个点亮到另一端（全灭→全亮），全亮停一拍→**整条熄灭**→换色后从同一端重新点亮；<br>**闭幕** = 整条点亮→**从尾部往回**逐渐熄灭（全亮→全灭），全灭停一拍→换色后重新整条点亮。<br>**节奏：一趟的时长只由速度决定**（`ANIM_OPEN_CLOSE_WALK_SLOW_MS` 2400ms ~ `..._WALK_FAST_MS` 800ms），帧间隔 = 一趟时长 / 一趟帧数（每帧推进 `ceil(灯带长度 / frames_max)` 个灯，帧间隔下限 10ms）→ 灯带长短都不会闪；颜色每轮用颜色池里的下一种 |
| 跑动 0x0C | `anim_run` | 「点 + 底色间隔」交替的图案沿灯带跑动；点的颜色按颜色池循环取 → **多色就是多个不同颜色的点同时跑**（贴近旧 `led_strip_rgb_anim_multi_dot_running`）。<br>**「点 + 间隔」为一组：一组 = 点宽个灯亮 + 同样多的底色灯**（`anim_run_group_len()`：**byte7 = 0 → 按灯带长度自动分组**，点宽 = 灯带长度 / `ANIM_RUN_GROUP_DIV`(3)、最少 1 个、最多 灯带长度/2；byte7 非 0 就直接用它。6 灯 = 2 个灯一组 → 图案 `●●○○●●`，12 灯 = 4 个灯一组）；**每跑完一圈颜色偏移 +1，点的颜色按颜色池依次流动**。<br>**节奏：跑完整条灯带的时长只由速度决定**（`ANIM_RUN_ROUND_SLOW_MS` 2000ms ~ `..._FAST_MS` 500ms），帧间隔 = 时长/帧数（每帧推进 `ceil(灯带长度/frames_max)` 个灯，再夹到 ≤ unit = 点宽 * 2，帧间隔下限 10ms）→ 6 灯最快 83ms/格（不再常亮 / 闪），灯带长短都一致 |
| 跑动集合 0x0D | `anim_run_collection` | **单独跑一种动画并循环执行**：内置 20 个「跑动」子动画（红/绿/蓝/黄/紫/青/白、红绿蓝、紫青黄、七彩 + 它们的反向版本；配色与方向写死在表里，不读 app 的颜色池），**每个子动画跑完一圈（图案跑过整条灯带）就切到下一个**，20 个跑完回到第 1 个。<br>点宽跟着灯带长度走（= 跑动允许的最大点宽，6 灯 = 1）；速度/亮度仍用 app 下发的值；**节奏和跑动同一套**（每个子动画跑完整条灯带用 `ANIM_RUN_ROUND_*_MS`：最快 0.5s、最慢 2s，不再是最快 60ms 切一个） |

### 声控模式（app 的声控模式索引 `fc_effect.music.m`）

原来的 12 种声控动画（`led_strip_rgb_anim_sound_control_*`）不符合要求（没有考虑灯串数量），
已按「能量 / 节奏 / 频谱 / 滚动」四个主题重新设计，实现在 `led_strip_rgb_scene_anim.c`：

```c
static const mode_ptr led_strip_rgb_scene_sound_mode_table[] = {
    led_strip_rgb_scene_anim_sound_energy,   // 0：能量
    led_strip_rgb_scene_anim_sound_rhythm,   // 1：节奏
    led_strip_rgb_scene_anim_sound_spectrum, // 2：频谱
    led_strip_rgb_scene_anim_sound_scroll,   // 3：滚动
};
```

| 索引 | 动画实现 | 说明 |
| --- | --- | --- |
| 0 能量 | `anim_sound_energy` | 声音越大，从灯带头部亮起的「能量条」越长（一个灯满亮算 255 份能量，边界那个灯按剩余能量渐变），声音停下后平滑回落；颜色用随机调色板的彩色（按「段」取色，多种颜色时能量条有层次） |
| 1 节奏 | `anim_sound_rhythm` | 每检测到一次声音（一拍），灯光从头到尾「打」过去：头部最亮、身后按拖尾渐暗（拖尾 = 灯带长度的一半），扫完回到底色等下一拍；每拍换一种颜色。**自己的帧间隔 `ANIM_SOUND_RHYTHM_FRAME_MS`(40ms，比其它声控慢)**，每帧推进 `_seg_len / ANIM_SOUND_RHYTHM_SPAN`(8) 个灯（短灯带就是 1 个灯）；两拍之间至少隔 `ANIM_SOUND_RHYTHM_MIN_FRAMES`(6 帧)（否则声音密时灯光会停在起点）；没声音时隔 `ANIM_SOUND_RHYTHM_IDLE_FRAMES`(15 帧) 自动起一拍，灯带不会一直黑着 |
| 2 频谱 | `anim_sound_spectrum` | 每颗灯当成一根「频谱柱」（柱高 = 亮度），灯珠位置依次落在 8 个「频段」上；每次声音让各频段在 [强度/2, 强度] 之间随机取高，再逐帧回落，并有底噪亮度 |
| 3 滚动 | `anim_sound_scroll` | 颜色块沿灯带滚动（**一次只滚一格**）：没声音时每 120ms 一格，检测到声音后每 30ms 一格并保持 12 帧，声音一停就降回原速。**块大小**：app 下发的「多少个灯为一组」≥ 2 就用它；等于 app 默认的 1 时改成按颜色数量均分（如 6 灯 + 红/绿/蓝 = 每 2 灯一块），否则每个灯都在变颜色，看起来就是闪烁 |

声控动画的公共约定：

- **帧间隔固定 = `ANIM_SOUND_FRAME_MS`(20ms)**，不跟 app 下发的动画速度走：
  声控要跟得上声音，跟着「往往是最慢」的动画速度走就变成一两秒才动一下；
  例外：节奏用自己的 `ANIM_SOUND_RHYTHM_FRAME_MS`(40ms)，整拍才不会一闪而过；
- **所有长度/位置都按当前段的真实长度 `_seg_len` 计算并夹紧**，灯串数量少（例如 6 个灯）时也不会画到段外面，即「不会超过灯串数量」；
- 声音信息来自声控模块（`led_strip_voice.c`）：
  - `led_strip_voice_get_level()`：声音强度 0 ~ 100，快起慢落（能量 / 频谱用）；
  - `get_sound_triggered_by_led_strip_rgb()`：这一帧有没有声音（读后自动清零，**每帧只能读一次**，节奏 / 滚动用）；
  - 两者在 `sound_handle()` 里是**同一个 `adc_percent`** 算出来的：
    强度达到 `SOUND_BEAT_PERCENT`(15) 就置一次「一拍」。
    ⚠ 原来「一拍」的判定用的是白光流星灯的灵敏度 `led_strip_white.sensitivity`，
    而 `led_strip_white_schedule_init()` 在本工程里没有被调用，它一直是 0
    → 触发标志永远不会置起来 → 靠「一拍」驱动的效果（节奏、频谱）一直没有反应。
- **满格校准**：实测满音量时声音强度只有 60 左右（到不了 100），
  所以用 `ANIM_SOUND_LEVEL_FULL`(60) 当作「满格」再换算成 0 ~ 255，
  否则能量条永远点不亮最后几个灯、频谱也跳不高（调大 = 更迟钝）；
- **配色：完全不用 app 的颜色池和底色**，而是自己维护一张**随机彩色调色板**
  （`ANIM_SOUND_COLOR_NUM` = 3 种）：颜色取自色环上的随机位置，相邻两种至少隔
  `ANIM_SOUND_HUE_GAP`(85)，保证几种颜色分得开；调色板每
  `ANIM_SOUND_PALETTE_FAST_FRAMES`(20 帧，有声音时) / `ANIM_SOUND_PALETTE_SLOW_FRAMES`(100 帧，没声音时)
  重新随机一次 —— 同一帧内颜色是稳定的，所以看起来是「换色」而不是「闪」；
  「暗」的状态固定用黑色（`anim_sound_off_color()`），所以切到声控模式后不受上一条动画指令下发的颜色/底色影响；
- 声控模式索引越界（≥ 4）时退化为第一种（能量），不会让灯带黑着。

### 如何新增 / 修改一个动画

1. 在 `led_strip_rgb_scene_anim.c` 里写一个 `u16 xxx(void)`，返回值是「下一帧间隔(ms)」；
2. 参数只从 `led_strip_rgb_scene_*` 读取，状态只放 `_seg_rt->counter_mode_step / aux_param / aux_param3`；
3. 在模式表里加一行即可（不用改其它任何地方）。

声控动画同理：加在 `led_strip_rgb_scene_sound_mode_table[]` 里，并同步 `led_strip_sys.c` 的
`MAX_MUSIC_EFFECT_NUMBER`（当前 `TYPE_Fiber_optic_lights` 为 4）。

### 动画节奏（速度）配置

`led_strip_rgb_scene_anim.c` 顶部集中放了节奏宏，调手感只改这里：

```c
#define ANIM_FRAME_MS            20    // 每帧间隔(ms)：越小越平滑
#define ANIM_WATER_ROUND_SLOW_MS 6000  // 流水：速度 0% 时一轮(颜色循环走一圈)的时长
#define ANIM_WATER_ROUND_FAST_MS 1500  // 流水：速度 100% 时一轮的时长
#define ANIM_GRADUAL_ROUND_SLOW_MS 6000 // 渐变：速度 0% 时一轮的时长
#define ANIM_GRADUAL_ROUND_FAST_MS 1200 // 渐变：速度 100% 时一轮的时长
```

思路：**帧间隔固定成小值**（保证平滑），速度百分比只决定「一轮时长」，
再由一轮的总步数换算出每帧步进（`anim_step_of_round()`）。
这样就不会出现「帧间隔太大 → 一帧一帧跳」，也不会「一帧挪一个灯 → 快得看不清」。

---

## 5. 与旧代码的兼容策略

保留 `fc_effect` 作为「旧通道的公共状态」，双向同步，避免遥控器 / DP 协议失效：

- 场景更新时会 `scene_mirror_to_fc_effect()`：把开关、亮度、速度、方向、多少个灯为一组、颜色池同步给 `fc_effect`，
  这样 BLE 状态上报、flash 保存、`dp_data_tran.c` 等旧代码读到的值都是一致的。
- `led_strip_rgb_scene_tick_10ms()` 反向同步：旧通道改了 `fc_effect.app_b / app_speed / on_off_flag` 时，
  自动折算进场景（亮度/速度只改参数，不重启动画）。
- 亮度/速度在上电时以 `fc_effect`（有独立 flash 保存通道）恢复出来的值为准。

**已知边界**：旧通道「切换模式」（`fc_effect.dream_scene.change_type` + `led_strip_rgb_schedule()`，
例如遥控器的模式键、DP 协议的炫彩情景）仍然由旧调度接管显示，
此时 app 的场景不会自动抢回（避免两套调度互相打架）。如果需要旧通道切模式也走新场景，
把对应位置的 `led_strip_rgb_schedule()` 换成 `led_strip_rgb_scene_apply()` 即可。

声控模式 `IS_light_music` 也走旧的 `led_strip_rgb_schedule()`，但显示已经改为由场景层接管：
`led_strip_rgb_schedule.c` 里的 `__led_strip_rgb_schedule_in_light_music__()` 只调一次
`led_strip_rgb_scene_sound_apply(fc_effect.music.m)`，具体的声控动画见上面「声控模式」一节。

---

## 6. 其它

- 新动画不使用 `_seg->colors`（颜色池由场景层维护），因此不会再出现
  「动画把颜色池原地旋转，重复下发同一条指令颜色会漂」的问题。
- 段参数的写入包在临界区里，和 10ms 的 `WS2812FX_service()` 互斥。
- 关灯走新的 `anim_power_off`（渐灭后熄灭）。

---

## 7. app 动画参数的存放与访问

app 下发的动画参数（`app_msg_anim_info_t`）统一放在**一个全局变量**里，其他模块直接读它，
不再到处传局部结构体：

| 项目 | 说明 |
| --- | --- |
| 全局缓存 | `app_msg_anim_info`（`app_msg_handle.h` 里 `extern volatile app_msg_anim_info_t app_msg_anim_info;`）：**其他模块需要时直接读取**，也可以继续用 `app_msg_anim_info_get() / app_msg_anim_info_set()`（内部带临界区）；它同时是 flash 保存/恢复用的那份数据（`save_flash.c`） |
| 模块级缓冲区 | `static app_msg_anim_info_t app_msg_anim_info_buf`：解析指令、把场景同步回参数时，**先写缓冲区、校验通过后再整体发布到全局** —— 不在 `app_msg` 任务的栈上开 `app_msg_anim_info_t`（36B × 多份，该任务栈本来就紧张）；同步指令用的 `user_alarm_t alarm[3]` 同样改成了模块级 `app_msg_sync_alarm_buf` |
| 临界区 | 写入全局统一走 `app_msg_anim_info_set()`（`OS_ENTER / OS_EXIT_CRITICAL` 包住 36B 的 memcpy），读取走 `app_msg_anim_info_get()`；解析过程在缓冲区里做，其他任务不会读到半成品 |
| 上电 | `led_strip_rgb_scene_start()` 直接读全局（也不再在栈上开一份）填充场景 |
| 写回时机 | 除了动画指令（`7F FF ..`），**静态色 / 亮度 / 速度 / 手机音乐律动** 等会改场景的指令处理完也会 `app_msg_anim_info_sync_from_scene()` 把场景的真实状态回写进全局，保证存 flash 和上报 app 的值都是当前真实状态 |

---

## 8. 后续可做的事

1. **`04 02 xx` 简易模式指令未实现**
   `与app通信使用到的指令.md` 里的「模式界面 / 基本」还列了一组 `04 02 xx`
   （三色跳变 `04 02 07`、七彩渐变 `04 02 0A`、红色渐变 `04 02 0B` ……）。
   目前 `app_msg_cmd_table[]` 里没有 `04 02` 条目，收到会打印 `unknown cmd`。
   文档里 `04 02 0B~0x11` 同时出现在「渐变」和「呼吸」两张表（应该是文档笔误），
   需要和 app 端确认后再加：
   - 在 `app_msg_handle.c` 的指令表里加一行 `{{0x04, 0x02}, 2, 3, app_msg_set_simple_mode_handle}`；
   - 在 `app_msg_set_simple_mode_handle()` 里按子索引查一张
     `{子索引 -> mode_idx / 颜色 / 方向 / 多少个灯为一组}` 的表，然后调用
     `led_strip_rgb_scene_set_from_app_info()` + `led_strip_rgb_scene_apply()`。
2. 星空 / 星云、流星 / 流星雨 目前只靠 `mode_idx` 区分（协议里它们的差别是「有没有底色」），
   如果 app 端希望「无底色 = 星空 / 有底色 = 星云」这种分法，可以在
   `app_msg_handle.c` 或模式表里按 `background == 黑` 再细分，改动很小。
3. 开/闭幕目前用 `anim_dir` 区分（dir=0 开幕、dir=1 闭幕），
   如果 app 端后续把开/闭幕拆成两个 `mode_idx`，只需在模式表里加一行。
