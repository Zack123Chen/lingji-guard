#include "power.h"
#include "main.h"

void vPowerTask(void *pvParameters)
{
	for(;;){vTaskDelay(pdMS_TO_TICKS(200));}
}
