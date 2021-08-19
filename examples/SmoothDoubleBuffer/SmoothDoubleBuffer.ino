#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>

MatrixPanel_I2S_DMA *display = nullptr;

const byte row0 = 2+0*11;
const byte row1 = 2+1*11;
const byte row2 = 2+2*11;

int start_x   = 0;
int buffer_id = 0;


void setup() 
{
  // put your setup code here, to run once:
  delay(1000); 
  Serial.begin(115200);
  delay(200);

  Serial.println("...Starting Display");
  HUB75_I2S_CFG mxconfig;
  mxconfig.double_buff = true; // Turn of double buffer
  mxconfig.clkphase = true;

  // OK, now we can create our matrix object
  display = new MatrixPanel_I2S_DMA(mxconfig);  
  
  display->begin();  // setup display with pins as pre-defined in the library
  
  start_x = display->width();
}
/*
 This example draws a red square on one buffer only, and a green square on another buffer only.
 It then flips between buffers as fast as possible whilst moving them as well - to give the impression they're both on screen at the same time!
 
 The only thing that's painted to both buffers is a blue square.
 */
const int square_size = 16;
void loop() 
{   
  display->flipDMABuffer();    
  //if ( !display->backbuffready() ) return;
  //display->showDMABuffer();        
  display->clearScreen();  
    
  buffer_id ^= 1;

  // Blue square on the left is printed to BOTH buffers.
  display->fillRect(0, 0, square_size/2, square_size/2, display->color565(0,0,200));

  start_x--;

  if (buffer_id)
  {
    display->setCursor(3, row1);    
    display->setTextColor(display->color565(200, 0, 0));      
    display->fillRect(start_x, 6, square_size, square_size, display->color565(200,0,0));
    //delay(40); // simulate slow drawing operation
  } 
  else 
  {
    display->setCursor(3, row2);    
    display->setTextColor(display->color565(0, 200, 0));          
    display->fillRect(10, start_x, square_size, square_size, display->color565(0,200,0));    
  }
  
  display->printf("Buffer %d", buffer_id);
 
  if (start_x < (-1*square_size)) start_x = display->width()+square_size;
  
}
