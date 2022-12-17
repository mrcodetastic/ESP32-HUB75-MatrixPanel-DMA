/*
  Various LED Driver chips might need some specific code for initialisation/control logic 

*/

#ifdef ARDUINO_ARCH_ESP32
  #include <Arduino.h>
#else
    #include <driver/gpio.h>
    #define LOW 0
    #define HIGH 1
#endif
#include "ESP32-HUB75-MatrixPanel-I2S-DMA.h"

#define CLK_PULSE          gpio_set_level((gpio_num_t) _cfg.gpio.clk, HIGH); gpio_set_level((gpio_num_t) _cfg.gpio.clk, LOW);

/**
 * @brief - pre-init procedures for specific led-drivers
 * this method is called before DMA/I2S setup while GPIOs
 * aint yet assigned for DMA operation
 * 
 */
void MatrixPanel_I2S_DMA::shiftDriver(const HUB75_I2S_CFG& _cfg){
    switch (_cfg.driver){
    case HUB75_I2S_CFG::ICN2038S:
    case HUB75_I2S_CFG::FM6124:
    case HUB75_I2S_CFG::FM6126A:
        fm6124init(_cfg);
        break;
    case HUB75_I2S_CFG::MBI5124:
        /* MBI5124 chips must be clocked with positive-edge, since it's LAT signal
        * resets on clock's rising edge while high
        * https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/files/5952216/5a542453754da.pdf
        */
        m_cfg.clkphase=true;
        break;
    case HUB75_I2S_CFG::SHIFTREG:
    default:
        break;
    }
}


void MatrixPanel_I2S_DMA::fm6124init(const HUB75_I2S_CFG& _cfg) {

    ESP_LOGI("LEDdrivers", "MatrixPanel_I2S_DMA - initializing FM6124 driver...");

    bool REG1[16] = {0,0,0,0,0, 1,1,1,1,1,1, 0,0,0,0,0};    // this sets global matrix brightness power
    bool REG2[16] = {0,0,0,0,0, 0,0,0,0,1,0, 0,0,0,0,0};    // a single bit enables the matrix output

    for (uint8_t _pin:{_cfg.gpio.r1, _cfg.gpio.r2, _cfg.gpio.g1, _cfg.gpio.g2, _cfg.gpio.b1, _cfg.gpio.b2, _cfg.gpio.clk, _cfg.gpio.lat, _cfg.gpio.oe}){
        gpio_set_direction((gpio_num_t) _pin, GPIO_MODE_OUTPUT);
        gpio_set_level((gpio_num_t) _pin, LOW);
    }

    gpio_set_level((gpio_num_t) _cfg.gpio.oe, HIGH); // Disable Display

    // Send Data to control register REG1
    // this sets the matrix brightness actually
    for (int l = 0; l < PIXELS_PER_ROW; l++){
        for (uint8_t _pin:{_cfg.gpio.r1, _cfg.gpio.r2, _cfg.gpio.g1, _cfg.gpio.g2, _cfg.gpio.b1, _cfg.gpio.b2})
          gpio_set_level((gpio_num_t) _pin, REG1[l%16]);   // we have 16 bits shifters and write the same value all over the matrix array

        if (l > PIXELS_PER_ROW - 12){         // pull the latch 11 clocks before the end of matrix so that REG1 starts counting to save the value
            gpio_set_level((gpio_num_t) _cfg.gpio.lat, HIGH);
        }
        CLK_PULSE
    }

    // drop the latch and save data to the REG1 all over the FM6124 chips
    gpio_set_level((gpio_num_t) _cfg.gpio.lat, LOW);

    // Send Data to control register REG2 (enable LED output)
    for (int l = 0; l < PIXELS_PER_ROW; l++){
        for (uint8_t _pin:{_cfg.gpio.r1, _cfg.gpio.r2, _cfg.gpio.g1, _cfg.gpio.g2, _cfg.gpio.b1, _cfg.gpio.b2})
          gpio_set_level((gpio_num_t) _pin, REG2[l%16]);   // we have 16 bits shifters and we write the same value all over the matrix array

        if (l > PIXELS_PER_ROW - 13){       // pull the latch 12 clocks before the end of matrix so that reg2 stars counting to save the value
            gpio_set_level((gpio_num_t) _cfg.gpio.lat, HIGH);
        }
        CLK_PULSE
    }

    // drop the latch and save data to the REG1 all over the FM6126 chips
    gpio_set_level((gpio_num_t) _cfg.gpio.lat, LOW);

    // blank data regs to keep matrix clear after manipulations
    for (uint8_t _pin:{_cfg.gpio.r1, _cfg.gpio.r2, _cfg.gpio.g1, _cfg.gpio.g2, _cfg.gpio.b1, _cfg.gpio.b2})
       gpio_set_level((gpio_num_t) _pin, LOW);

    for (int l = 0; l < PIXELS_PER_ROW; ++l){
        CLK_PULSE
    }

    gpio_set_level((gpio_num_t) _cfg.gpio.lat, HIGH);
    CLK_PULSE
    gpio_set_level((gpio_num_t) _cfg.gpio.lat, LOW);
    gpio_set_level((gpio_num_t) _cfg.gpio.oe, LOW); // Enable Display
    CLK_PULSE
}