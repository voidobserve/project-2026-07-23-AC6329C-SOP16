#ifndef __ALARM_H__
#define __ALARM_H__

#include "typedef.h"

#define ALARM_MAX_NUM 3

typedef struct {
	u8 enable; // 是否使能
	u8 mode; 
	
	u8 week;
	u8 hour;
	u8 minute;
	u32 countdown_time;

} alarm_t;

extern volatile alarm_t alarm[ALARM_MAX_NUM];

#endif
