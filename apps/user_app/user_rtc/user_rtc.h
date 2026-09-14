#ifndef __USER_RTC_H__
#define __USER_RTC_H__

#include "typedef.h"
#include "system/sys_time.h"

typedef struct
{
    u16 year;
    u8 month;
    u8 day;
    u8 hour;
    u8 min;
    u8 sec;
} sys_time_t;

typedef struct
{
    u8 enable; // 是否使能, 0:关闭， 1:打开
	// 闹钟到来后，控制设备开机或关机
	u8 power_on; // 0:关机， 1:开机
    /**
     * @brief 闹钟模式  
	 * 
	 * bit 0 : 
	 * bit 1 : 
     * 
     */
    u8 weekday;

    u8 hour;
    u8 min;
    u8 sec;
} user_alarm_t;

typedef struct
{
    u8 hour;
    u8 min;
    u8 sec;

    u8 weekday; // 星期，需要自行计算
} user_time_t;

void user_rtc_init(void);
void user_rtc_get_time(user_time_t *cur_time); 
void user_rtc_set_time(user_time_t *cur_time);

void user_rtc_set_alarm(user_alarm_t *alarm, u8 alarm_idx);
void user_rtc_get_alarm_info(user_alarm_t *alarm, u8 alarm_idx);

void user_rtc_handle(void);


#endif
