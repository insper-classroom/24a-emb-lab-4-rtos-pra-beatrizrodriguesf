/*
 * LED blink with FreeRTOS
 */
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>
#include <queue.h>

#include "ssd1306.h"
#include "gfx.h"

#include "pico/stdlib.h"
#include <stdio.h>


const int TRIGER = 4;
const int ECHO = 5;

SemaphoreHandle_t xSemaphore_triger;
QueueHandle_t xQueueDistance;
QueueHandle_t xQueueTime;


void triger_task() {
    printf("triger\n");
    gpio_init(TRIGER);
    gpio_set_dir(TRIGER, GPIO_OUT);

    while (true) {
        gpio_put(TRIGER, 1);
        vTaskDelay(pdMS_TO_TICKS(5));
        gpio_put(TRIGER, 0);

        vTaskDelay(pdMS_TO_TICKS(1000));
        xSemaphoreGive(xSemaphore_triger);
    }
}

void echo_callback(uint gpio, uint32_t events){
    if (events == 0x4) { // fall edge
        int time_end = to_us_since_boot(get_absolute_time());
        xQueueSendFromISR(xQueueTime, &time_end, 0);
    } else if (events == 0x8) { // rise edge
        int time_start = to_us_since_boot(get_absolute_time());
        xQueueSendFromISR(xQueueTime, &time_start, 0);
    }
}

void echo_task() {
    gpio_init(ECHO);
    gpio_set_dir(ECHO, GPIO_IN);

    gpio_set_irq_enabled_with_callback(ECHO, GPIO_IRQ_EDGE_FALL | GPIO_IRQ_EDGE_RISE, true,
                                       &echo_callback);

    printf("echo_task\n");
    int time_start = 0;
    int time_end = 0;

    while (true) {
        if (xQueueReceive(xQueueTime, &time_start, 100) && xQueueReceive(xQueueTime, &time_end, 100)) {
            double dist = (time_end - time_start)*0.017;
            xQueueSend(xQueueDistance, &dist, 0);
            printf("echo: %lf", dist);
        }
    }
}

void oled_task() {
    printf("Inicializando Driver\n");
    ssd1306_init();

    printf("Inicializando GLX\n");
    ssd1306_t disp;
    gfx_init(&disp, 128, 32);

    while (true) {
        if (xSemaphoreTake(xSemaphore_triger, pdMS_TO_TICKS(1200)) == pdTRUE) {
            double distancia;
            if (xQueueReceive(xQueueDistance, &distancia, pdMS_TO_TICKS(200))) {
                printf("oled: %lf", distancia);

                char str[24];
                sprintf(str, "Dist: %.2lf cm", distancia);
                printf(str);

                gfx_clear_buffer(&disp);
                gfx_draw_string(&disp, 0, 0, 1, str);

                int size_bar = 15+(distancia*112)/100;
                gfx_draw_line(&disp, 15, 27, size_bar, 27);
                
                gfx_show(&disp);
                vTaskDelay(pdMS_TO_TICKS(150));
            } else {
                gfx_clear_buffer(&disp);
                gfx_draw_string(&disp, 0, 0, 2, "falha");
                gfx_show(&disp);
                vTaskDelay(pdMS_TO_TICKS(150));
            }
        }
    }
}
int main() {
    stdio_init_all();

    xQueueDistance = xQueueCreate(32, sizeof(double));
    xSemaphore_triger = xSemaphoreCreateBinary();
    xQueueTime = xQueueCreate(32, sizeof(int));

    //xTaskCreate(oled1_demo_1, "Demo 1", 4095, NULL, 1, NULL);
    xTaskCreate(triger_task, "Triger_task", 4095, NULL, 1, NULL);
    xTaskCreate(oled_task, "Oled_task", 4095, NULL, 1, NULL);
    xTaskCreate(echo_task, "Echo_task", 4095, NULL, 1, NULL);

    vTaskStartScheduler();

    while (true)
        ;
}
