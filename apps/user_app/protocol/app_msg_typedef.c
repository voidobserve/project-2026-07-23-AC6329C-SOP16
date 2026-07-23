#include "app_msg_typedef.h"
#include "typedef.h"
#include "Adafruit_NeoPixel.H"

#define APP_MSG_MAX_LEN 20

// 和通信协议对应
const u8 rgb_sequence_map[6] =	{
		NEO_RGB,
		NEO_RBG,
		NEO_GRB,
		NEO_GBR,
		NEO_BRG,
		NEO_BGR,
};

typedef enum
{
	MSG_TYPE_DEVICE_STATUS,	   // 设备开关机
	MSG_TYPE_SET_STATIC_COLOR, // 设置静态颜色
};

typedef struct
{
	u8 msg_type;
	u8 msg_databuf[][APP_MSG_MAX_LEN];
} app_msg_map_t;

app_msg_map_t app_msg_map = {
	{MSG_TYPE_DEVICE_STATUS, {0x01, 0x01}},
	{MSG_TYPE_SET_STATIC_COLOR, {0x04, 0x01, 0x01, 0x1E}},
};

// 需要给 数据最后一个有效数据字节赋值为无效数据
void app_msg_map_init(void)
{
}
