// Code for Smartmatrix Tile 


#ifdef ESP32
#include <WiFi.h> // ugly to add here, but later causes error about param count to swap method
#endif

#define DISABLE_MATRIX_TEST // No grey on start
#define SMARTMATRIX
#include "neomatrix_config.h"


#include<FastLED.h>

//---LED SETUP STUFF

#define COLOR_ORDER BGR

// the size of your matrix - now defined in neomatrix_config
#define kMatrixWidth  mw
#define kMatrixHeight mh

// used in FillNoise for central zooming
byte CentreX =  (kMatrixWidth / 2) - 1;
byte CentreY = (kMatrixHeight / 2) - 1;

#define NUM_LEDS (kMatrixWidth * kMatrixHeight)
#define leds matrixleds

// a place to store the color palette
CRGBPalette16 currentPalette;
TBlendType    currentBlending;

// can be used for palette rotation
// "colorshift"
byte colorshift;

// The coordinates for 3 16-bit noise spaces.
#define NUM_LAYERS 3

uint32_t x[NUM_LAYERS];
uint32_t y[NUM_LAYERS];
uint32_t z[NUM_LAYERS];
uint32_t scale_x[NUM_LAYERS];
uint32_t scale_y[NUM_LAYERS];

// used for the random based animations
int16_t dx;
int16_t dy;
int16_t dz;
int16_t dsx;
int16_t dsy;

// a 3dimensional array used to store the calculated
// values of the different noise planes
uint8_t noise[NUM_LAYERS][kMatrixWidth][kMatrixHeight];

// used for the color histogramm
uint16_t values[256];

uint8_t noisesmoothing;

// polar

#define num_x  kMatrixWidth                       // how many LEDs are in one row?
#define num_y  kMatrixHeight                      // how many rows?
float polar_theta[num_x][num_y];        // look-up table for polar angles
float distance[num_x][num_y];           // look-up table for polar distances

unsigned long a, b, c;                  // for time measurements

struct render_parameters {

  float center_x = (num_x / 2) - 0.5;   // center of the matrix
  float center_y = (num_y / 2) - 0.5;
  float dist, angle;                
  float scale_x = 0.1;                  // smaller values = zoom in
  float scale_y = 0.1;
  float scale_z = 0.1;       
  float offset_x, offset_y, offset_z;     
  float z;  
  float low_limit  = 0;                 // getting contrast by highering the black point
  float high_limit = 1;                                            
};

render_parameters animation;     // all animation parameters in one place

#define num_oscillators 10

struct oscillators {

  float master_speed;            // global transition speed
  float offset[num_oscillators]; // oscillators can be shifted by a time offset
  float ratio[num_oscillators];  // speed ratios for the individual oscillators                                  
};

oscillators timings;             // all speed settings in one place

struct modulators {  

  float linear[num_oscillators];        // returns 0 to FLT_MAX
  float radial[num_oscillators];        // returns 0 to 2*PI
  float directional[num_oscillators];   // returns -1 to 1
  float noise_angle[num_oscillators];   // returns 0 to 2*PI        
};

modulators move;                 // all oscillator based movers and shifters at one place

struct rgb {

  float red, green, blue;
};

rgb pixel;


// everything for the button + menu handling
int button1;
int button2;
int button3;
byte mode;
int pgm = 0;
byte spd;
byte brightness;
byte red_level;
byte green_level;
byte blue_level;

int BRIGHTNESS = 250;
int SPEED = 30;
int FADE = 50;

// storage of the 7 10Bit (0-1023) audio band values
int left[7];
int right[7];

#if defined(CORE_TEENSY)
#ifdef TEENSY4
#include "control_null.h" // Teensy 4.0 - no input
#else 
#include "control_tdmx.h" // DMX and MSGEQ7 with Teensy 3.2
#endif
#else
#include "control_esp.h" // ESP32/ESP8266 - E1.31 and audio from WLED sender
#endif

#ifndef ESP32
#include "stars.h" // needs too much ram for ESP32
#endif

// basically beatsin16 with an additional phase

uint16_t beatsin(accum88 beats_per_minute, uint16_t lowest = 0, uint16_t highest = 65535, byte phase = 0)
{
  uint16_t beat = beat16( beats_per_minute);
  uint16_t beatsin = (sin16( beat + (phase * 256)) + 32768);
  uint16_t rangewidth = highest - lowest;
  uint16_t scaledbeat = scale16( beatsin, rangewidth);
  uint16_t result = lowest + scaledbeat;
  return result;
}

typedef void (*Pattern)();
typedef Pattern PatternList[];
typedef struct {
  Pattern pattern;
  String name;
} PatternAndName;
typedef PatternAndName PatternAndNameList[];

PatternAndNameList gPatterns = {
  { autoRun, "autoRun"}, // must be first
#ifndef ESP32
  { showStars, "showStars"},
#endif
  { Yves, "Yves" },
  { Scaledemo1, "Scaledemo1" },
  { Lava1, "Lava1" },
  { CaleidoP3, "CaleidoP3" },
  { CaleidoP2, "CaleidoP2" },
  { CaleidoP1, "CaleidoP1" },
  { Distance_Experiment, "Distance_Experiment" },
  { Center_Field, "Center_Field" },
  { Waves, "Waves" },
  { Chasing_Spirals, "Chasing_Spirals" },
  { Rotating_Blob, "Rotating_Blob" },
  { Rings, "Rings" },
#ifndef TEENSY4
  { EQ, "EQ"}, 
  { VU, "VU"},
  { FunkyPlank, "FunkyPlank"},
  { DJLight, "DJLight"},
#endif
  { MirroredNoise, "MirroredNoise"},
//  RedClouds,
//  Lavalamp1,
  { Lavalamp2, "Lavalamp2"},
//  { Lavalamp3, "Lavalamp3"}, //just white and green
  { Lavalamp4, "Lavalamp4"},
  { Lavalamp5, "Lavalamp5"},
  { Constrained1, "Constrained1"},
  { RelativeMotion1, "RelativeMotion1"},
  { Water, "Water"},
  //    Bubbles,
  { TripleMotion, "TripleMotion"},
  { CrossNoise, "CrossNoise"},
  { CrossNoise2, "CrossNoise2"},
  { RandomAnimation, "RandomAnimation"},
  { MilliTimer, "MilliTimer"},
  { Caleido1, "Caleido1"},
  { Caleido2, "Caleido2"},
//  { Caleido3, "Caleido3"}, just black on ESP/Smartmatrix
  { Caleido5, "Caleido5"},
  { vortex, "vortex"},
  { squares, "squares"},

#ifndef TEENSY4
  //      // Audio
  { MSGEQtest, "MSGEQtest"},
  { MSGEQtest2, "MSGEQtest2"},
  //   MSGEQtest3,
  { MSGEQtest4, "MSGEQtest4"},
  // AudioSpiral, // TODO: resize
  //     MSGEQtest5,// TODO: resize
  //     MSGEQtest6, //boring
  { MSGEQtest7, "MSGEQtest7"}, // nice but resize?
  { MSGEQtest8, "MSGEQtest8"},
  //   MSGEQtest9,
  //     CopyTest,
  { Audio1, "Audio1"},// TODO: resize
  { Audio2, "Audio2"}, // cool wave - move
  //     Audio3, // boring
  //     Audio4,// TODO: move
  //   CaleidoTest1,
  //  CaleidoTest2,// TODO: move
  { Audio5, "Audio5"}, // cool wave - move
  //     Audio6,
#endif
};

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))
int gPatternCount = ARRAY_SIZE(gPatterns);
int autopgm = 1;// random(1, (gPatternCount - 1));

void setup() {
  // enable debugging info output
  Serial.begin(115200);
  delay(1000);
  Serial.println("Setup");
  delay(1000);
  controlSetup();
  delay(1000);
  Serial.println("matrix_setup");
  delay(500);
  matrix_setup(false);

  ledtest();
  
//  // Initialize our noise coordinates to some random values
//  fx = random16();
//  fy = random16();
//  fz = random16();
//
//  x2 = random16();
//  y2 = random16();
//  z2 = random16();


  render_polar_lookup_table((num_x / 2) - 0.5, (num_y / 2) - 0.5);          // precalculate all polar coordinates 
                                                                            // polar origin is set to matrix centre
  
  Serial.println("\n\nEnd of setup");
  Serial.printf("There are %u patterns\n", gPatternCount);
}

void loop() {

  controlLoop();

  EVERY_N_SECONDS(10) {
    if(pgm != 0) {
      Serial.println(gPatterns[pgm].name);
    }
    else {
      Serial.print("Auto: ");
      Serial.println(gPatterns[autopgm].name);
    }
  }
  gPatterns[pgm].pattern();
  ShowFrame();
}

void autoRun() {
  EVERY_N_SECONDS(90) {
//    autopgm = random(1, (gPatternCount - 1));
     autopgm++;
    if (autopgm >= gPatternCount) autopgm = 1;
    Serial.print("Next Auto pattern: ");
    Serial.println(gPatterns[autopgm].name);
  }

  gPatterns[autopgm].pattern();

}
