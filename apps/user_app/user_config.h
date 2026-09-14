#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

#define USER_DEBUG_ENABLE        1
#define USE_ILAMP_APP_INSTRUCT   1 // 使用 iLamp 的指令
#define USE_PRIVATE_APP_INSTRUCT 0 // 使用客户定制的app私有指令

// 只能使用一种app的指令
#if (USE_ILAMP_APP_INSTRUCT && USE_PRIVATE_APP_INSTRUCT)
#error "Can not use both iLamp and private app instruct at the same time"
#elif (0 == USE_ILAMP_APP_INSTRUCT && 0 == USE_PRIVATE_APP_INSTRUCT)
#error "Must use either iLamp or private app instruct"
#endif

#endif
