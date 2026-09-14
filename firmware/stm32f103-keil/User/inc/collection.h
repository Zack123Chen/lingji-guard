#ifndef __COLLECTION_H
#define __COLLECTION_H

#include "stm32f10x.h"

void vSensorTask(void *pvParameters);
void collectData_SingleFrame(void);

#endif
