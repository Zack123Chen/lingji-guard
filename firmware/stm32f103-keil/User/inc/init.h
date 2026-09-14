#ifndef __INIT_H
#define __INIT_H

#include "stm32f10x.h"

void SystemClock_Init(void);
void SystemHardware_Init(void);
void SystemAlgorithm_Init(void);
void FreeRTOS_Init(void);

#endif
