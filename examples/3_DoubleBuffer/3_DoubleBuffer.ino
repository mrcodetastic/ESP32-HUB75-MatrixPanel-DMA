// Example uses the following configuration:  mxconfig.double_buff = true;
// to enable double buffering, which means display->flipDMABuffer(); is required.

// Bounce squares around the screen, doing the re-drawing in the background back-buffer.
// Double buffering is not always required in reality.

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
  //mxconfig.clkphase = false;

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
  
  display->flipDMABuffer(); // Show the back buffer, set currently output buffer to the back (i.e. no longer being sent to LED panels)
  display->clearScreen();   // Now clear the back-buffer

  delay(16);  // <----------- Shouldn't see this clearscreen occur as it happens on the back buffer when double buffering is enabled.

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
