#ifndef __USER_THREAD_COMMUNICATION_H__
#define __USER_THREAD_COMMUNICATION_H__

#include "typedef.h"

/*
    定义需要通信的线程名称

    注意：内核（maskrom）编译时的 configMAX_TASK_NAME_LEN 是 12，
    xTaskGetHandle() 里有 configASSERT(strlen(name) < 12)，
    任务名长度 >= 12 字节会触发断言：打印 "xTaskGetHandle 2145" 后死循环复位。

    所以任务名必须 <= 11 字节，而且必须与 user_main.c 里 task_create()
    的名字、app_main.c 里 task_info_table 的名字完全一致
    （task_create() 用 strcmp 在 task_info_table 里查栈大小，查不到会软复位）。
*/
#define  LED_STRIP_RGB_TASK_NAME "app_msg"

enum
{
    USER_THREAD_COMM_MSG_TYPE_NONE = 0,                 // 无效消息
    // USER_THREAD_COMM_MSG_TYPE_APP_MSG_ANIM_INFO_UPDATE, // 动画信息更新
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
