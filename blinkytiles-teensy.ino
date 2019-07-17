#include <DmxSimple.h>
#include <FastLED.h>
#include <SoftwareSerial.h>

#if FASTLED_VERSION < 3001000
#error "Requires FastLED 3.1 or later; check github for latest code."
#endif

SoftwareSerial HWSERIAL(6,7); // RX, TX

#define LED_PIN     3
#define NUM_LEDS    7
#define LED_TYPE    DMXSIMPLE
#define COLOR_ORDER BGR
#define BRIGHTNESS  200

CRGB leds[NUM_LEDS];

enum mode {modeTwinkle, modeConfetti, modeRainbow, modeRainbowWithGlitter, modeFade, modeHighChaos, modeMidChaos, modeLowChaos};

// set the default starting LED program
mode currentMode = modeHighChaos;

uint8_t framesPerSecond = 12;  // 12 for start with sinelon + random leds, 30 for Sinelon, 60 normally for rest
uint8_t newHue;
uint8_t gHue = 160; // rotating "base color" used by many of the patterns


// SINELON //
//
CRGBPalette16 currentPalette;
CRGBPalette16 targetPalette;
TBlendType    currentBlending;                                // NOBLEND or LINEARBLEND


// Define variables used by the sequences.
uint8_t thisfade =  30;                                       // How quickly does it fade? Lower = slower fade rate.
uint8_t  thissat = 255;                                       // The saturation, where 255 = brilliant colours.
uint8_t  thisbri = 255;                                       // Brightness of a sequence.
int        myhue =   0;

// Base background color
CHSV BASE_COLOR(gHue, 255, 60);

// Peak color to twinkle up to
CHSV PEAK_COLOR(gHue, 255, 180);

// Currently set to brighten up a bit faster than it dims down, 
// but this can be adjusted.

// Amount to increment the color by each loop as it gets brighter:
CHSV DELTA_COLOR_UP(gHue, 255, 12);

// Amount to decrement the color by each loop as it gets dimmer:
CHSV DELTA_COLOR_DOWN(gHue, 255, 14);

// Chance of each pixel starting to brighten up.  
// 1 or 2 = a few brightening pixels at a time.
// 10 = lots of pixels brightening at a time.
uint8_t chanceOfTwinkle = 1;

// For receiving serial port messages from RPI
byte peopleCount;
boolean newData = false;
static byte startMarker = 0X3D;
static byte endMarker = 0x3E;

// SETUP
void setup() {
  delay(4000);
  FastLED.addLeds<LED_TYPE,LED_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  set_max_power_in_volts_and_milliamps(5, 500);               // FastLED Power management set at 5V, 500mA.

  Serial.begin(9600);
  HWSERIAL.begin(9600);
  Serial.println("<Arduino is ready>"); 

  currentBlending = LINEARBLEND;
  
  //  InitPixelStates();
} // setup()



void loop() {
    switch(currentMode)
    {
       case modeConfetti:
          confetti();
          break;
       case modeTwinkle:
          twinkle(leds, chanceOfTwinkle);
          break;
       // HighChaos for 0 - 1 people?
       case modeHighChaos:
          if (peopleCount == 0) {
            framesPerSecond = 25;
            sinelon3();
          } else {
            framesPerSecond = 16;
            sinelon3();
          }
          break;
       // MidChaos for 2 - 3 people?
       case modeMidChaos:
          if (peopleCount == 2) {
            framesPerSecond = 12;
            sinelon3();
          } else {
            framesPerSecond = 9;
            sinelon3();
          }
          break;
       case modeLowChaos:
          framesPerSecond = 30;
          sinelon3();
          break;
       case modeRainbow:
          rainbow();
          break;
      case modeRainbowWithGlitter:
          rainbowWithGlitter();
          break;
      case modeFade:
          fadeTowardColor( leds, NUM_LEDS, BASE_COLOR, 5);
          break;
      default:
        break;
    }

      EVERY_N_MILLISECONDS(100) {
        uint8_t maxChanges = 24; 
        nblendPaletteTowardPalette(currentPalette, targetPalette, maxChanges);   // AWESOME palette blending capability.
      }
    
      EVERY_N_SECONDS(1) {                                        // Change the target palette to a random one every 5 seconds.
        static uint8_t baseC = random8();                         // You can use this as a baseline colour if you want similar hues in the next line.
        targetPalette = CRGBPalette16(CHSV(random8(), 255, random8(128,255)), CHSV(random8(), 255, random8(128,255)), CHSV(random8(), 192, random8(128,255)), CHSV(random8(), 255, random8(128,255)));
      }

    // send the 'leds' array out to the actual LED strip
    FastLED.show();  
    // insert a delay to keep the framerate modest
    FastLED.delay(1000/framesPerSecond); 
  
    // do some periodic updates
    EVERY_N_MILLISECONDS( 20 ) { gHue++; } // slowly cycle the "base color" through the rainbow

    recvBytesWithStartEndMarkers();
    showNewData();
} // loop()

// ref: https://forum.arduino.cc/index.php?topic=396450.0
void recvBytesWithStartEndMarkers() {
    static boolean recvInProgress = false;
    byte rb;
   
    while (HWSERIAL.available() > 0 && newData == false) {
        rb = HWSERIAL.read();
        if (recvInProgress == true) {
            if (rb != endMarker) {
                peopleCount = rb;
            }
            else {
                recvInProgress = false;
                newData = true;
            }
        }
        else if (rb == startMarker) {
            recvInProgress = true;
        }
    }
} // recvBytesWithStartEndMarkers()

// REFACTOR THIS SINCE IT'S GETTING TOO BIG!
void showNewData() {
    if (newData == true) {
        Serial.println(peopleCount);
        if (peopleCount < 2) { //2
            currentMode = modeHighChaos;
        } else if (peopleCount < 4) { //4
            currentMode = modeMidChaos;
        } else if (peopleCount < 6) { //6
            currentMode = modeLowChaos;
        } else if (peopleCount < 8) { //8
          // I DON'T THINK THIS SHOULD BE THE SOLID RAINBOW HERE
            if (currentMode != modeRainbow) {
              for ( uint16_t i = 0; i < 255; i++) {
                fadeTowardRainbow( leds, NUM_LEDS, 2);
                  FastLED.show();
                  FastLED.delay(10);
              }
              currentMode = modeRainbow;
            }
//        if (peopleCount > 0) {
//          chanceOfTwinkle = 1;
//          if (currentMode != modeRainbow) {
//              for ( uint16_t i = 0; i < 255; i++) {
//                fadeTowardRainbow( leds, NUM_LEDS, 2);
//                  FastLED.show();
//                  FastLED.delay(10);
//              }
//              currentMode = modeRainbow;
//          }
//        } else if (peopleCount == 1) {
//          chanceOfTwinkle = 1;
//          currentMode = modeTwinkle;
//        } else if (peopleCount == 0) {
//          chanceOfTwinkle = 0;
//          if (currentMode != modeTwinkle) {
//              for ( uint16_t i = 0; i < 255; i++) {
//                  fadeTowardColor( leds, NUM_LEDS, BASE_COLOR, 2);
//                  FastLED.show();
//                  FastLED.delay(15);
//              }
//              currentMode = modeTwinkle;
//          }
        }
        newData = false;
    }
} // showNewData()

enum { SteadyDim, GettingBrighter, GettingDimmerAgain };
uint8_t PixelState[NUM_LEDS];

void InitPixelStates()
{
  memset( PixelState, sizeof(PixelState), SteadyDim); // initialize all the pixels to SteadyDim.
  fill_solid( leds, NUM_LEDS, BASE_COLOR);
} // InitPixelStates()

void rainbow() 
{
  // FastLED's built-in rainbow generator
  fill_rainbow( leds, NUM_LEDS, gHue, 7);
}

void rainbowWithGlitter() 
{
  // built-in FastLED rainbow, plus some random sparkly glitter
  rainbow();
  addGlitter(10);
}

void addGlitter( fract8 chanceOfGlitter) 
{
  if( random8() < chanceOfGlitter) {
    leds[ random8(NUM_LEDS) ] += CRGB::White;
  }
}
void confetti() 
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy(leds, NUM_LEDS, BRIGHTNESS*0.8);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV( gHue + random8(64), 200, 255);
}

void twinkle(CRGB tmp[NUM_LEDS], uint8_t chance)
{
  for( uint16_t i = 0; i < NUM_LEDS; i++) {
    if( PixelState[i] == SteadyDim) {
      // this pixels is currently: SteadyDim
      // so we randomly consider making it start getting brighter
      if( random8() < chance) {
        PixelState[i] = GettingBrighter;
      }
    } else if( PixelState[i] == GettingBrighter ) {
      // this pixels is currently: GettingBrighter
      // so if it's at peak color, switch it to getting dimmer again
      if( tmp[i] >= PEAK_COLOR ) {
        PixelState[i] = GettingDimmerAgain;
      } else {
        // otherwise, just keep brightening it:
        tmp[i] += DELTA_COLOR_UP;
      }
      
    } else { // getting dimmer again
      // this pixels is currently: GettingDimmerAgain
      // so if it's back to base color, switch it to steady dim
      if( tmp[i] <= BASE_COLOR ) {
        tmp[i] = BASE_COLOR; // reset to exact base color, in case we overshot
        PixelState[i] = SteadyDim;
      } else {
        // otherwise, just keep dimming it down:
        tmp[i] -= DELTA_COLOR_DOWN;
      }
    }
  }
}

void sinelon3() {                               // a colored dot sweeping back and forth, with fading trails

  fadeToBlackBy( leds, NUM_LEDS, thisfade);
  int pos;
  int correctedPos;

  if (currentMode == modeHighChaos) 
  {
    pos = random8(NUM_LEDS);
    myhue = random16(255);
    if (myhue < 100) {
      myhue = random16(160,224);
    } else {
      myhue = random16(96);
    } 
  } else if (currentMode == modeMidChaos) {
    pos = random8(NUM_LEDS);
    myhue = 0;
    myhue++;
//    if (myhue == 255) {
//      myhue = 1;
//    }
  } else if (currentMode == modeLowChaos) 
  {
    pos = beatsin16( 18, 0, NUM_LEDS-1 );
    if (pos == 0) {
      correctedPos = 1;
    } else if (pos == 1) {
      correctedPos = 0; 
    } else if (pos == 2) {
      correctedPos = 3;
    } else if (pos == 3) {
      correctedPos = 4;
    } else if (pos == 4) {
      correctedPos = 2;
    } else {
      correctedPos = pos;
    }
  }

  leds[pos] += ColorFromPalette(currentPalette, myhue, thisbri, currentBlending);
} // sinelon()



//
//void sinelon() // OG SINELON PROGRAM, THE TRUTH
//{
//  // a colored dot sweeping back and forth, with fading trails
//  fadeToBlackBy( leds, NUM_LEDS, 20);
////  int pos = beatsin16( 13, 0, NUM_LEDS-1 );
//  byte pos = beatsin16( 20, 0, NUM_LEDS-1 );
//  byte correctedPos;
//  if (pos == 0) {
//    correctedPos = 1;
//  } else if (pos == 1) {
//    correctedPos = 0; 
//  } else if (pos == 2) {
//    correctedPos = 3;
//  } else if (pos == 3) {
//    correctedPos = 4;
//  } else if (pos == 4) {
//    correctedPos = 2;
//  } else {
//    correctedPos = pos;
//  }
//  
//  leds[correctedPos] += CHSV( gHue, 255, 192);
//}

// Helper function that blends one uint8_t toward another by a given amount
void nblendU8TowardU8( uint8_t& cur, const uint8_t target, uint8_t amount)
{
  if( cur == target) return;
  
  if( cur < target ) {
    uint8_t delta = target - cur;
    delta = scale8_video( delta, amount);
    cur += delta;
  } else {
    uint8_t delta = cur - target;
    delta = scale8_video( delta, amount);
    cur -= delta;
  }
}

// Blend one CRGB color toward another CRGB color by a given amount.
// Blending is linear, and done in the RGB color space.
// This function modifies 'cur' in place.
CRGB fadeTowardColor( CRGB& cur, const CRGB& target, uint8_t amount)
{
  nblendU8TowardU8( cur.red,   target.red,   amount);
  nblendU8TowardU8( cur.green, target.green, amount);
  nblendU8TowardU8( cur.blue,  target.blue,  amount);
  return cur;
}

// Fade an entire array of CRGBs toward a given background color by a given amount
// This function modifies the pixel array in place.
void fadeTowardColor( CRGB* L, uint16_t N, const CRGB& bgColor, uint8_t fadeAmount)
{
  for( uint16_t i = 0; i < N; i++) {
    fadeTowardColor( L[i], bgColor, fadeAmount);
  }
}

void fadeTowardRainbow( CRGB* L, uint16_t N, uint8_t fadeAmount)
{
  for( uint16_t i = 0; i < N; i++) {
    CHSV hsv( gHue+(i*7), 255, 255);
    CRGB rgb;
    hsv2rgb_rainbow( hsv, rgb);
    fadeTowardColor( L[i], rgb, fadeAmount);
  }
}
