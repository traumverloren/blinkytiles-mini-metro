//#define FASTLED_ALLOW_INTERRUPTS 0
//#define FASTLED_INTERRUPT_RETRY_COUNT 1

#include <DmxSimple.h>
#include <FastLED.h>
#include <SoftwareSerial.h>
#include <ArduinoJson.h>

#if FASTLED_VERSION < 3001000
#error "Requires FastLED 3.1 or later; check github for latest code."
#endif

SoftwareSerial HWSERIAL(6,7); // RX, TX

#define LED_PIN     3
#define NUM_LEDS    7
#define LED_TYPE    DMXSIMPLE
#define COLOR_ORDER BGR
#define BRIGHTNESS   128

CRGB leds[NUM_LEDS];

// animation blend
CRGB leds2[NUM_LEDS];
CRGB leds3[NUM_LEDS];

enum mode {modeTwinkle, modeConfetti, modeRainbow, modeRainbowWithGlitter, modeFade};

// set the default starting LED program
mode currentMode = modeTwinkle;

uint8_t framesPerSecond = 60;
uint8_t newHue;
uint8_t gHue = 0; // rotating "base color" used by many of the patterns

// Base background color
CHSV BASE_COLOR(gHue, 255, 64);

// Peak color to twinkle up to
CHSV PEAK_COLOR(gHue, 255, 192);

// Currently set to brighten up a bit faster than it dims down, 
// but this can be adjusted.

// Amount to increment the color by each loop as it gets brighter:
CHSV DELTA_COLOR_UP(gHue, 255, 12);

// Amount to decrement the color by each loop as it gets dimmer:
CHSV DELTA_COLOR_DOWN(gHue, 255, 12);

// Chance of each pixel starting to brighten up.  
// 1 or 2 = a few brightening pixels at a time.
// 10 = lots of pixels brightening at a time.
uint8_t chanceOfTwinkle = 0;

// For receiving serial port messages from RPI
byte peopleCount;
boolean newData = false;
static byte startMarker = 0x3C;
static byte endMarker = 0x3E;

void setup() {
  delay(3000);
  FastLED.addLeds<LED_TYPE,LED_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  
  Serial.begin(9600);
  HWSERIAL.begin(9600);
  Serial.println("<Arduino is ready>");
  InitPixelStates();
}

void loop() {
    switch(currentMode)
    {
       case modeConfetti:
          confetti();
          break;
       case modeTwinkle:
          twinkle(leds, chanceOfTwinkle);
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

    if (currentMode == modeTwinkle) {
       // fade all existing pixels toward bgColor by "5" (out of 255)
       fadeTowardColor( leds, NUM_LEDS, BASE_COLOR, 5);
    }

    // send the 'leds' array out to the actual LED strip
    FastLED.show();  
    // insert a delay to keep the framerate modest
    FastLED.delay(1000/framesPerSecond); 
  
    // do some periodic updates
    EVERY_N_MILLISECONDS( 20 ) { gHue++; } // slowly cycle the "base color" through the rainbow

    recvBytesWithStartEndMarkers();
    showNewData();
}

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
}

void showNewData() {
    if (newData == true) {
        Serial.print(peopleCount, HEX);
        Serial.println(' ');
        if (peopleCount > 1) {
          chanceOfTwinkle = 1;
          currentMode = modeRainbow;
        } else if (peopleCount == 1) {
          chanceOfTwinkle = 1;
          currentMode = modeFade;
        } else if (peopleCount == 0) {
          chanceOfTwinkle = 0;
          currentMode = modeFade;
        }
        newData = false;
    }
}

enum { SteadyDim, GettingBrighter, GettingDimmerAgain };
uint8_t PixelState[NUM_LEDS];

void InitPixelStates()
{
  memset( PixelState, sizeof(PixelState), SteadyDim); // initialize all the pixels to SteadyDim.
  fill_solid( leds, NUM_LEDS, BASE_COLOR);
}

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

void sinelon()
{
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, NUM_LEDS, 20);
  int pos = beatsin16( 13, 0, NUM_LEDS-1 );
  leds[pos] += CHSV( gHue, 255, 192);
}


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
