#ifndef __USER_THREAD_COMMUNICATION_H__
#define __USER_THREAD_COMMUNICATION_H__

#include "typedef.h"

// 定义需要通信的线程名称
#define  LED_STRIP_RGB_TASK_NAME "app_msg_handle"

enum
{
    USER_THREAD_COMM_MSG_TYPE_NONE = 0,                 // 无效消息
    USER_THREAD_COMM_MSG_TYPE_APP_MSG_ANIM_INFO_UPDATE, // 动画信息更新
    USER_THREAD_COMM_MSG_TYPE_APP_MSG_RAW, // app 下发的原始指令（需要在任务里解析）
};
typedef u8 user_msg_type_t;

void user_thread_communication_send_msg(const char *task_name,
                                        user_msg_type_t msg_type);
int user_thread_communication_recv_msg(user_msg_type_t msg_type);

/**
 * @brief 接收任意类型的用户消息
 *
 * @param msg_type 输出参数，用于返回收到的消息类型
 * @return int 0 成功；非 0 对应的错误码
 */
int user_thread_communication_recv_msg_any(user_msg_type_t *msg_type);

#endif
