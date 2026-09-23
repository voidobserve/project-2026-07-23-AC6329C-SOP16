#ifndef __APP_MSG_HANDLE_H__
#define __APP_MSG_HANDLE_H__

#include "app_msg_typedef.h"

/*
    一条 app 指令可能的最大长度：
    指令前缀 + app_msg_anim_info_t中的12 字节固定字段 + 最大颜色数据
*/
// #define APP_MSG_POST_BUF_MAX_LEN                                               \
//     (INSTRUCTION_PREFIX_LEN + 12 + APP_MSG_COLOR_BUF_MAX)
#define APP_MSG_POST_BUF_MAX_LEN 100

void app_msg_anim_info_init(void);
void app_msg_anim_info_set(app_msg_anim_info_t *info);

/*
    在蓝牙写回调里调用：只把指令投递出去，不做解析和执行
*/
void app_msg_post(uint8_t *buf, uint16_t len);

/*
    在 app_msg_handle 任务里调用：从投递缓冲区取出一条待处理的指令
    返回 1 表示取到指令，返回 0 表示没有待处理的指令
*/
u8 app_msg_post_fetch(uint8_t *buf, uint16_t *len);

/*
    在 app_msg_handle 任务里调用：解析并执行一条指令
*/
void app_msg_handle(uint8_t *buf, uint16_t len);

void app_msg_anim_info_get(app_msg_anim_info_t *info);

#endif
