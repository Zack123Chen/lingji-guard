#ifndef __CONNECT_H
#define __CONNECT_H

#include "stm32f10x.h"

struct sensorData_t;

void vCommTask(void *pvParameters);
void sendLTE(struct sensorData_t *p);

#endif
