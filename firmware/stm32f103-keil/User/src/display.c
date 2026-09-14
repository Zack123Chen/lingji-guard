#include "display.h"
#include "main.h"

static char oled_buf[20]; 

void vDisplayTask(void *pvParameters) {
    static uint8_t frame = 0;
    char pet_face[8];
    uint8_t last_ready;
    struct sensorData_t *p;

    for(;;) {
        // 1. 检查翻页标志位（由 vKeyTask 触发）
        if (g_page_changed) {
            OLED_Clear();
            g_page_changed = 0;
        }

        // 获取最新传感器数据
        last_ready = (SystemData.calc_idx + QUEUE_SIZE - 1) % QUEUE_SIZE;
        p = &SystemData.dataQueue[last_ready];
        frame = !frame;

        taskENTER_CRITICAL(); 

        // --- 第一行：状态与宠物表情 (固定栏) ---
        if (p->status & 0x80)      strcpy(pet_face, frame ? " X_X " : " x_x ");
        else if (p->status & 0x04) strcpy(pet_face, frame ? " \\o/ " : " /o\\ ");
        else if (p->status & 0x02) strcpy(pet_face, frame ? "(^o^)" : "(^-^)");
        else                       strcpy(pet_face, frame ? "-.-zZ" : "=_=Zz");

        char *state_text =  (p->status & 0x80) ? "ALARM!" : 
                            (p->status & 0x04) ? "RUN" : 
                            (p->status & 0x02) ? "WALK" : "SLEEP";
        
        sprintf(oled_buf, "St:%-6s %s", state_text, pet_face);
        OLED_ShowString(1, 1, oled_buf);

        // --- 中间行：根据 g_ui_page 显示不同页面 ---
        if (g_ui_page == 0) {
            // 【页面 0：核心健康】
            sprintf(oled_buf, "Heart: %3d BPM ", p->bpm); 
            OLED_ShowString(2, 1, oled_buf);

            sprintf(oled_buf, "                ");
            OLED_ShowString(3, 1, oled_buf);
        } 
        else if (g_ui_page == 1) {
            // 【页面 1：环境与系统】
            sprintf(oled_buf, "Temp : %4.1f C  ", p->temp_val);
            OLED_ShowString(2, 1, oled_buf);

            sprintf(oled_buf, "Heap : %4d B   ", (int)xPortGetFreeHeapSize());
            OLED_ShowString(3, 1, oled_buf);
        }
        else if (g_ui_page == 2) {
            // 【页面 2：运动数据 (假设你以后会加步数)】
            //sprintf(oled_buf, "Step : %5d    ", p->step_count); 
           // OLED_ShowString(2, 1, oled_buf);

            sprintf(oled_buf, "Stat : ACTIVE  ");
            OLED_ShowString(3, 1, oled_buf);
        }

        // --- 第四行：页码指示器 (让你看到在第几页) ---
        // 使用 [n/m] 格式显示在右下角
        sprintf(oled_buf, "PAGE [%d/%d] ----", g_ui_page + 1, MAX_PAGES);
        OLED_ShowString(4, 1, oled_buf);

        taskEXIT_CRITICAL(); 

        // 200ms 刷新频率，平衡动画流畅度与按键响应
        vTaskDelay(pdMS_TO_TICKS(200)); 
    }
}
