//ALVARO BORGES SUAREZ
//Digital piano with the ESP32 using freeRTOS

#include <stdio.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include "sdkconfig.h"
#include <math.h>
#include <list>
#include <stdio.h>
#include "Codecs/ES8388/ES8388.h"
#include "Runnable.h"
#include "BlockingFifo.h"
#include "driver/gpio.h"

/*NOTES*/
#define SI 493.883
#define LA 440.0
#define SOL 391.995
#define MI 329.628
#define FA 349.228

using namespace mia::rtos;

//define the button gpios
#define BUTTON_5 (gpio_num_t) 18
#define BUTTON_6 (gpio_num_t) 5
#define BUTTON_4 (gpio_num_t) 23
#define BUTTON_2 (gpio_num_t) 13
#define BUTTON_3 (gpio_num_t) 19
//Defect value of the interruption flag 
#define ESP_INTR_FLAG_DEFAULT 0

//Global variables
float freq_note = 0.0f;               //frequence of the note
bool any_button_pressed = false;      //true if any button is pressed, false if not

//Event of the button (comunicate if any button is pressed)
Event eventButton;
Event eventToneGeneratorWake;
//mutex to protect the global variables
Mutex mutexButton;

//Interruption handler (only one for all the buttons)
void IRAM_ATTR button_isr_handler(void *arg){
    //comunicate that one button is pressed
    eventButton.Signal();
}

//class to fill a buffer with one tone
class Tone{
    private:
        float sample_freq;
        float tone_freq;
        float time = 0.0;
    public:
        //constructor (set the sample frequency)
        Tone(float sample_f){
            sample_freq = sample_f;
        }

        //set the frecuency of the note
        void set_Note(float tone_f){
            tone_freq = tone_f;
        }
    
        int fill(int16_t* buff, int samples){
            for(int i = 0; i < samples; ++i){
                float step = (2.0f*M_PI*(time*tone_freq))/sample_freq;
                buff[i*2] = (int)(4095.0f*sin(step));
                buff[i*2+1] = buff[i*2];
                /******/
                time = time + 1.0f;
                if(time > sample_freq/tone_freq){
                    time = 0.0f;
                }
            }
        return samples; 
        }
};

typedef struct s_frame
{
    int16_t *samples;
    size_t len;
} frame_t;

//task to player the tones on the earphones
class Player : public Runnable
{
private:
    static const int STACK_SIZE = 10 * 1024; //10 kbytes stack size
    ES8388 *codec = NULL;
    BlockingFifo<frame_t> data;
    Mutex mutex;

public:
    Player(ES8388 *codec) : Runnable("Player", 2, STACK_SIZE, eCore::CORE0)
    {
        this->codec = codec;
    }

public:
    void Play(const frame_t &&frame)
    {
        AutoMutexLock lock(&mutex);
        data.Push(frame);
    }

protected:

    bool Run() override
    {
        frame_t frame;
        if (data.Pop(&frame, INFINITE))
        {
            if (frame.samples != NULL && frame.len > 0)
            {
                size_t write;
                codec->writeBlock(frame.samples, frame.len, &write);
                delete[] frame.samples;
            }
        }
        return true;
    }
};


//task to generate the notes
class ToneGenerator: public Runnable
{
private:
    static const int STACK_SIZE = 10 * 1024; //10 kbytes stack size
    static const int FRAME_SIZE = 512;
    Player *player = NULL;
    int16_t *sample = NULL;
    Tone *tone = NULL;

public:
    ToneGenerator(Player *player): 
    Runnable("ToneGenerator", 2, STACK_SIZE, eCore::CORE0)
    {
        this->player = player;
        this->sample = new int16_t[FRAME_SIZE];
        this->tone = new Tone(44100);
    }

protected:
    bool Run() override
    {
        //mutex to read the global variables
        mutexButton.Wait();

        //only wait for a signal of wake up, if none of the buttons are pressed  
        if(any_button_pressed == false){
            //release the mutex to avoid a interblocking situation
            mutexButton.Release();
            //wait a signal to wake up the Tone Generator task
            eventToneGeneratorWake.Wait();
            //get the mutex again to work with the freq_note variable
            mutexButton.Wait();
        }

            printf("I am ToneGenerator\n");      //print to check that the tone Generator task only work, when you press any button  
            tone->set_Note(freq_note);           //set the note
            tone->fill(sample, FRAME_SIZE/2);    //fill the buffer
            player->Play({ sample, FRAME_SIZE}); //play the note
            sample = new int16_t[FRAME_SIZE];    //reset the buffer
        
        mutexButton.Release();
        return true;
    }
};

//task to handle the buttons
class Button_control: public Runnable
{
private:
    static const int STACK_SIZE = 10 * 1024; //10 kbytes stack size
    static const int FRAME_SIZE = 512;

public:
    //the button task have the maximum priority. We want that the user 
    Button_control(): 
    Runnable("Button_control", 3, STACK_SIZE, eCore::CORE0)
    {
    }

protected:
    bool Run() override
    {
        //Wait for a event of the interruption (automatic event)
        eventButton.Wait();
        //Mutex to protect the global variables
        mutexButton.Wait();

        any_button_pressed = true; //by default is true

        //The buttons have priorities. 
        if (gpio_get_level(BUTTON_6) == 0){
            freq_note = FA;
            printf("Set FA note\n");
        }
        else if (gpio_get_level(BUTTON_5) == 0){
            freq_note = MI;
            printf("Set MI note\n");
        }
        else if (gpio_get_level(BUTTON_4) == 0){
            freq_note = SOL;
            printf("Set SOL note\n");
        }
        else if (gpio_get_level(BUTTON_3) == 0){
            freq_note = LA;
            printf("Set LA note\n");
        }
        else if (gpio_get_level(BUTTON_2) == 0){
            freq_note = SI;
            printf("Set SI note\n");
        }
        else{
            any_button_pressed = false;
        }

        //if any button is pressed, wake up the Tone Generator task 
        if(any_button_pressed == true){
            eventToneGeneratorWake.Signal();
        }
        //release the mutex
        mutexButton.Release();
        return true;
    }
};

extern "C"
{

#define I2C_CLK 32
#define I2S_DATA 33

#define I2S_DIN_PIN 35
#define I2S_DOUT_PIN 26
#define I2S_LRCLK_PIN 25
#define I2S_BCK_PIN 27
#define I2S_MCLK_PIN 0
#define FREQ 44100
#define PIN_PLAY (23)    // KEY 4
#define PIN_VOL_UP (18)  // KEY 5
#define PIN_VOL_DOWN (5) // KEY 6
#define GPIO_PA_EN (21)

    void app_main(void)
    {
        ES8388 codec;
        codec.Setup(FREQ,
                    I2C_CLK, I2S_DATA,
                    I2S_BCK_PIN, I2S_LRCLK_PIN, I2S_MCLK_PIN,
                    I2S_DOUT_PIN, I2S_DIN_PIN,
                    GPIO_PA_EN, I2S_NUM_0);

        codec.SetDACOutput(ES8388::DACOutput_t::DAC_OUTPUT_ALL, true);
        codec.SetADCInput(ES8388::ADCInput_t::ADC_INPUT_LINPUT2_RINPUT2);
        codec.SetGain(ES8388::Mode_t::MODE_ADC_DAC, 0, 0);
        codec.SetDACVolume(100);
        codec.SetMicGain(ES8388::MicGain_t::MIC_GAIN_6DB);
        codec.Start(ES8388::Mode_t::MODE_ADC_DAC);
        codec.SetPAPower(false);

        //reset the pin configuration
        gpio_reset_pin(BUTTON_5);
        gpio_reset_pin(BUTTON_6);
        gpio_reset_pin(BUTTON_2);
        gpio_reset_pin(BUTTON_4);
        gpio_reset_pin(BUTTON_3);

        /* Set the GPIO as input for the Button */
        gpio_set_direction(BUTTON_5, GPIO_MODE_INPUT);
        gpio_set_direction(BUTTON_6, GPIO_MODE_INPUT);
        gpio_set_direction(BUTTON_2, GPIO_MODE_INPUT);
        gpio_set_direction(BUTTON_4, GPIO_MODE_INPUT);
        gpio_set_direction(BUTTON_3, GPIO_MODE_INPUT);

        //set the interruptions in the buttons
        gpio_set_intr_type(BUTTON_5, GPIO_INTR_ANYEDGE);
        gpio_set_intr_type(BUTTON_6, GPIO_INTR_ANYEDGE);
        gpio_set_intr_type(BUTTON_2, GPIO_INTR_ANYEDGE);
        gpio_set_intr_type(BUTTON_4, GPIO_INTR_ANYEDGE);
        gpio_set_intr_type(BUTTON_3, GPIO_INTR_ANYEDGE);

        //install the service with default configuratin
        gpio_install_isr_service(ESP_INTR_FLAG_DEFAULT);

        //set the interruption handler
        gpio_isr_handler_add(BUTTON_5,button_isr_handler,NULL);
        gpio_isr_handler_add(BUTTON_6,button_isr_handler,NULL);
        gpio_isr_handler_add(BUTTON_2,button_isr_handler,NULL);
        gpio_isr_handler_add(BUTTON_4,button_isr_handler,NULL);
        gpio_isr_handler_add(BUTTON_3,button_isr_handler,NULL);

        //instantiate the tasks
        Player player(&codec);
        Button_control button_control;
        ToneGenerator toneGenerator(&player);

        //start the tasks
        player.Start();
        button_control.Start();
        toneGenerator.Start();

        while (true) {  
        }
    }
}