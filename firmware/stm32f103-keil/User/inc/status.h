#ifndef __STATUS_H
#define __STATUS_H

#include "stm32f10x.h"

// [alarm, 0, highHR, lowHR, 0, running, walking, sleeping]
#define STATUS_SLEEPING 0x01  // 0000 0001
#define STATUS_WALKING  0x02  // 0000 0010
#define STATUS_RUNNING  0x04  // 0000 0100
#define STATUS_LOW_HR   0x10  // 0001 0000
#define STATUS_HIGH_HR  0x20  // 0010 0000
#define STATUS_ALARM    0x80  // 1000 0000

struct sensorData_t;

void calculateStatus(struct sensorData_t *p);

#endif
