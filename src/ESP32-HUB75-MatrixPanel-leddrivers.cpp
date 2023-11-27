/*
  Various LED Driver chips might need some specific code for initialisation/control logic 

*/

#include <driver/gpio.h>

#ifdef ARDUINO_ARCH_ESP32
  #include <Arduino.h>
#else
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
    case HUB75_I2S_CFG::DP3246_SM5368:
        dp3246init(_cfg);
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
        gpio_reset_pin((gpio_num_t)_pin);                        // some pins are not in gpio mode after reset => https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/gpio.html#gpio-summary
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

void MatrixPanel_I2S_DMA::dp3246init(const HUB75_I2S_CFG& _cfg) {

    ESP_LOGI("LEDdrivers", "MatrixPanel_I2S_DMA - initializing DP3246 driver...");

    // DP3246 needs positive clock edge
    m_cfg.clkphase = true;

    // 15:13   3   000        reserved
    // 12:9    4   0000       OE widening (= OE_ADD * 6ns)
    // 8       1   0          reserved
    // 7:0     8   11111111   Iout = (Igain+1)/256 * 17.6 / Rext
    bool REG1[16] = { 0,0,0, 0,0,0,0, 0, 1,1,1,1,1,1,1,1 };  // MSB first

    // 15:11   5   11111      Blanking potential selection, step 77mV, 00000: VDD-0.8V
    // 10:8    3   111        Constant current source output inflection point selection
    // 7       1   0          Disable dead pixel removel, 1: Enable
    // 6       1   0          0->1: (OPEN_DET rising edge) start detection, 0: reset to ready-to-detect state
    // 5       1   0          0: Enable black screen power saving, 1: Turn off the black screen to save energy
    // 4       1   0          0: Do not enable the fading function, 1: Enable the fade function
    // 3       1   0          Reserved
    // 2:0     3   000        000: single edge pass, others: double edge transfer
    bool REG2[16] = { 1,1,1,1,1, 1,1,1, 0, 0, 0, 0, 0, 0,0,0 };  // MSB first

    for (uint8_t _pin : {_cfg.gpio.r1, _cfg.gpio.r2, _cfg.gpio.g1, _cfg.gpio.g2, _cfg.gpio.b1, _cfg.gpio.b2, _cfg.gpio.clk, _cfg.gpio.lat, _cfg.gpio.oe}) {
        gpio_reset_pin((gpio_num_t)_pin);                        // some pins are not in gpio mode after reset => https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/gpio.html#gpio-summary
        gpio_set_direction((gpio_num_t)_pin, GPIO_MODE_OUTPUT);
        gpio_set_level((gpio_num_t)_pin, LOW);
    }

    gpio_set_level((gpio_num_t)_cfg.gpio.oe, HIGH); // disable Display

    // clear registers - this seems to help with reliability
    for (int l = 0; l < PIXELS_PER_ROW; ++l) {
        if (l == PIXELS_PER_ROW - 3) {       // DP3246 wants the latch dropped for 3 clk cycles
            gpio_set_level((gpio_num_t)_cfg.gpio.lat, HIGH);
        }
        CLK_PULSE
    }

    gpio_set_level((gpio_num_t)_cfg.gpio.lat, LOW);

    // Send Data to control register REG1
    for (int l = 0; l < PIXELS_PER_ROW; l++) {
        for (uint8_t _pin : {_cfg.gpio.r1, _cfg.gpio.r2, _cfg.gpio.g1, _cfg.gpio.g2, _cfg.gpio.b1, _cfg.gpio.b2})
            gpio_set_level((gpio_num_t)_pin, REG1[l % 16]);   // we have 16 bits shifters and write the same value all over the matrix array

        if (l == PIXELS_PER_ROW - 11) {         // pull the latch 11 clocks before the end of matrix so that REG1 starts counting to save the value
            gpio_set_level((gpio_num_t)_cfg.gpio.lat, HIGH);
        }
        CLK_PULSE
    }

    // drop the latch and save data to the REG1 all over the DP3246 chips
    gpio_set_level((gpio_num_t)_cfg.gpio.lat, LOW);

    // Send Data to control register REG2
    for (int l = 0; l < PIXELS_PER_ROW; l++) {
        for (uint8_t _pin : {_cfg.gpio.r1, _cfg.gpio.r2, _cfg.gpio.g1, _cfg.gpio.g2, _cfg.gpio.b1, _cfg.gpio.b2})
            gpio_set_level((gpio_num_t)_pin, REG2[l % 16]);   // we have 16 bits shifters and we write the same value all over the matrix array

        if (l == PIXELS_PER_ROW - 12) {       // pull the latch 12 clocks before the end of matrix so that REG2 starts counting to save the value
            gpio_set_level((gpio_num_t)_cfg.gpio.lat, HIGH);
        }
        CLK_PULSE
    }

    // drop the latch and save data to the REG2 all over the DP3246 chips
    gpio_set_level((gpio_num_t)_cfg.gpio.lat, LOW);
    CLK_PULSE

    // blank data regs to keep matrix clear after manipulations
    for (uint8_t _pin : {_cfg.gpio.r1, _cfg.gpio.r2, _cfg.gpio.g1, _cfg.gpio.g2, _cfg.gpio.b1, _cfg.gpio.b2})
        gpio_set_level((gpio_num_t)_pin, LOW);

    for (int l = 0; l < PIXELS_PER_ROW; ++l) {
        if (l == PIXELS_PER_ROW - 3) {       // DP3246 wants the latch dropped for 3 clk cycles
            gpio_set_level((gpio_num_t)_cfg.gpio.lat, HIGH);
        }
        CLK_PULSE
    }

    gpio_set_level((gpio_num_t)_cfg.gpio.lat, LOW);
    gpio_set_level((gpio_num_t)_cfg.gpio.oe, LOW); // enable Display
    CLK_PULSE
}