#ifndef __APP_FREERTOS_H__
#define __APP_FREERTOS_H__

#ifdef __cplusplus
extern "C" {
#endif

#include "FreeRTOS.h"

/* 创建项目当前需要的 FreeRTOS 任务。pdPASS 表示创建成功。 */
BaseType_t APP_FreeRTOS_Init(void);

#ifdef __cplusplus
}
#endif

#endif /* __APP_FREERTOS_H__ */
