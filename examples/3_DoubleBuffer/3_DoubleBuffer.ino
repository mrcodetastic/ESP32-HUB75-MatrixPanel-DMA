/**
 Example uses the following configuration:  mxconfig.double_buff = true;
 to enable double buffering, which means display->flipDMABuffer(); is required.

 Bounce squares around the screen, doing the re-drawing in the background back-buffer.

 Double buffering is not usually required. It is only useful when you have a long (in duration)
 drawing routine that you want to 'flip to' once complete without the drawing being visible to 
 the naked eye when looking at the HUB75 panel.

 Please note that double buffering isn't a silver bullet, and may still result in flickering 
 if you end up 'flipping' the buffer quicker than the physical HUB75 refresh output rate. 

 Refer to the runtime debug output to see, i.e:

[  2103][I][ESP32-HUB75-MatrixPanel-I2S-DMA.cpp:85] setupDMA(): [I2S-DMA] Minimum visual refresh rate (scan rate from panel top to bottom) requested: 60 Hz
[  2116][W][ESP32-HUB75-MatrixPanel-I2S-DMA.cpp:105] setupDMA(): [I2S-DMA] lsbMsbTransitionBit of 0 gives 57 Hz refresh rate.
[  2128][W][ESP32-HUB75-MatrixPanel-I2S-DMA.cpp:105] setupDMA(): [I2S-DMA] lsbMsbTransitionBit of 1 gives 110 Hz refresh rate.
[  2139][W][ESP32-HUB75-MatrixPanel-I2S-DMA.cpp:118] setupDMA(): [I2S-DMA] lsbMsbTransitionBit of 1 used to achieve refresh rate of 60 Hz.

**/


#include <ESP32-HUB75-MatrixPanel-I2S-DMA.h>
#include <array>

MatrixPanel_I2S_DMA *display = nullptr;

constexpr std::size_t color_num = 5;
using colour_arr_t = std::array<uint16_t, color_num>;

uint16_t myDARK, myWHITE, myRED, myGREEN, myBLUE;
colour_arr_t colours;

struct Square
{
  float xpos, ypos;
  float velocityx;
  float velocityy;
  boolean xdir, ydir;
  uint16_t square_size;
  uint16_t colour;
};

const int numSquares = 25;
Square Squares[numSquares];

void setup()
{
  // put your setup code here, to run once:
  delay(1000);
  Serial.begin(115200);
  delay(200);

  Serial.println("...Starting Display");
  HUB75_I2S_CFG mxconfig;
  mxconfig.double_buff = true; // <------------- Turn on double buffer
  //mxconfig.clkphase = false; // <------------- Turn off double buffer and it'll look flickery

  // OK, now we can create our matrix object
  display = new MatrixPanel_I2S_DMA(mxconfig);
  display->begin();  // setup display with pins as pre-defined in the library

  myDARK = display->color565(64, 64, 64);
  myWHITE = display->color565(192, 192, 192);
  myRED = display->color565(255, 0, 0);
  myGREEN = display->color565(0, 255, 0);
  myBLUE = display->color565(0, 0, 255);

  colours = {{ myDARK, myWHITE, myRED, myGREEN, myBLUE }};

  // Create some random squares
  for (int i = 0; i < numSquares; i++)
  {
    Squares[i].square_size = random(2,10);
    Squares[i].xpos = random(0, display->width() - Squares[i].square_size);
    Squares[i].ypos = random(0, display->height() - Squares[i].square_size);
    Squares[i].velocityx = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);
    Squares[i].velocityy = static_cast <float> (rand()) / static_cast <float> (RAND_MAX);

    int random_num = random(6);
    Squares[i].colour = colours[random_num];
  }
}

void loop()
{

  // Flip all future drawPixel calls to write to the back buffer which is NOT being displayed.
  display->flipDMABuffer(); 

  // SUPER IMPORTANT: Wait at least long enough to ensure that a "frame" has been displayed on the LED Matrix Panel before the next flip!
  delay(1000/display->calculated_refresh_rate);  

  // Now clear the back-buffer we are drawing to.
  display->clearScreen();   

  // This is here to demonstrate flicker if double buffering is disabled. Emulates a long draw routine that would typically occur after a 'clearscreen'.
  delay(25);
 

  for (int i = 0; i < numSquares; i++)
  {
    // Draw rect and then calculate
    display->fillRect(Squares[i].xpos, Squares[i].ypos, Squares[i].square_size, Squares[i].square_size, Squares[i].colour);

    if (Squares[i].square_size + Squares[i].xpos >= display->width()) {
      Squares[i].velocityx *= -1;
    } else if (Squares[i].xpos <= 0) {
      Squares[i].velocityx = abs (Squares[i].velocityx);
    }

    if (Squares[i].square_size + Squares[i].ypos >= display->height()) {
      Squares[i].velocityy *= -1;
    } else if (Squares[i].ypos <= 0) {
      Squares[i].velocityy = abs (Squares[i].velocityy);
    }

    Squares[i].xpos += Squares[i].velocityx;
    Squares[i].ypos += Squares[i].velocityy;
  }
}
