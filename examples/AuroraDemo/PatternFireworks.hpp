#ifndef FireWork_H
#define FireWork_H

/****************************************************************
 * Fireworks Class
 ****************************************************************/
// Ripped from: https://github.com/lmirel/MorphingClockRemix

const int FIREWORKS = 4;           // Number of fireworks
const int FIREWORK_PARTICLES = 32;  // Number of particles per firework

const float GRAVITY = 0.03f;
const float baselineSpeed = -1.2f;
const float maxSpeed = -2.0f;

class Firework
{
  public:
    float x[FIREWORK_PARTICLES];
    float y[FIREWORK_PARTICLES];
    char lx[FIREWORK_PARTICLES], ly[FIREWORK_PARTICLES];
    float xSpeed[FIREWORK_PARTICLES];
    float ySpeed[FIREWORK_PARTICLES];

    char red;
    char blue;
    char green;
    char alpha;

    int framesUntilLaunch;

    char particleSize;
    bool hasExploded;

    Firework(); // Constructor declaration
    void initialise();
    void move();
    void explode();
};

// Constructor implementation
Firework::Firework()
{
  initialise();
  for (int loop = 0; loop < FIREWORK_PARTICLES; loop++)
  {
    lx[loop] = 0;
    ly[loop] = VPANEL_H + 1; // Push the particle location down off the bottom of the screen
  }
}

void Firework::initialise()
{
    // Pick an initial x location and  random x/y speeds
    float xLoc = (rand() % VPANEL_W);
    float xSpeedVal = baselineSpeed + (rand() % (int)maxSpeed);
    float ySpeedVal = baselineSpeed + (rand() % (int)maxSpeed);

    // Set initial x/y location and speeds
    for (int loop = 0; loop < FIREWORK_PARTICLES; loop++)
    {
        x[loop] = xLoc;
        y[loop] = VPANEL_H + 1; // Push the particle location down off the bottom of the screen
        xSpeed[loop] = xSpeedVal;
        ySpeed[loop] = ySpeedVal;
        //don't reset these otherwise particles won't be removed
        //lx[loop] = 0;
        //ly[loop] = LAYER_HEIGHT + 1; // Push the particle location down off the bottom of the screen
    }

    // Assign a random colour and full alpha (i.e. particle is completely opaque)
    red   = (rand() % 255);/// (float)RAND_MAX);
    green = (rand() % 255); /// (float)RAND_MAX);
    blue  = (rand() % 255); /// (float)RAND_MAX);
    alpha = 50;//max particle frames

    // Firework will launch after a random amount of frames between 0 and 400
    framesUntilLaunch = ((int)rand() % (VPANEL_H));

    // Size of the particle (as thrown to glPointSize) - range is 1.0f to 4.0f
    particleSize = 1.0f + ((float)rand() / (float)RAND_MAX) * 3.0f;

    // Flag to keep trackof whether the firework has exploded or not
    hasExploded = false;

    //cout << "Initialised a firework." << endl;
}

void Firework::move()
{
    for (int loop = 0; loop < FIREWORK_PARTICLES; loop++)
    {
        // Once the firework is ready to launch start moving the particles
        if (framesUntilLaunch <= 0)
        {
            //draw black on last known position
            //tl->drawPixel (x[loop], y[loop], cc_blk);
            lx[loop] = x[loop];
            ly[loop] = y[loop];
            //
            x[loop] += xSpeed[loop];

            y[loop] += ySpeed[loop];

            ySpeed[loop] += GRAVITY;
        }
    }
    framesUntilLaunch--;

    // Once a fireworks speed turns positive (i.e. at top of arc) - blow it up!
    if (ySpeed[0] > 0.0f)
    {
        for (int loop2 = 0; loop2 < FIREWORK_PARTICLES; loop2++)
        {
            // Set a random x and y speed beteen -4 and + 4
            xSpeed[loop2] = -2 + (rand() / (float)RAND_MAX) * 4;
            ySpeed[loop2] = -2 + (rand() / (float)RAND_MAX) * 4;
        }

        //cout << "Boom!" << endl;
        hasExploded = true;
    }
}

void Firework::explode()
{
    for (int loop = 0; loop < FIREWORK_PARTICLES; loop++)
    {
        // Dampen the horizontal speed by 1% per frame
        xSpeed[loop] *= 0.99;

        //draw black on last known position (NO LONGER USED tl->dim used instead)
        //tl->drawPixel (x[loop], y[loop], cc_blk);
        lx[loop] = x[loop];
        ly[loop] = y[loop];
        //
        // Move the particle
        x[loop] += xSpeed[loop];
        y[loop] += ySpeed[loop];

        // Apply gravity to the particle's speed
        ySpeed[loop] += GRAVITY;
    }

    // Fade out the particles (alpha is stored per firework, not per particle)
    if (alpha > 0)
    {
        alpha -= 1;
    }
    else // Once the alpha hits zero reset the firework
    {
        initialise();
    }
}

/*********************** */



class PatternFirework : public Drawable {

  public:
    PatternFirework() {
        name = (char *)"PatternFirework";

    }

    void start();
    unsigned int drawFrame();
    void stop();

    private:
        // Create our array of fireworks
        Firework fw[FIREWORKS];

};


void PatternFirework::start() { }

void PatternFirework::stop()  { }    

unsigned int  PatternFirework::drawFrame()
{

    effects.DimAll(250);

    CRGB cc_frw;
    //display.fillScreen (0);
    // Draw fireworks
    //cout << "Firework count is: " << Firework::fireworkCount << endl;
    for (int loop = 0; loop < FIREWORKS; loop++)
    {
        for (int particleLoop = 0; particleLoop < FIREWORK_PARTICLES; particleLoop++)
        {

            // Set colour to yellow on way up, then whatever colour firework should be when exploded
            if (fw[loop].hasExploded == false)
            {
                //glColor4f(1.0f, 1.0f, 0.0f, 1.0f);
                cc_frw = CRGB (255, 255, 0);
            }
            else
            {
                //glColor4f(fw[loop].red, fw[loop].green, fw[loop].blue, fw[loop].alpha);
                //glVertex2f(fw[loop].x[particleLoop], fw[loop].y[particleLoop]);
                cc_frw = CRGB (fw[loop].red, fw[loop].green, fw[loop].blue);
            }

            // Draw the point
            //glVertex2f(fw[loop].x[particleLoop], fw[loop].y[particleLoop]);
            effects.setPixel (fw[loop].x[particleLoop], fw[loop].y[particleLoop], cc_frw);
           // effects.setPixel (fw[loop].lx[particleLoop], fw[loop].ly[particleLoop], 0);
        }
        // Move the firework appropriately depending on its explosion state
        if (fw[loop].hasExploded == false)
        {
            fw[loop].move();
        }
        else
        {
            fw[loop].explode();
        }
        //
        //delay (10);
    } // end loop

      effects.ShowFrame();

      return 20;    

} // end drawframe


#endif