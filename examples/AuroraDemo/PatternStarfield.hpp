#ifndef PatternStarfield_H
#define PatternStarfield_H

struct Star
{
    Star() {
        x = y = z = 0;
        colour = CRGB::White;
    }

    float x;
    float y;
    float z;
    CRGB colour;    
};


// Based on https://github.com/sinoia/oled-starfield/blob/master/src/starfield.cpp
class PatternStarfield : public Drawable {

  private:

    const int starCount = 100; // number of stars in the star field
    const int maxDepth  = 32;   // maximum distance away for a star

    // the star field - starCount stars represented as x, y and z co-ordinates
    // https://www.cplusplus.com/doc/tutorial/dynamic/
    Star * stars;
    //CRGBPalette16 currentPalette;

    unsigned int drawFrame() { // aka drawStars

        // Dim routine
        
		for (int16_t i = 0; i < VPANEL_W; i++) {
			for (int16_t j = 0; j < VPANEL_H; j++) {

                    uint16_t xy = XY16(i, j);
                    effects.leds[xy].nscale8(250);
			}
		}        

        int origin_x = VPANEL_W / 2;
        int origin_y = VPANEL_H / 2;

        // Iterate through the stars reducing the z co-ordinate in order to move the
        // star closer.
        for (int i = 0; i < starCount; ++i) {
            stars[i].z -= 0.1;
            // if the star has moved past the screen (z < 0) reposition it far away
            // with random x and y positions.
            if (stars[i].z <= 0) 
            {
                stars[i].x = getRandom(-25, 25);
                stars[i].y = getRandom(-25, 25);
                stars[i].z = maxDepth;                
            }

            // Convert the 3D coordinates to 2D using perspective projection.
            float k = VPANEL_W / stars[i].z;
            int x = static_cast<int>(stars[i].x * k + origin_x);
            int y = static_cast<int>(stars[i].y * k + origin_y);

            //  Draw the star (if it is visible in the screen).
            // Distant stars are smaller than closer stars.
            if ((0 <= x and x < VPANEL_H) 
                and (0 <= y and y < VPANEL_H)) {

                CRGB tmp = stars[i].colour;
                //CRGB tmp = CRGB::White;
                byte scale = 255 -(stars[i].z*7);
                tmp.nscale8(scale);

                effects.setPixel(x,y, CRGB(tmp.r,tmp.g,tmp.b));
            }
            else
            {
                stars[i].z = -1; // set to -1 so it gets re-popualted
            }
        }

        effects.ShowFrame();        

        return 5;
    }  
    
    int getRandom(int lower, int upper) {
        /* Generate and return a  random number between lower and upper bound */
        return lower + static_cast<int>(rand() % (upper - lower + 1));
    }

/*
    CRGB ColorFromCurrentPalette(uint8_t index = 0, uint8_t brightness = 255, TBlendType blendType = LINEARBLEND) {
      return ColorFromPalette(currentPalette, index, brightness, blendType);
    }
*/    

  public:
    PatternStarfield() 
    {
        name = (char *)"PatternStarfield";
    }

    void start() {
   
        //currentPalette = RainbowColors_p;
        //currentPalette = CloudColors_p;

        // Allocate memory
        stars = new Star[starCount];

        // Initialise the star field with random stars
        for (int i = 0; i < starCount; i++) {
            stars[i].x = getRandom(-25, 25);
            stars[i].y = getRandom(-25, 25);
            stars[i].z = getRandom(0, maxDepth);
            //stars[i].colour = ColorFromCurrentPalette(random(0, 128));
            stars[i].colour = effects.ColorFromCurrentPalette(random(0, 128));
        }
    } // end start

    void stop() {
        delete[] stars;       
    }



};

#endif
