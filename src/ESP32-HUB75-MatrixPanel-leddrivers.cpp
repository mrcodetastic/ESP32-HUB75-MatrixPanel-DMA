/*
  Initialization routines and control logic for specialized LED Driver ICs.
*/

#include <driver/gpio.h>

#ifdef ARDUINO_ARCH_ESP32
  #include <Arduino.h>
#else
  #ifndef LOW
    #define LOW 0
  #endif
  #ifndef HIGH
    #define HIGH 1
  #endif
#endif

#include "ESP32-HUB75-MatrixPanel-I2S-DMA.h"

/**
 * @brief Pulse clock pin high then low.
 */
static inline void pulseClock(gpio_num_t clkPin) noexcept {
    gpio_set_level(clkPin, 1);
    gpio_set_level(clkPin, 0);
}

/**
 * @brief Pre-initialization routines for specific driver ICs.
 * Called prior to DMA/I2S hardware setup before GPIOs are assigned to DMA peripherals.
 */
void MatrixPanel_I2S_DMA::shiftDriver(const HUB75_I2S_CFG& _cfg) {
    switch (_cfg.driver) {
    case HUB75_I2S_CFG::shift_driver::ICN2038S:
    case HUB75_I2S_CFG::shift_driver::FM6124:
    case HUB75_I2S_CFG::shift_driver::FM6126A:
        fm6124init(_cfg);
        break;
    case HUB75_I2S_CFG::shift_driver::DP3246:
        dp3246init(_cfg);
        break;
    case HUB75_I2S_CFG::shift_driver::MBI5124:
        /* MBI5124 chips require positive-edge clocking because the LAT signal
         * resets on the clock's rising edge while high.
         * Reference: https://github.com/mrfaptastic/ESP32-HUB75-MatrixPanel-I2S-DMA/files/5952216/5a542453754da.pdf
         */
        m_cfg.clkphase = true;
        break;
    case HUB75_I2S_CFG::shift_driver::SHIFTREG:
    default:
        break;
    }
}

void MatrixPanel_I2S_DMA::fm6124init(const HUB75_I2S_CFG& _cfg) {
    ESP_LOGI("LEDdrivers", "MatrixPanel_I2S_DMA - initializing FM6124 driver...");

    const bool REG1[16] = {0,0,0,0,0, 1,1,1,1,1,1, 0,0,0,0,0};    // Global matrix brightness control power setup
    const bool REG2[16] = {0,0,0,0,0, 0,0,0,0,1,0, 0,0,0,0,0};    // Matrix output enable configuration bit

    const gpio_num_t pins[] = {(gpio_num_t)_cfg.gpio.r1, (gpio_num_t)_cfg.gpio.r2, (gpio_num_t)_cfg.gpio.g1, (gpio_num_t)_cfg.gpio.g2, (gpio_num_t)_cfg.gpio.b1, (gpio_num_t)_cfg.gpio.b2, (gpio_num_t)_cfg.gpio.clk, (gpio_num_t)_cfg.gpio.lat, (gpio_num_t)_cfg.gpio.oe};
    for (gpio_num_t pin : pins) {
        gpio_reset_pin(pin);                        // Reset pin out of default state post-boot
        gpio_set_direction(pin, GPIO_MODE_OUTPUT);
        gpio_set_level(pin, LOW);
    }

    gpio_set_level((gpio_num_t)_cfg.gpio.oe, HIGH); // Disable display output during initialization

    // Send register payload to control register REG1 (global brightness)
    for (int l = 0; l < PIXELS_PER_ROW; l++) {
        for (gpio_num_t pin : {(gpio_num_t)_cfg.gpio.r1, (gpio_num_t)_cfg.gpio.r2, (gpio_num_t)_cfg.gpio.g1, (gpio_num_t)_cfg.gpio.g2, (gpio_num_t)_cfg.gpio.b1, (gpio_num_t)_cfg.gpio.b2}) {
            gpio_set_level(pin, REG1[l % 16]); // Shift 16-bit payload across matrix shifters
        }

        if (l > PIXELS_PER_ROW - 12) {         // Assert latch 11 clocks prior to row end to store REG1
            gpio_set_level((gpio_num_t)_cfg.gpio.lat, HIGH);
        }
        pulseClock((gpio_num_t)_cfg.gpio.clk);
    }

    // Release latch to commit data to REG1 across FM6124 drivers
    gpio_set_level((gpio_num_t)_cfg.gpio.lat, LOW);

    // Send register payload to control register REG2 (enable matrix LED output)
    for (int l = 0; l < PIXELS_PER_ROW; l++) {
        for (gpio_num_t pin : {(gpio_num_t)_cfg.gpio.r1, (gpio_num_t)_cfg.gpio.r2, (gpio_num_t)_cfg.gpio.g1, (gpio_num_t)_cfg.gpio.g2, (gpio_num_t)_cfg.gpio.b1, (gpio_num_t)_cfg.gpio.b2}) {
            gpio_set_level(pin, REG2[l % 16]); // Shift 16-bit payload across matrix shifters
        }

        if (l > PIXELS_PER_ROW - 13) {       // Assert latch 12 clocks prior to row end to store REG2
            gpio_set_level((gpio_num_t)_cfg.gpio.lat, HIGH);
        }
        pulseClock((gpio_num_t)_cfg.gpio.clk);
    }

    // Release latch to commit data to REG2 across FM6126 drivers
    gpio_set_level((gpio_num_t)_cfg.gpio.lat, LOW);

    // Clear data registers to blank panel output following initialization
    for (gpio_num_t pin : {(gpio_num_t)_cfg.gpio.r1, (gpio_num_t)_cfg.gpio.r2, (gpio_num_t)_cfg.gpio.g1, (gpio_num_t)_cfg.gpio.g2, (gpio_num_t)_cfg.gpio.b1, (gpio_num_t)_cfg.gpio.b2}) {
        gpio_set_level(pin, LOW);
    }

    for (int l = 0; l < PIXELS_PER_ROW; ++l) {
        pulseClock((gpio_num_t)_cfg.gpio.clk);
    }

    gpio_set_level((gpio_num_t)_cfg.gpio.lat, HIGH);
    pulseClock((gpio_num_t)_cfg.gpio.clk);
    gpio_set_level((gpio_num_t)_cfg.gpio.lat, LOW);
    gpio_set_level((gpio_num_t)_cfg.gpio.oe, LOW); // Re-enable display output
    pulseClock((gpio_num_t)_cfg.gpio.clk);
}

void MatrixPanel_I2S_DMA::dp3246init(const HUB75_I2S_CFG& _cfg) {
    ESP_LOGI("LEDdrivers", "MatrixPanel_I2S_DMA - initializing DP3246 driver...");

    // DP3246 drivers require positive clock edge configuration
    m_cfg.clkphase = true;

    // Register 1 config: OE widening, current gain settings
    
    // 15:13   3   000        reserved
    // 12:9    4   0000       OE widening (= OE_ADD * 6ns)
    // 8       1   0          reserved
    // 7:0     8   11111111   Iout = (Igain+1)/256 * 17.6 / Rext

    const bool REG1[16] = { 0,0,0, 0,0,0,0, 0, 1,1,1,1,1,1,1,1 };  // MSB first

    // Register 2 config: Blanking potential selection, dead pixel removal, fading options

    // 15:11   5   11111      Blanking potential selection, step 77mV, 00000: VDD-0.8V
    // 10:8    3   111        Constant current source output inflection point selection
    // 7       1   0          Disable dead pixel removel, 1: Enable
    // 6       1   0          0->1: (OPEN_DET rising edge) start detection, 0: reset to ready-to-detect state
    // 5       1   0          0: Enable black screen power saving, 1: Turn off the black screen to save energy
    // 4       1   0          0: Do not enable the fading function, 1: Enable the fade function
    // 3       1   0          Reserved
    // 2:0     3   000        000: single edge pass, others: double edge transfer    
    const bool REG2[16] = { 1,1,1,1,1, 1,1,1, 0, 0, 0, 0, 0, 0,0,0 };  // MSB first

    const gpio_num_t pins[] = {(gpio_num_t)_cfg.gpio.r1, (gpio_num_t)_cfg.gpio.r2, (gpio_num_t)_cfg.gpio.g1, (gpio_num_t)_cfg.gpio.g2, (gpio_num_t)_cfg.gpio.b1, (gpio_num_t)_cfg.gpio.b2, (gpio_num_t)_cfg.gpio.clk, (gpio_num_t)_cfg.gpio.lat, (gpio_num_t)_cfg.gpio.oe};
    for (gpio_num_t pin : pins) {
        gpio_reset_pin(pin);                        // Reset pin state post-boot
        gpio_set_direction(pin, GPIO_MODE_OUTPUT);
        gpio_set_level(pin, LOW);
    }

    gpio_set_level((gpio_num_t)_cfg.gpio.oe, HIGH); // Disable display output during initialization

    // Flush and reset registers to ensure startup stability
    for (int l = 0; l < PIXELS_PER_ROW; ++l) {
        if (l == PIXELS_PER_ROW - 3) {       // DP3246 requires latch held low for 3 clock cycles
            gpio_set_level((gpio_num_t)_cfg.gpio.lat, HIGH);
        }
        pulseClock((gpio_num_t)_cfg.gpio.clk);
    }

    gpio_set_level((gpio_num_t)_cfg.gpio.lat, LOW);

    // Send payload to control register REG1
    for (int l = 0; l < PIXELS_PER_ROW; l++) {
        for (gpio_num_t pin : {(gpio_num_t)_cfg.gpio.r1, (gpio_num_t)_cfg.gpio.r2, (gpio_num_t)_cfg.gpio.g1, (gpio_num_t)_cfg.gpio.g2, (gpio_num_t)_cfg.gpio.b1, (gpio_num_t)_cfg.gpio.b2}) {
            gpio_set_level(pin, REG1[l % 16]); // Shift 16-bit payload across matrix shifters
        }

        if (l == PIXELS_PER_ROW - 11) {         // Assert latch 11 clocks prior to row end to store REG1
            gpio_set_level((gpio_num_t)_cfg.gpio.lat, HIGH);
        }
        pulseClock((gpio_num_t)_cfg.gpio.clk);
    }

    // Release latch to commit data to REG1 across DP3246 chips
    gpio_set_level((gpio_num_t)_cfg.gpio.lat, LOW);

    // Send payload to control register REG2
    for (int l = 0; l < PIXELS_PER_ROW; l++) {
        for (gpio_num_t pin : {(gpio_num_t)_cfg.gpio.r1, (gpio_num_t)_cfg.gpio.r2, (gpio_num_t)_cfg.gpio.g1, (gpio_num_t)_cfg.gpio.g2, (gpio_num_t)_cfg.gpio.b1, (gpio_num_t)_cfg.gpio.b2}) {
            gpio_set_level(pin, REG2[l % 16]); // Shift 16-bit payload across matrix shifters
        }

        if (l == PIXELS_PER_ROW - 12) {       // Assert latch 12 clocks prior to row end to store REG2
            gpio_set_level((gpio_num_t)_cfg.gpio.lat, HIGH);
        }
        pulseClock((gpio_num_t)_cfg.gpio.clk);
    }

    // Release latch to commit data to REG2 across DP3246 chips
    gpio_set_level((gpio_num_t)_cfg.gpio.lat, LOW);
    pulseClock((gpio_num_t)_cfg.gpio.clk);

    // Clear data registers to blank panel output following initialization
    for (gpio_num_t pin : {(gpio_num_t)_cfg.gpio.r1, (gpio_num_t)_cfg.gpio.r2, (gpio_num_t)_cfg.gpio.g1, (gpio_num_t)_cfg.gpio.g2, (gpio_num_t)_cfg.gpio.b1, (gpio_num_t)_cfg.gpio.b2}) {
        gpio_set_level(pin, LOW);
    }

    for (int l = 0; l < PIXELS_PER_ROW; ++l) {
        if (l == PIXELS_PER_ROW - 3) {       // DP3246 requires latch held low for 3 clock cycles
            gpio_set_level((gpio_num_t)_cfg.gpio.lat, HIGH);
        }
        pulseClock((gpio_num_t)_cfg.gpio.clk);
    }

    gpio_set_level((gpio_num_t)_cfg.gpio.lat, LOW);
    gpio_set_level((gpio_num_t)_cfg.gpio.oe, LOW); // Re-enable display output
    pulseClock((gpio_num_t)_cfg.gpio.clk);
}