#ifndef __MAIN_H
#define __MAIN_H

// Stardard libs
#include <stdio.h>
#include <string.h> 
#include <stdlib.h> 
#include <math.h>
// FreeRTOS
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
// System
#include "stm32f10x.h"
#include "Delay.h" 
// Hardware
#include "oled.h"
#include "ds18b20.h"
#include "mpu6050.h"
#include "max30102.h"
#include "ch9141k.h"
#include "gps.h"
#include "lte.h"
#include "button.h"
#include "led1.h"
#include "led2.h"
#include "motor.h"
// App
#include "init.h"
#include "algorithm.h"
#include "collection.h"
#include "power.h"
#include "status.h"
#include "connect.h"
#include "display.h"

#define QUEUE_SIZE 8
#define MAX_PAGES 3 

struct sensorData_t{
	MPU_Data_t mpu_data;
	uint32_t activityLevel;
	uint32_t red_led, ir_led; 
	TickType_t timestamp;
	float temp_val;
	float current_lat, current_lng;
	uint8_t bpm;
	uint8_t spo2;
	uint8_t status; // [alarm, 0, highHR, lowHR, 0, running, walking, sleeping]
};

struct DataManager_t{
    struct sensorData_t dataQueue[QUEUE_SIZE];
    volatile uint8_t write_idx;
    volatile uint8_t calc_idx;
    volatile uint8_t send_idx;
};

extern struct DataManager_t SystemData;
extern TaskHandle_t StartTask_Handler;
extern uint8_t g_ui_page;    
extern uint8_t g_page_changed; 

void start_task(void *pvParameters);

#endif
