#include <DmxSimple.h>
#include <FastLED.h>

#define LED_PIN     12
#define NUM_LEDS    4
#define BRIGHTNESS  255
#define LED_TYPE    DMXSIMPLE
#define COLOR_ORDER BGR
CRGB leds[NUM_LEDS];

void setup() {
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);
}

void loop() {
  leds[0] = CRGB::White; FastLED.show(); delay(200);
  leds[0] = CRGB::Black; FastLED.show(); delay(200);
  //  leds[1] = CRGB::White; FastLED.show(); delay(200);
  //  leds[1] = CRGB::Black; FastLED.show(); delay(200);
  leds[2] = CRGB::White; FastLED.show(); delay(200);
  leds[2] = CRGB::Black; FastLED.show(); delay(200);
  //  leds[3] = CRGB::White; FastLED.show(); delay(200);
  //  leds[3] = CRGB::Black; FastLED.show(); delay(200);
}
