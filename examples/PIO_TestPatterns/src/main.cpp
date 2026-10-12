// How to use this library with a FM6126 panel, thanks goes to:
// https://github.com/hzeller/rpi-rgb-led-matrix/issues/746

#include <Arduino.h>
#include "xtensa/core-macros.h"
#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include "main.h"

// ==================== CONFIGURATION ====================
#define PIN_E 32
#define PANEL_WIDTH 64
#define PANEL_HEIGHT 64         
#define PANELS_NUMBER 2         

#define PANE_WIDTH (PANEL_WIDTH * PANELS_NUMBER)
#define PANE_HEIGHT PANEL_HEIGHT
#define NUM_LEDS (PANE_WIDTH * PANE_HEIGHT)
#define PATTERN_DELAY 2000

// ==================== GLOBALS ====================
MatrixPanel_I2S_DMA *matrix = nullptr;
CRGB *ledbuff = nullptr;
uint16_t cycles = 0;
const char *str = "* ESP32 I2S DMA *";

// ==================== FUNCTION PROTOTYPES ====================
void buffclear(CRGB *buf);
void mxfill(CRGB *leds);
void drawText(int colorWheelOffset);
uint16_t colorWheel(uint8_t pos);
void runSolidColorTests();
void runBufferAndFillTests();
void runLineBenchmarks();

// ==================== SETUP ====================
void setup() {
    Serial.begin(BAUD_RATE);
    Serial.println("Starting pattern test...");

    HUB75_I2S_CFG mxconfig(PANEL_WIDTH, PANEL_HEIGHT, PANELS_NUMBER);
    mxconfig.gpio.e = PIN_E;
    mxconfig.driver = HUB75_I2S_CFG::FM6126A;   

    matrix = new MatrixPanel_I2S_DMA(mxconfig);
    matrix->begin();
    matrix->setBrightness8(255);

    ledbuff = (CRGB *)malloc(NUM_LEDS * sizeof(CRGB));  
    buffclear(ledbuff);
}

// ==================== MAIN LOOP ====================
uint8_t wheelval = 0;
void loop() {
    Serial.printf("\n--- Cycle: %d ---\n", ++cycles);

#ifndef NO_GFX
    drawText(wheelval++);
#endif

    runSolidColorTests();
    runBufferAndFillTests();
    runLineBenchmarks();

    Serial.println("==== Cycle Complete, resting... ====\n");
    delay(10000);
}

// ==================== TEST MODULES ====================
void runSolidColorTests() {
    Serial.println("Estimating clearScreen() ...");
    uint32_t ccount = XTHAL_GET_CCOUNT();
    matrix->clearScreen();
    ccount = XTHAL_GET_CCOUNT() - ccount;
    Serial.printf("  clearScreen() took %d ticks\n", ccount);
    delay(PATTERN_DELAY);

    Serial.println("Fill screen: RED");
    matrix->fillScreenRGB888(255, 0, 0);
    delay(PATTERN_DELAY);
    
    Serial.println("Fill screen: GREEN");
    matrix->fillScreenRGB888(0, 255, 0);
    delay(PATTERN_DELAY);
    
    Serial.println("Fill screen: BLUE");
    matrix->fillScreenRGB888(0, 0, 255);
    delay(PATTERN_DELAY);
}

void runBufferAndFillTests() {
    // Clearing CRGB ledbuff
    Serial.print("Estimating ledbuff clear time: ");
    unsigned long t1 = micros();
    buffclear(ledbuff);
    unsigned long t2 = micros() - t1;
    Serial.printf("%lu us\n", t2);

    // Create random color gradient in ledbuff
    uint8_t r_val = 0;
    uint8_t g_val = random8();
    uint8_t b_val = 0;

    for (uint16_t i = 0; i < NUM_LEDS; ++i) {
        ledbuff[i].r = r_val++;
        ledbuff[i].g = g_val;
        if (i % PANE_WIDTH == 0) b_val += 255 / PANE_HEIGHT;
        ledbuff[i].b = b_val;
    }

    Serial.print("Estimating ledbuff-to-matrix fillrate: ");
    t1 = micros();
    mxfill(ledbuff);
    t2 = micros() - t1;
    Serial.printf("%lu us\n", t2);
    delay(PATTERN_DELAY);
}

void runLineBenchmarks() {
#ifndef NO_FAST_FUNCTIONS
    Serial.println("Estimating H-lines with drawFastHLine(): ");
    matrix->fillScreen(0);
    unsigned long t1 = micros();
    for (uint16_t y = 0; y < PANE_HEIGHT; y += 2) {
        matrix->drawFastHLine(0, y, PANE_WIDTH, 255, 255, 255);
    }
    unsigned long t2 = micros() - t1;
    Serial.printf("%lu us\n", t2);
    delay(PATTERN_DELAY);
#endif
}

// ==================== UTILITY FUNCTIONS ====================
void buffclear(CRGB *buf) {
    fill_solid(buf, NUM_LEDS, CRGB::Black); 
}

void IRAM_ATTR mxfill(CRGB *leds) {
    uint16_t y = PANE_HEIGHT;
    do {
        --y;
        uint16_t x = PANE_WIDTH;
        do {
            --x;
            uint16_t _pixel = y * PANE_WIDTH + x;
            matrix->drawPixelRGB888(x, y, leds[_pixel].r, leds[_pixel].g, leds[_pixel].b);
        } while(x);
    } while(y);
}

#ifndef NO_GFX
void drawText(int colorWheelOffset) {
    matrix->setTextSize(1);     
    matrix->setTextWrap(false); 
    matrix->setCursor(5, 5);    

    for (size_t w = 0; w < strlen(str); w++) {
        matrix->setTextColor(colorWheel((w * 32) + colorWheelOffset));
        matrix->print(str[w]);
    }
}
#else
void drawText(int colorWheelOffset) {}
#endif

uint16_t colorWheel(uint8_t pos) {
    if (pos < 85) {
        return matrix->color565(pos * 3, 255 - pos * 3, 0);
    } else if (pos < 170) {
        pos -= 85;
        return matrix->color565(255 - pos * 3, 0, pos * 3);
    } else {
        pos -= 170;
        return matrix->color565(0, pos * 3, 255 - pos * 3);
    }
}
