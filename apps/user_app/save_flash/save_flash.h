
#ifndef __SAVE_FLASH_H__
#define __SAVE_FLASH_H__

#include "led_strand_effect.h"
#include "led_strip_driver.h"

#include "led_strip_rgb_schedule.h"
#include "led_strip_white_schedule.h"

#pragma pack(1)
typedef struct
{
    unsigned char header; // 头部

    fc_effect_t fc_save;
    led_strip_white_t led_strip_white;

} save_flash_t;

#pragma pack()

// 需要保存数据时，延时保存的时间：（单位：ms）
#define DELAY_SAVE_FLASH_TIMES ((u16)3000)

void user_data_save_time_10ms_isr(void);

void user_data_init(void);
void user_data_save_enable(void);
void user_data_save_handle(void);

#endif