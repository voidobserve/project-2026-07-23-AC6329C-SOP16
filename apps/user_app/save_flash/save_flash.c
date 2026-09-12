
#include "system/includes.h"
#include "syscfg_id.h"
#include "save_flash.h"
#include "user_config.h"

#define FLASH_CRC_DATA 0xC5

static volatile u8 flag_is_enable_to_save = 0; // 标志位，是否使能了保存

volatile u16 save_data_dly_cnt = 0; // 数据延时保存的计数
volatile save_flash_t save_data;

void user_data_init(void)
{
    int ret = 0;
    ret = syscfg_read(CFG_USER_LED_DATA, (void *)(&save_data),
                      sizeof(save_flash_t));
    if (ret != sizeof(save_flash_t)) {
        // 如果读取到的数据个数不一致
        // printf("read save info error \n");
        memset((void *)&save_data, 0, sizeof(save_flash_t));
    }

    if (save_data.header != FLASH_CRC_DATA) {
        // 保存的数据无效，可能是第一次上电，或者是数据损坏，重新初始化
        save_data.header = FLASH_CRC_DATA;
        led_strip_rgb_schedule_init();
        // led_strip_white_schedule_init();
        user_data_save_enable();
    } else {
        // 保存的数据有效，根据保存的数据进行初始化
        memcpy((void *)(&fc_effect), (void *)(&save_data.fc_save),
               sizeof(fc_effect_t));
        // memcpy((u8 *)(&led_strip_white), (u8 *)(&save_data.led_strip_white),
        //        sizeof(led_strip_white_t));
    }

    // 每次上电，默认打开设备
    fc_effect.on_off_flag = DEVICE_ON;
    // led_strip_white.is_dev_open = 1;
}

/**
 * @brief 
 * 
 */
void user_data_save_time_10ms_isr(void)
{
    if (flag_is_enable_to_save) {
        if (save_data_dly_cnt < (((u16)-1) - 10)) {
            save_data_dly_cnt += 10;
        }
    } else {
        save_data_dly_cnt = 0;
    }
}

/**
 * @brief 启用用户数据保存功能
 * @details 将保存数据计数器清零，并设置允许保存标志位
 */
void user_data_save_enable(void)
{
    save_data_dly_cnt = 0;      // 将保存数据延迟计数器清零
    flag_is_enable_to_save = 1; // 设置允许保存标志位为1，启用保存功能
}

static void user_data_save(void)
{
    int ret = 0;
    save_data.header = FLASH_CRC_DATA; // 表示数据有效

    memcpy((void *)(&save_data.fc_save), (void *)(&fc_effect),
           sizeof(fc_effect_t));

    os_time_dly(1); // 先让出cpu，处理其他任务，防止看门狗复位
    ret = syscfg_write(CFG_USER_LED_DATA, (u8 *)(&save_data),
                       sizeof(save_flash_t));

#if USER_DEBUG_ENABLE
    printf("save info done \n");
#endif
}

void user_data_save_handle(void)
{
    if (!(flag_is_enable_to_save &&
          save_data_dly_cnt >= DELAY_SAVE_FLASH_TIMES)) {
        return;
    }

    save_data_dly_cnt = 0;
    flag_is_enable_to_save = 0;
    user_data_save();
}
