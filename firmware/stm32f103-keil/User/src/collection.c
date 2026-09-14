#include "collection.h"
#include "main.h"

void vSensorTask(void *pvParameters) {
	TickType_t xLastWakeTime;
	const TickType_t xFrequency = pdMS_TO_TICKS(100);

	xLastWakeTime = xTaskGetTickCount();
	
	for(;;) {
		vTaskDelayUntil(&xLastWakeTime, xFrequency);
		struct sensorData_t *p = &SystemData.dataQueue[SystemData.write_idx];
		uint8_t idx = SystemData.write_idx;
        
		MAX30102_Read(&p->red_led, &p->ir_led);
		p -> mpu_data = MPU6050_GetData();
		p -> temp_val = DS18B20_GetTemp();
		
		SystemData.dataQueue[idx].timestamp = xTaskGetTickCount();

		SystemData.write_idx = (idx + 1) % QUEUE_SIZE;
  }
	
}
