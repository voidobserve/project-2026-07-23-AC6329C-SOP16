#include "user_thread_communication.h"
#include "os_api.h"

void user_thread_communication_send_msg(const char *task_name, user_msg_type_t msg_type)
{
    /*
		发送Q_USER类型消息队列，只发送1个消息
	*/
    os_taskq_post(task_name, 1, msg_type);
}

/**
 * @brief 接收指定类型的消息
 * 
 * @param msg_type 
 * @return int 
 * 			0 成功
 * 			非0 对应的错误码
 */
int user_thread_communication_recv_msg(user_msg_type_t msg_type)
{
    int ret = 0;
    int msg[32] = {0};
    ret = os_taskq_pend(os_current_task, msg, 1);
    if (ret != OS_TASKQ) {
        // 不是消息对队列
        ret = 1;
        goto label_user_thread_communication_recv_msg_err;
    }

    if (msg[0] != Q_USER) {
        // 不是用户消息队列
        ret = 2;
        goto label_user_thread_communication_recv_msg_err;
    }

    if (msg[1] != msg_type) {
        // 不是指定的消息类型
        ret = 3;
        goto label_user_thread_communication_recv_msg_err;
    }

label_user_thread_communication_recv_msg_err:
    return ret;
}

/**
 * @brief 接收任意类型的用户消息
 *
 * @param msg_type 输出参数，用于返回收到的消息类型
 * @return int 0 成功；非 0 对应的错误码
 */
int user_thread_communication_recv_msg_any(user_msg_type_t *msg_type)
{
    int ret = 0;
    int msg[32] = {0};

    if (msg_type == NULL) {
        // 输出参数为空
        return 4;
    }

    // ret = os_taskq_pend(os_current_task, msg, 1);
    ret = os_taskq_pend(LED_STRIP_RGB_TASK_NAME, msg, 1);
    if (ret != OS_TASKQ) {
        // 不是消息队列
        return 1;
    }

    if (msg[0] != Q_USER) {
        // 不是用户消息队列
        return 2;
    }

    // 返回收到的消息类型，由调用者决定怎么处理
    *msg_type = (user_msg_type_t)msg[1];

    return 0;
}
