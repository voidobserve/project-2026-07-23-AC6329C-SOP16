#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

#define USER_DEBUG_ENABLE        1

/*
	两种指令的区别： 
	iLAmp：在设备返回数据给app时，会带有6个字节的数据(用于存放蓝牙地址)作为前缀
	私有指令：在设备返回数据给app时，不会带有6个字节的前缀
*/
#define USE_ILAMP_APP_INSTRUCT   0 // 使用 iLamp 的指令
#define USE_PRIVATE_APP_INSTRUCT 1 // 使用客户定制的app私有指令

// 只能使用一种app的指令
#if (USE_ILAMP_APP_INSTRUCT && USE_PRIVATE_APP_INSTRUCT)
#error "Can not use both iLamp and private app instruct at the same time"
#elif (0 == USE_ILAMP_APP_INSTRUCT && 0 == USE_PRIVATE_APP_INSTRUCT)
#error "Must use either iLamp or private app instruct"
#endif


#endif
