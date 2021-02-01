
#include <FastLED.h>

#define BAUD_RATE       115200  // serial debug port baud rate

void buffclear(CRGB *buf);
uint16_t XY16( uint16_t x, uint16_t y);
void mxfill(CRGB *leds);