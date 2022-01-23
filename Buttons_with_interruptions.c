#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "sdkconfig.h"
#include "freertos/semphr.h"

#define LED1_GPIO 22
#define LED2_GPIO 19

#define BUTTON_4 5
#define BUTTON_5 18

#define ESP_INTR_FLAG_DEFAULT 0

int ButtonA, ButtonB;
SemaphoreHandle_t xSemaphore = NULL;

void IRAM_ATTR button_isr_handler(void *arg){

    //notify the button control with a signal
    xSemaphoreGiveFromISR(xSemaphore, NULL);
}

/*
//task to read the status of the buttons
void Button_handle (void *arg){

    while(1){
        printf("I am Button handle\n");
        ButtonA = gpio_get_level(BUTTON_4);
        ButtonB = gpio_get_level(BUTTON_5);
        //Delay of 10 miliseconds
        vTaskDelay(10/ portTICK_PERIOD_MS);
    }
}*/

//TasK to change the value of the leds
void Button_control (void *arg){

    while(1){
        
        //if the signal is actived
        if (xSemaphoreTake(xSemaphore, portMAX_DELAY) == pdTRUE){
            
            //set the level of the LEDS
            ButtonA = gpio_get_level(BUTTON_4);
            ButtonB = gpio_get_level(BUTTON_5);
            gpio_set_level(LED1_GPIO, ButtonA);
            gpio_set_level(LED2_GPIO, ButtonB);
        }
    }
}

void app_main(void)
{
    //create a binary semaphore (to used as automatic signal)
    xSemaphore = xSemaphoreCreateBinary();

    //reset the pin configuration
    gpio_reset_pin(LED1_GPIO);
    gpio_reset_pin(LED2_GPIO);
    gpio_reset_pin(BUTTON_4);
    gpio_reset_pin(BUTTON_5);

    /* Set the GPIO as output for the LEDs */
    gpio_set_direction(LED1_GPIO, GPIO_MODE_OUTPUT);
    gpio_set_direction(LED2_GPIO, GPIO_MODE_OUTPUT);
    /* Set the GPIO as input for the Buttons */
    gpio_set_direction(BUTTON_4, GPIO_MODE_INPUT);
    gpio_set_direction(BUTTON_5, GPIO_MODE_INPUT);

    //set the interruptions in the buttons
    gpio_set_intr_type(BUTTON_4, GPIO_INTR_ANYEDGE);
    gpio_set_intr_type(BUTTON_5, GPIO_INTR_ANYEDGE);

    //install the service with default configuratin
    gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);

    //set the interruption handler
    gpio_isr_handler_add(BUTTON_4,button_isr_handler,NULL);
    gpio_isr_handler_add(BUTTON_5,button_isr_handler,NULL);
    /*Create the task

        xTaskCreate(function of the task,
                    name of the task,
                    number of words in the task stack,
                    parameters,
                    priority,
                    task handle [NULL if I not going to use it])
    */
    //xTaskCreate(Button_handle, "handle", 2048, NULL, 20, NULL);
    xTaskCreate(Button_control, "control", 2048, NULL, 12, NULL);
    
}
