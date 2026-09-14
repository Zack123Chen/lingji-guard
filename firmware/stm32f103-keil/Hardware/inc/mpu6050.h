#ifndef __MPU6050_H
#define __MPU6050_H
#include "stm32f10x.h"

typedef struct {
	int16_t AccX;
	int16_t AccY;
	int16_t AccZ;
	int16_t GyroX;
	int16_t GyroY;
	int16_t GyroZ;
	
	float roll;
	float pitch;

} MPU_Data_t;

void MPU6050_Init(void);
MPU_Data_t MPU6050_GetData(void);

#endif
