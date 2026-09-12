#include "user_ble_notify.h"
#include "typedef.h"
#include "btstack_typedef.h"
#include "ble_multi_profile.h"
#include "att.h"
#include "user_config.h"

volatile user_ble_notify_t user_ble_notify_param = {
    // .send_buff[0][0] = {0},
    // .send_buff_len[0] = {0},
    .send_buff_head = 0,
    .send_buff_tail = 0,
    .send_buff_num = 0,
};

void user_ble_notify_param_put(u8 *buff, u16 len)
{
    // 如果缓冲区满，下面的操作会覆盖缓冲区中旧的数据：

    // 先偏移索引，再存入数据
    user_ble_notify_param.send_buff_head++;
    if (user_ble_notify_param.send_buff_head >=
        USER_BLE_NOTIFY_SEND_BUFF_MAX_NUM) {
        user_ble_notify_param.send_buff_head = 0;
    }

    // 将指令保存到对应的缓冲区中
    memcpy(
        user_ble_notify_param.send_buff[user_ble_notify_param.send_buff_head],
        buff, len);
    // 将指令的长度保存到对应的缓冲区中
    user_ble_notify_param.send_buff_len[user_ble_notify_param.send_buff_head] =
        len;
    // 表示指令数量加1：
    user_ble_notify_param.send_buff_num++;
    if (user_ble_notify_param.send_buff_num >=
        USER_BLE_NOTIFY_SEND_BUFF_MAX_NUM) {
        user_ble_notify_param.send_buff_num = USER_BLE_NOTIFY_SEND_BUFF_MAX_NUM;
    }
}

// // 获取指令数量
// u8 ble_notify_param_get_num(void)
// {
//     return user_ble_notify_param.send_buff_num;
// }

void user_ble_notify_param_handle(void)
{
#if USE_ILAMP_APP_INSTRUCT
    volatile u8 send_buf[USER_BLE_NOTIFY_SEND_BUFF_MAX_LEN];
#endif

    if (user_ble_notify_param.send_buff_num == 0) {
        // 缓冲区中没有存放指令，直接返回
        return;
    }

    // 先偏移索引，再取出数据
    user_ble_notify_param.send_buff_tail++;
    if (user_ble_notify_param.send_buff_tail >=
        USER_BLE_NOTIFY_SEND_BUFF_MAX_NUM) {
        user_ble_notify_param.send_buff_tail = 0;
    }

    extern hci_con_handle_t ZD_HCI_handle;
#if USE_ILAMP_APP_INSTRUCT

    // 跳过前6个字节，前6个字节存放蓝牙地址(目前未使用，所以可以是任意值)
    memcpy(
        send_buf + 6,
        user_ble_notify_param.send_buff[user_ble_notify_param.send_buff_tail],
        user_ble_notify_param
            .send_buff_len[user_ble_notify_param.send_buff_tail]);

    ble_comm_att_send_data(
        ZD_HCI_handle, ATT_CHARACTERISTIC_fff1_01_VALUE_HANDLE,
        send_buf, //
        user_ble_notify_param
                .send_buff_len[user_ble_notify_param.send_buff_tail] +
            6, // 指令的长度
        ATT_OP_AUTO_READ_CCC);
    user_ble_notify_param.send_buff_num--;

#elif USE_PRIVATE_APP_INSTRUCT

    ble_comm_att_send_data(
        ZD_HCI_handle, ATT_CHARACTERISTIC_fff1_01_VALUE_HANDLE,
        (user_ble_notify_param
             .send_buff[user_ble_notify_param.send_buff_tail]), // 指令
        user_ble_notify_param
            .send_buff_len[user_ble_notify_param.send_buff_tail], // 指令的长度
        ATT_OP_AUTO_READ_CCC);
    user_ble_notify_param.send_buff_num--;

#endif
}