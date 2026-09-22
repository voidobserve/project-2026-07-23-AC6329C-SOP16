#include "app_msg_typedef.h"
#include "typedef.h"
#include "Adafruit_NeoPixel.H"
 
// 指令前缀 instruction prefix
const u8 instruction_prefix[INSTRUCTION_PREFIX_LEN] = {
	0x02, 0x01, 0xE9,
};

