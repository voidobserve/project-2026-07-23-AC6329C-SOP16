#ifndef __APP_MSG_TYPEDEF_H__
#define __APP_MSG_TYPEDEF_H__

#include "typedef.h"

// 设备与app通信中，最大传递的颜色数量
#define APP_MSG_COLOR_NUM_MAX 8
#define APP_MSG_COLOR_BUF_MAX (APP_MSG_COLOR_NUM_MAX * 3)

#define INSTRUCTION_PREFIX_LEN 3

// app传递过来的消息中，动画模式的索引值
enum
{
    APP_MSG_MOD_IDX_STATIC = 0x01,       // 静态色
    APP_MSG_MOD_IDX_GRADUAL,             // 渐变
    APP_MSG_MOD_IDX_JUMP,                // 跳变
    APP_MSG_MOD_IDX_BREATH,              // 呼吸
    APP_MSG_MOD_IDX_RUNNING_WATER,       // 流水
    APP_MSG_MOD_IDX_ACCUMULATION,        // 堆积
    APP_MSG_MOD_IDX_STARRY_SKY,          // 星空
    APP_MSG_MOD_IDX_NEBULA,              // 星云
    APP_MSG_MOD_IDX_METEOR,              // 流星
    APP_MSG_MOD_IDX_METEOR_SHOWER,       // 流星雨
    APP_MSG_MOD_IDX_OPENING_AND_CLOSING, // 开幕 or 闭幕
    APP_MSG_MOD_IDX_RUN,                 // 跑动
    APP_MSG_MOD_IDX_RUN_COLLECTION,      // 跑动集合（0x0D，内置多个跑动子动画循环执行）
};
typedef u8 app_msg_mod_idx_t;

/*
    设备与app通信中，
    幻彩灯动画相关信息对应的结构体定义
*/
typedef struct
{
    u16 format_head;            // 格式头 ，目前固定是 0x 7F FF
    app_msg_mod_idx_t mode_idx; // 动画模式索引
    /*
        anim_dir 动画方向
        0x00：正向
        0x01：反向
        正向：从控制板连接灯带的一端开始，沿灯带的方向向另一端推进
        反向：从灯带另一端开始，沿相反方向
    */
    u8 anim_dir;
    u8 anim_speed;         // 动画速度
    u8 anim_brightness;    // 动画亮度
    /*
        byte6 / byte7 的字段名与含义（以 `与app通信使用到的指令.md` 为准）：
            byte6：保留，app 未使用该功能 —— 接收时忽略，上报时填 0
            byte7：协议里叫「段数量」，实际含义是「多少个灯为一组」
    */
    u8 byte_reserved;       // byte6：保留字节
    u8 leds_per_seg;       // byte7：多少个灯为一组
    u8 background_color_r; // 背景色，r分量
    u8 background_color_g; // 背景色，g分量
    u8 background_color_b; // 背景色，b分量
    u8 color_num;          // 动画颜色数量

    u8 color_buf[APP_MSG_COLOR_BUF_MAX]; // 动画颜色数据

} app_msg_anim_info_t;

#define APP_MSG_ANIM_INFO_FIXED_LEN \
    (sizeof(app_msg_anim_info_t) - APP_MSG_COLOR_BUF_MAX)

extern const u8 instruction_prefix[INSTRUCTION_PREFIX_LEN]; // 指令前缀

#endif