#ifndef _ESP32_ONE_EIGHT_SCAN_PANEL
#define _ESP32_ONE_EIGHT_SCAN_PANEL

#include <ESP32-VirtualMatrixPanel-I2S-DMA.h>

/* This class inherits all the goodness of the VirtualMatrixPanel class used to created
   large displays of chained panels, but does trickery to convert from 1/16 to 1/8
   DMA output.

   Some guidance given by looking at hzeller's code, in relation to the 'stretch factor'
   concept where these panels are really double width and half the height, from the perspective
   of the digital input required to get the 'real world' resolution and output.
   https://github.com/hzeller/rpi-rgb-led-matrix/blob/master/lib/multiplex-mappers.cc
*/
class OneEightScanPanel : public VirtualMatrixPanel
{
  public:
    using VirtualMatrixPanel::VirtualMatrixPanel; // inherit VirtualMatrixPanel's constructor(s)

  protected:
    /* Convert Real World 'VirtualMatrixPanel' co-ordinates (i.e. Real World pixel you're looking at
       on the panel or chain of panels, per the chaining configuration) to a 1/8 panels
       double 'stretched' and 'squished' coordinates which is what needs to be sent from the
       DMA buffer.

       Note: Look at the One_Eight_1_8_ScanPanel code and you'll see that the DMA buffer is setup
       as if the panel is 2 * W and 0.5 * H !
    */
    VirtualCoords getCoords(int16_t &x, int16_t &y);

}; // end class header note the ;



inline VirtualCoords OneEightScanPanel::getCoords(int16_t &x, int16_t &y) {
  VirtualMatrixPanel::getCoords(x, y); // call to base to update coords for chaining approach
  
  if ( coords.x == -1 || coords.y == -1 ) { // Co-ordinates go from 0 to X-1 remember! width() and height() are out of range!
    return coords;
  }

/*
  Serial.print("VirtualMatrixPanel Mapping ("); Serial.print(x, DEC); Serial.print(","); Serial.print(y, DEC); Serial.print(") ");
  // to
  Serial.print("to ("); Serial.print(coords.x, DEC);  Serial.print(",");  Serial.print(coords.y, DEC);   Serial.println(") ");  
 */
  if ( (y & 8) == 0) { 
    coords.x += ((coords.x / panelResX)+1)*panelResX; // 1st, 3rd 'block' of 8 rows of pixels, offset by panel width in DMA buffer
  }
  else {
    coords.x += (coords.x / panelResX)*panelResX; // 2nd, 4th 'block' of 8 rows of pixels, offset by panel width in DMA buffer
  }

  // http://cpp.sh/4ak5u
  // Real number of DMA y rows is half reality
  // coords.y = (y / 16)*8 + (y & 0b00000111);   
  coords.y = (y >> 4)*8 + (y & 0b00000111);   

 /*
  Serial.print("OneEightScanPanel Mapping ("); Serial.print(x, DEC); Serial.print(","); Serial.print(y, DEC); Serial.print(") ");
  // to
  Serial.print("to ("); Serial.print(coords.x, DEC);  Serial.print(",");  Serial.print(coords.y, DEC);   Serial.println(") ");
 */
 
  return coords;
}


#endif
