#include "connect.h"
#include "main.h"
#include <stdio.h>

static void Internal_CH9141K_RawSend(char *str) {
    while (*str) {
        if (USART_GetFlagStatus(USART1, USART_FLAG_ORE) != RESET) {
            USART_ReceiveData(USART1); 
        }
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
        USART_SendData(USART1, *str++);
    }
}

void Bluetooth_SendString(char *str) {
    taskENTER_CRITICAL();
    Internal_CH9141K_RawSend(str);
    taskEXIT_CRITICAL();
    vTaskDelay(pdMS_TO_TICKS(100));
}

static const char *Telemetry_StateText(uint8_t status) {
    if (status & STATUS_ALARM) {
        return "alarm";
    }
    if (status & STATUS_RUNNING) {
        return "running";
    }
    if (status & STATUS_WALKING) {
        return "walking";
    }
    return "sleeping";
}

void vCommTask(void *pvParameters) {
    uint8_t ltePublishTicks = 0;
    
    for(;;) {
        struct sensorData_t *p = &SystemData.dataQueue[SystemData.send_idx];
        
        // 修改：每个包自带分隔符，且不再额外添加
        char bpm_buf[32];
        snprintf(bpm_buf, sizeof(bpm_buf), "BPM:%d Tp:%.1f\r\n", p->bpm, p->temp_val);
        Bluetooth_SendString(bpm_buf);
        
        char id_buf[32];
        snprintf(id_buf, sizeof(id_buf), "ID:STM3210086\r\n");
        Bluetooth_SendString(id_buf);

        if (++ltePublishTicks >= 20) {
            sendLTE(p);
            ltePublishTicks = 0;
        }
        
        SystemData.send_idx = (SystemData.send_idx + 1) % QUEUE_SIZE;
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void sendLTE(struct sensorData_t *p) {
    char payload[128];

    snprintf(payload, sizeof(payload),
        "{\"id\":\"STM3210086\",\"hr\":%d,\"temp\":%.1f,\"state\":\"%s\",\"isSimulator\":false}",
        p->bpm,
        p->temp_val,
        Telemetry_StateText(p->status));

    USART2_SendString("AT+QMTPUB=0,0,0,0,\"HIT/PetData\"\r\n");
    vTaskDelay(pdMS_TO_TICKS(300));
    USART2_SendString(payload);
    vTaskDelay(pdMS_TO_TICKS(100));
    USART2_SendByte(0x1A);
}
