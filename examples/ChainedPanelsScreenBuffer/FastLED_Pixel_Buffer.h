#ifndef VIRTUAL_MATRIX_PANEL_FASTLED_LAYER
#define VIRTUAL_MATRIX_PANEL_FASTLED_LAYER

#include <ESP32-VirtualMatrixPanel-I2S-DMA.h>
#include <FastLED.h>

class VirtualMatrixPanel_FastLED_Pixel_Buffer : public VirtualMatrixPanel 
{
    public:
        using VirtualMatrixPanel::VirtualMatrixPanel; // perform VirtualMatrixPanel class constructor

        bool allocateMemory() // allocate memory
        {   
                // https://www.geeksforgeeks.org/how-to-declare-a-2d-array-dynamically-in-c-using-new-operator/ 
                buffer = new CRGB[virtualResX * virtualResY]; // These are defined in the underliny  
                
                if (!buffer) {  return false; }
                
                Serial.printf("Allocated %d bytes of memory for pixel buffer.\r\n", sizeof(CRGB)*((virtualResX * virtualResY)+1));
                this->clear();
                
                return true;
                
        } // end Buffer

        virtual void drawPixel(int16_t x, int16_t y, uint16_t color);           // overwrite adafruit implementation
        void drawPixel(int16_t x, int16_t y, int r, int g, int b);      // Buffer implementation        
        void drawPixel(int16_t x, int16_t y, CRGB color);               // Buffer implementation        
        CRGB getPixel(int16_t x, int16_t y);                            // Returns a pixel value from the buffer.


        void dimAll(byte value); 
        void dimRect(int16_t x, int16_t y, int16_t w, int16_t h, byte value); 
        
        void clear();
        
        void show();  //    Send buffer to physical hardware / DMA engine.
        
        // Release Memory
        ~VirtualMatrixPanel_FastLED_Pixel_Buffer(void); 
        
    protected:
        uint16_t XY16( uint16_t x, uint16_t y);

    private:
        CRGB* buffer = nullptr;
};



#endif