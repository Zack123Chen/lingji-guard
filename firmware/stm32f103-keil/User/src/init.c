#include "init.h"
#include "main.h"

void SystemClock_Init(void)
{
	RCC_DeInit();
	RCC_HSEConfig(RCC_HSE_ON); // start external 8M oscillator
	if (RCC_WaitForHSEStartUp() == SUCCESS) 
	{
		FLASH_PrefetchBufferCmd(FLASH_PrefetchBuffer_Enable);
		FLASH_SetLatency(FLASH_Latency_2);

		RCC_HCLKConfig(RCC_SYSCLK_Div1);   	// HCLK = 72M
		RCC_PCLK2Config(RCC_HCLK_Div1);   	// APB2 = 72M
		RCC_PCLK1Config(RCC_HCLK_Div2);   	// APB1 = 36M

		RCC_PLLConfig(RCC_PLLSource_HSE_Div1, RCC_PLLMul_9);
		RCC_PLLCmd(ENABLE);
		while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET);

		RCC_SYSCLKConfig(RCC_SYSCLKSource_PLLCLK);
		while (RCC_GetSYSCLKSource() != 0x08);
	}
	DWT_Init();
}

void SystemHardware_Init(void)
{
	CH9141K_Init(115200);
	USART2_Init(115200);
	USART3_Init(9600);
	Module_4G_Init();

	OLED_Init();
	DS18B20_Init();
	MPU6050_Init();
	MAX30102_Init();

	OLED_Clear();
	OLED_ShowString(1, 1, "System Starting.");
	OLED_ShowString(2, 1, "Sensors: OK");
	OLED_ShowString(3, 1, "Wait for data..");
}

TaskHandle_t StartTask_Handler;

void SystemAlgorithm_Init(void)
{

}

void FreeRTOS_Init(void)
{
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_4);
	xTaskCreate((TaskFunction_t)start_task, "start", 256, NULL, 1, &StartTask_Handler);
	vTaskStartScheduler();
}
