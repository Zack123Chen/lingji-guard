#include "main.h"

SemaphoreHandle_t xPageEventSemaphore;
struct DataManager_t SystemData = {1, 0, 0};
uint8_t g_ui_page;    
uint8_t g_page_changed; 

int main(void)
{
	SystemClock_Init();
	SystemHardware_Init();
	SystemAlgorithm_Init();
	FreeRTOS_Init();
	xPageEventSemaphore = xSemaphoreCreateBinary();
}

void start_task(void *pvParameters) {
	taskENTER_CRITICAL();	// avoid being interrupted

	xTaskCreate	(vSensorTask,		"Sensor",		64,	NULL, 2, NULL);
	xTaskCreate	(vLogicTask,		"Logic",		256,	NULL, 2, NULL);
	xTaskCreate	(vPowerTask,		"Power",		16,	NULL, 1, NULL);
	xTaskCreate	(vDisplayTask,	"Display",	128,	NULL, 1, NULL);
	xTaskCreate	(vCommTask,			"Comm",			256,	NULL, 3, NULL);
	xTaskCreate (vKeyTask,			"Key",			32,	NULL, 3, NULL);

	vTaskDelete(StartTask_Handler); 
	taskEXIT_CRITICAL();
}

// FreeRTOS stack overflow handler
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
    printf("Stack Overflow in task: %s\r\n", pcTaskName);
    taskDISABLE_INTERRUPTS();
    for( ;; );
}
