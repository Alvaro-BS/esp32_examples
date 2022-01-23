//ESTO ES UNA PRUEBA
#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"

#define LED1_GPIO CONFIG_LED1_GPIO
#define LED2_GPIO CONFIG_LED2_GPIO

//function of the task A
void taskA (void *arg){

    while(1){
        printf("I am A task\n");
        //Delay of 1 second
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

//function of the task B
void taskB (void *arg){

    while(1){
        printf("I am B task\n");
        //Delay of 500 miliseconds
        vTaskDelay(500/ portTICK_PERIOD_MS);
    }
}

void app_main(void)
{
    /*Create the task

        xTaskCreate(function of the task,
                    name of the task,
                    number of words in the task stack,
                    parameters,
                    priority,
                    task handle [NULL if I not going to use it])
    */
    xTaskCreate(taskA, "task A", 2048, NULL, 10, NULL);
    xTaskCreate(taskB, "task B", 2048, NULL, 12, NULL);
    
}
