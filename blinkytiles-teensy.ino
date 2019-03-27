#include <DmxSimple.h>
#include <FastLED.h>
#define HWSERIAL Serial1
#define LED_PIN     12
#define NUM_LEDS    4
#define BRIGHTNESS  255
#define LED_TYPE    DMXSIMPLE
#define COLOR_ORDER BGR
CRGB leds[NUM_LEDS];

const byte numChars = 32;
char receivedChars[numChars];
boolean newData = false;

void setup() {
  FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS);
  FastLED.setCorrection(TypicalLEDStrip);
  FastLED.setBrightness(BRIGHTNESS);

  Serial.begin(9600);
  HWSERIAL.begin(9600);
  Serial.println("<Arduino is ready>");
}

void loop() {
//  leds[0] = CRGB::White; FastLED.show(); delay(200);
//  leds[0] = CRGB::Black; FastLED.show(); delay(200);
//  leds[1] = CRGB::White; FastLED.show(); delay(200);
//  leds[1] = CRGB::Black; FastLED.show(); delay(200);
//  leds[2] = CRGB::White; FastLED.show(); delay(200);
//  leds[2] = CRGB::Black; FastLED.show(); delay(200);
//  leds[3] = CRGB::White; FastLED.show(); delay(200);
//  leds[3] = CRGB::Black; FastLED.show(); delay(200);

  recvWithStartEndMarkers();
  showNewData();
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
      }
    }

    else if (rc == startMarker) {
      recvInProgress = true;
    }
  }
}

void showNewData() {
  if (newData == true) {
      Serial.println(receivedChars);
      newData = false;
  }
}
