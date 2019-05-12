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

CRGB leds[NUM_LEDS];

// animation blend
CRGB leds2[NUM_LEDS];
CRGB leds3[NUM_LEDS];

enum mode {modeTwinkle, modeConfetti};
mode currentMode = modeTwinkle;

const byte numChars = 32;
char receivedChars[numChars];
boolean newData = false;

#define BRIGHTNESS   255

uint8_t newHue;
uint8_t gHue = 160; // rotating "base color" used by many of the patterns

// Base background color
CHSV BASE_COLOR(gHue, 255, 32);

// Peak color to twinkle up to
CHSV PEAK_COLOR(gHue, 255, 160);

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
      EVERY_N_MILLISECONDS(100) {
        confetti();
        FastLED.show();
      }
     case modeTwinkle:
      EVERY_N_MILLISECONDS( 10 ) {
        twinkle(leds, chanceOfTwinkle);
        FastLED.show();
      }
      break;
    default:
      break;
  }
  receiveMsg();
}

void receiveMsg() {
  while (HWSERIAL.available() > 0 && newData == false) {
    String json;
    json = HWSERIAL.readString();
  
    // Allocate the JSON document
    //
    // Inside the brackets, 200 is the capacity of the memory pool in bytes.
    // Don't forget to change this value to match your JSON document.
    // Use arduinojson.org/v6/assistant to compute the capacity.
    StaticJsonDocument<400> doc;
  
    // StaticJsonDocument<N> allocates memory on the stack, it can be
    // replaced by DynamicJsonDocument which allocates in the heap.
    //
    // DynamicJsonDocument doc(200);
  
    // JSON input string.
    //
    // Using a char[], as shown here, enables the "zero-copy" mode. This mode uses
    // the minimal amount of memory because the JsonDocument stores pointers to
    // the input buffer.
    // If you use another type of input, ArduinoJson must copy the strings from
    // the input to the JsonDocument, so you need to increase the capacity of the
    // JsonDocument.
  
    // Deserialize the JSON document
    DeserializationError error = deserializeJson(doc, json);
  
    // Test if parsing succeeds.
    if (error) {
      Serial.print(F("deserializeJson() failed: "));
      Serial.println(error.c_str());
      return;
    }
  
    // Fetch values.
    //
    // Most of the time, you can rely on the implicit casts.
    // In other case, you can do doc["time"].as<long>();
    uint8_t numberOfPeople = doc["person"];
  
    // Print values.
    // serializeJsonPretty(doc, Serial);
    String stringOne = "I see ";
    String stringTwo = " people";
    String peopleString = stringOne + numberOfPeople + stringTwo;
    Serial.println(peopleString);

    newData = true;
    if (numberOfPeople > 0) {
      chanceOfTwinkle = 1;
//      Must deal with interrupts to use confetti!
//      https://github.com/FastLED/FastLED/wiki/Interrupt-problems
//      currentMode = modeConfetti;
//    } else if (numberOfPeople == 1) {
//      chanceOfTwinkle = 1;
    } else if (numberOfPeople == 0) {
      chanceOfTwinkle = 0;
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

void confetti() 
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy(leds, NUM_LEDS, BRIGHTNESS*0.8);
  int pos = random16(NUM_LEDS);
  leds[pos] += CHSV( gHue + random8(64), 200, 255);
  FastLED.show();
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
