#include <DmxSimple.h>
#include <FastLED.h>
#include <SoftwareSerial.h>

SoftwareSerial HWSERIAL(6,7); // RX, TX

#define LED_PIN     3
#define NUM_LEDS    7
#define LED_TYPE    DMXSIMPLE
#define COLOR_ORDER BGR

// The brightness of our pixels (0 to 255).
// 0 is off.
// 16 is dim.
// 64 is medium.
// 128 is bright.
// 255 is blindingly bright!

#define BRIGHTNESS  8
#define MIN_BRIGHTNESS 8  // watch the power!
#define MAX_BRIGHTNESS 64  

CRGB leds[NUM_LEDS];

const byte numChars = 32;
char receivedChars[numChars];
unsigned int currentPatternIndex = 0;
boolean newData = false;

enum mode {modeSlowFade, modeBlueDots, modeRandomColors, modeBreathing, modeEase};
mode currentMode = modeEase;

int hue = 0;
int divisor = 30;
byte currentLed = 0;

int thisdelay = 15; // easing mode

void setup() {
  delay(3000); // sanity check
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
  Serial.begin(9600);
  HWSERIAL.begin(9600);
  Serial.println("<Arduino is ready>");
}

void loop() {

  switch(currentMode)
  {
    case modeBlueDots:
      Serial.print("blue dots\n");
      blueDots();
      break; 
    case modeBreathing:
      breathing();
      break;
    case modeEase:
      ease();
      break;
    case modeSlowFade:
      slowFade();
      break;
    case modeRandomColors:
      randomColors();
      break;
    default:
      break;
  }
  recvWithStartEndMarkers();
}

void trigger() {
  if (strcmp(receivedChars, "person") == 0) {
    Serial.println(receivedChars);
    currentMode = modeSlowFade;
  } else {
    Serial.println("other: ");
    Serial.println(receivedChars);
    currentMode = modeBlueDots;
  }
  newData = false;
}

// Credit: http://forum.arduino.cc/index.php?topic=396450
void recvWithStartEndMarkers() {
  static boolean recvInProgress = false;
  static byte ndx = 0;
  char startMarker = '<';
  char endMarker = '>';
  char rc;

  while (HWSERIAL.available() > 0 && newData == false) {
    rc = HWSERIAL.read();

    if (recvInProgress == true) {

      if (rc != endMarker) {
        receivedChars[ndx] = rc;
        ndx++;
        if (ndx >= numChars) {
            ndx = numChars - 1;
        }
      }
      else {
        receivedChars[ndx] = '\0'; // terminate the string
        recvInProgress = false;
        ndx = 0;
        newData = true;
        trigger();
      }
    }
    else if (rc == startMarker) {
      recvInProgress = true;
    }
  }
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

void randomColors() {
  CRGB bgColor( 0, 15, 2); // pine green ?
  
  // fade all existing pixels toward bgColor by "5" (out of 255)
  fadeTowardColor( leds, NUM_LEDS, bgColor, 5);

  // periodically set random pixel to a random color, to show the fading
  EVERY_N_MILLISECONDS( 50 ) {
    uint16_t pos = random16( NUM_LEDS);
    CRGB color = CHSV( random8(), 255, 255);
    leds[ pos ] = color;
  }
  FastLED.show();
}

void blueDots() {
  for(int dot = 0; dot < NUM_LEDS; dot++) { 
    leds[dot] = CRGB::Blue;
    FastLED.show();
    // clear this led for the next time around the loop
    leds[dot] = CRGB::Black;
    delay(100);
  }
}

void slowFade() {
  hue++;
  fill_solid(leds, NUM_LEDS, CHSV(hue, 255, 255));
  FastLED.show();
}

void breathing () {
 float breath = (exp(sin(millis()/5000.0*PI)) - 0.36787944)*108.0;
 breath = map(breath, 0, 255, MIN_BRIGHTNESS, MAX_BRIGHTNESS);
 FastLED.setBrightness(breath);
 fill_rainbow(leds, NUM_LEDS, (hue++/divisor));
 if(hue == (255 * divisor)) {
   hue = 0;
 }
 FastLED.show();
 delay(1);
}

void ease() {
  EVERY_N_MILLISECONDS(thisdelay) {  // FastLED based non-blocking delay to update/display the sequence.
    static uint8_t easeOutVal = 0;
    static uint8_t easeInVal  = 128;
    static uint8_t lerpVal    = 0;
  
    easeOutVal = ease8InOutQuad(easeInVal);  // Start with easeInVal at 0 and then go to 255 for the full easing.
    easeInVal++;
  
    lerpVal = lerp8by8(0, NUM_LEDS, easeOutVal);  // Map it to the number of LED's you have.
  
    leds[lerpVal] = CRGB::White;
    fadeToBlackBy(leds, NUM_LEDS, 2);  // 8 bit, 1 = slow fade, 255 = fast fade
    
  }
  FastLED.show();
}
