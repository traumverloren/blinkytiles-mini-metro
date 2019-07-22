#include <DmxSimple.h>
#include <FastLED.h>
#include <SoftwareSerial.h>

#if FASTLED_VERSION < 3001000
#error "Requires FastLED 3.1 or later; check github for latest code."
#endif

SoftwareSerial HWSERIAL(6,7); // RX, TX

#define ACTIVATED LOW          // button is active low (HIGH = 1, LOW = 0)
#define BUTTON_PIN 10          // Connect the button to GND and one of the pins. 
#define LED_PIN     3
#define NUM_LEDS    7
#define LED_TYPE    DMXSIMPLE
#define COLOR_ORDER BGR
#define BRIGHTNESS  255

CRGB leds[NUM_LEDS];

//----------initial button setup--------------------
long buttonTimer = 0;
long longPressTime = 250;

boolean buttonActive = false;
boolean longPressActive = false;


//---For receiving serial port messages from RPI---
byte peopleCount;
boolean newData = false;
static byte startMarker = 0X3D;
static byte endMarker = 0x3E;

//
// Light modes //
//
enum mode {
  modeDark, 
  modeTwinkle,
  modeBreathing, 
  modeRainbowBreathing, 
  modeHighChaos, 
  modeMidChaos, 
  modeLowChaos
};

// set the default starting LED program
mode currentMode = modeHighChaos;

boolean isCalmWhenCrowded = true;
boolean areLightsOn = true;

uint8_t framesPerSecond = 12;  // 12 for start with sinelon + random leds, 30 for Sinelon, 60 normally for rest
uint8_t newHue;
uint8_t gHue = 160; // rotating "base color" used by many of the patterns

enum { SteadyDim, GettingBrighter, GettingDimmerAgain };
uint8_t PixelState[NUM_LEDS];

//
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

//
// TWINKLE //
//
// Base background color
CHSV BASE_COLOR(gHue, 255, 50);

// Peak color to twinkle up to
CHSV PEAK_COLOR(gHue, 255, BRIGHTNESS);

// Amount to increment the color by each loop as it gets brighter:
CHSV DELTA_COLOR_UP(gHue, 255, 14);

// Amount to decrement the color by each loop as it gets dimmer:
CHSV DELTA_COLOR_DOWN(gHue, 255, 20);

// Chance of each pixel starting to brighten up.  
// 1 or 2 = a few brightening pixels at a time.
// 10 = lots of pixels brightening at a time.
uint8_t chanceOfTwinkle = 0;


//------------------SETUP------------------
void setup() {
  delay(4000);
  FastLED.addLeds<LED_TYPE,LED_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  set_max_power_in_volts_and_milliamps(5, 500);  // FastLED Power management set at 5V, 500mA.

  Serial.begin(9600);
  HWSERIAL.begin(9600);
  Serial.println("<Arduino is ready>"); 

  currentBlending = LINEARBLEND;

  pinMode(BUTTON_PIN, INPUT_PULLUP);
}

//------------------MAIN LOOP------------------
void loop() { 
    byte currKeyState = digitalRead(BUTTON_PIN);
  
    // button pressed
    if (currKeyState == ACTIVATED) {
      if (buttonActive == false) {
        buttonActive = true;
        buttonTimer = millis();
      }
      if ((millis() - buttonTimer > longPressTime) && (longPressActive == false)) {
        longPressActive = true;
        longButtonPress();
      }
    // button not pressed
    } else {
      if (buttonActive == true) {
        if (longPressActive == true) {
          longPressActive = false;
        } else {
          shortButtonPress();
        }
        buttonActive = false;
      }
    }

    switch(currentMode)
    {
       case modeHighChaos:
          setHighChaosMode();
          break;
       case modeMidChaos:
          setMidChaosMode();
          break;
       case modeLowChaos:
          setLowChaosMode();
          break;
       case modeRainbowBreathing:
          setRainbowBreathingMode();
          break;
       case modeBreathing:
          setBreathingMode();
          break;
       case modeTwinkle:
          setTwinkleMode(peopleCount);
          break;
      case modeDark:
          setDarkMode();
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
}

//------------------Button Press Setup------------------
void shortButtonPress() {
  if (areLightsOn == false) {
    return;
  }
  isCalmWhenCrowded = !isCalmWhenCrowded;
  setProgram();
}

void longButtonPress() {
  areLightsOn = !areLightsOn;
  if (areLightsOn == false) {
    setDarkMode();
  } else {
    setProgram();
  }
}
 

//------------------RECEIVING DATA FROM PI------------------
// ref: https://forum.arduino.cc/index.php?topic=396450.0
//
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

//------------------SET NEW LIGHT PROGRAM------------------
void showNewData() {
    if (newData == true) {
        setProgram();
        newData = false;
    }
}

// If not getting data, make sure RPI pinouts for Serial in #14 for data & ground above it!!!
void setProgram() {
  if (areLightsOn == false) {
       return;
  } else if (isCalmWhenCrowded == true) {
        if (peopleCount == 0) { //0 Chaotic, frequently bouncing no color order
            setHighChaosMode();
        } else if (peopleCount == 1) { //1 Less Chaotic, has rainbow color order
            setMidChaosMode();
        } else if (peopleCount == 2) { //2 Smooth flow up and down with rainbow color order
            setLowChaosMode();
        } else if (peopleCount == 3) { //3 All gently lit rotating rainbow with in/out dim/brighten
            setRainbowBreathingMode();
        } else if (peopleCount == 4) { //4 All gently blue with in/out dim/brighten
            setBreathingMode();
        } else if ((peopleCount == 5) || (peopleCount == 6)) { //5-6 Go to all blue with twinkle alternating
            setTwinkleMode(peopleCount);
        } else if (peopleCount > 6) { //6+? Go to all dim blue
          setDarkMode();
        }
    } else {
        if (peopleCount > 6) { //6+ Chaotic, frequently bouncing no color order
            setHighChaosMode();
        } else if (peopleCount == 6) { //6 Less Chaotic, has rainbow color order
            setMidChaosMode();
        } else if (peopleCount == 5) { //5 Smooth flow up and down with rainbow color order
            setLowChaosMode();
        } else if (peopleCount == 4) { //4 All gently lit rotating rainbow with in/out dim/brighten
            setRainbowBreathingMode();
        } else if (peopleCount == 3) { //3 All gently lit blue with in/out dim/brighten
            setBreathingMode();
        } else if ((peopleCount == 2) || (peopleCount == 1)) { //1-2 Go to all blue with twinkle alternating
            setTwinkleMode(peopleCount);
        } else if (peopleCount == 0) { //0 Go dark
            setDarkMode();
        }
    }
} 

void setHighChaosMode() {
  currentMode = modeHighChaos;
  framesPerSecond = 16;
  sinelon();
}

void setMidChaosMode() {
  currentMode = modeMidChaos;
  framesPerSecond = 12;
  sinelon();
}

void setLowChaosMode() {
  currentMode = modeLowChaos;
  framesPerSecond = 30;
  sinelon();
}

void setRainbowBreathingMode() {
  if (currentMode != modeRainbowBreathing) {
    for (uint16_t i = 0; i < 100; i++) {
      rainbowBreathing(leds, NUM_LEDS, 2);
        FastLED.show();
        FastLED.delay(10);
    }
  }
  currentMode = modeRainbowBreathing;
  framesPerSecond = 30;
  rainbowBreathing( leds, NUM_LEDS, 2 );
}

void setBreathingMode() {
  if (currentMode != modeBreathing) {
    for (uint16_t i = 0; i < 100; i++) {
        breathing(leds, NUM_LEDS, 2);
        FastLED.show();
        FastLED.delay(10);
    }
  }
  currentMode = modeBreathing;
  framesPerSecond = 30;
  breathing(leds, NUM_LEDS, 2);
}

void setTwinkleMode(byte peopleCount) {
  if ((peopleCount == 6) || (peopleCount == 2)) {
    chanceOfTwinkle = 1;
  } else {
    chanceOfTwinkle = 0;
  }
  if (currentMode != modeTwinkle) {
      for ( uint16_t i = 0; i < 100; i++) {
          fadeTowardColor( leds, NUM_LEDS, BASE_COLOR, 2);
          FastLED.show();
          FastLED.delay(10);
      }
  }
  currentMode = modeTwinkle;
  framesPerSecond = 40;
  twinkle(leds, chanceOfTwinkle);
}

void setDarkMode() {
  if (currentMode != modeDark) {
      for ( uint16_t i = 0; i < 100; i++) {
          fadeToBlackBy(leds, NUM_LEDS, BRIGHTNESS*0.1);
          FastLED.show();
          FastLED.delay(10);
      }
      for ( uint16_t i = 0; i < NUM_LEDS; i++) {
        leds[0] = CRGB::Black;
      }
  }
  currentMode = modeDark;
}

void InitPixelStates()
{
  memset( PixelState, sizeof(PixelState), SteadyDim); // initialize all the pixels to SteadyDim.
  fill_solid( leds, NUM_LEDS, BASE_COLOR);
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

void rainbowBreathing( CRGB* L, uint16_t N, uint8_t fadeAmount ) {
    //beatsin16 is a function on the FastLED library generating sinwave, (5) is bpm, (0,255) is value range.
    //value range will create the breathing effect 
    int pos = beatsin16( 5, 55, BRIGHTNESS ); // generating the sinwave 
     // shifting the HUE value by incrementing every millisecond this creates the spectrum wave
    for( uint16_t i = 0; i < N; i++) {
      CHSV hsv( gHue+(i*7), 255, pos);
      CRGB rgb;
      hsv2rgb_rainbow( hsv, rgb);
      fadeTowardColor( L[i], rgb, fadeAmount);
    }
}

void breathing( CRGB* L, uint16_t N, uint8_t fadeAmount ) {
    //beatsin16 is a function on the FastLED library generating sinwave, (5) is bpm, (0,255) is value range.
    //value range will create the breathing effect 
    int pos = beatsin16( 5, 55, 200 ); // generating the sinwave 
    fill_solid(leds, NUM_LEDS, CHSV( 160, 255, pos )); // CHSV (hue, saturation, value);
    for( uint16_t i = 0; i < N; i++) {
      fadeTowardColor( L[i], CHSV( 160, 255, pos ), fadeAmount);
    }
}

void sinelon() {   // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, NUM_LEDS, thisfade);
  int pos;
  int correctedPos;

  if (currentMode == modeHighChaos) 
  {
    correctedPos = random8(NUM_LEDS);
    myhue = random16(255);
    if (myhue < 100) {
      myhue = random16(160,224);
    } else {
      myhue = random16(96);
    } 
  } else if (currentMode == modeMidChaos) {
    correctedPos = random8(NUM_LEDS);
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

  leds[correctedPos] += ColorFromPalette(currentPalette, myhue, thisbri, currentBlending);
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

void fadeTowardRainbow( CRGB* L, uint16_t N, uint8_t fadeAmount )
{
  for( uint16_t i = 0; i < N; i++) {
    CHSV hsv( gHue+(i*7), 255, 255);
    CRGB rgb;
    hsv2rgb_rainbow( hsv, rgb);
    fadeTowardColor( L[i], rgb, fadeAmount);
  }
}
