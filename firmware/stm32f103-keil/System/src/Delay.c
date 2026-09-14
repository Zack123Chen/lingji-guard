#include "delay.h"
#include "FreeRTOS.h"
#include "task.h"

#define DWT_CR         *(volatile uint32_t *)0xE0001000
#define DWT_CYCCNT     *(volatile uint32_t *)0xE0001004
#define DEM_CR         *(volatile uint32_t *)0xE000EDFC
    
#define DEM_CR_TRCENA  (1 << 24)
#define DWT_CR_CYCCNTENA (1 << 0)

void DWT_Init(void) {
	DEM_CR |= DEM_CR_TRCENA;
	DWT_CYCCNT = 0;
	DWT_CR |= DWT_CR_CYCCNTENA;
}

void Delay_us(uint32_t xus) {
	uint32_t startTicks = DWT_CYCCNT;
	uint32_t targetTicks = xus * (SystemCoreClock / 1000000); 
	while ((DWT_CYCCNT - startTicks) < targetTicks);
}

void Delay_ms(uint32_t xms) {
	if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
		vTaskDelay(pdMS_TO_TICKS(xms)); 
	} else {
		while(xms--) {
			Delay_us(1000);
		}
	}
}
