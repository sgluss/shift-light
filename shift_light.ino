#include <FastLED.h>
#include "BluetoothSerial.h"
#include <iostream> 
#include <unordered_map> 


#define LED_PIN     27
#define SPARK_INPUT_PIN 35
#define NUM_LEDS    12
#define BRIGHTNESS  255
#define LED_TYPE    WS2811
#define COLOR_ORDER GRB
CRGB leds[NUM_LEDS];

#define UPDATES_PER_SECOND 100

#define START_RPM 500.0
#define REDLINE 5000.0
int ANALOG_THRESHOLD = 128;

// This example shows several ways to set up and use 'palettes' of colors
// with FastLED.
//
// These compact palettes provide an easy way to re-colorize your
// animation on the fly, quickly, easily, and with low overhead.
//
// USING palettes is MUCH simpler in practice than in theory, so first just
// run this sketch, and watch the pretty lights as you then read through
// the code.  Although this sketch has eight (or more) different color schemes,
// the entire sketch compiles down to about 6.5K on AVR.
//
// FastLED provides a few pre-configured color palettes, and makes it
// extremely easy to make up your own color schemes with palettes.
//
// Some notes on the more abstract 'theory and practice' of
// FastLED compact palettes are at the bottom of this file.



CRGBPalette16 currentPalette;
TBlendType    currentBlending;

extern CRGBPalette16 myRedWhiteBluePalette;
extern const TProgmemPalette16 myRedWhiteBluePalette_p PROGMEM;

String serialBuffer;
CRGB outColor = CHSV( HUE_GREEN, 255, 255);
std::unordered_map<char*, int> colorMap;
int selectedColor = 1;

void setup() {
    pinMode(SPARK_INPUT_PIN, INPUT);
  
    Serial.begin(9600);           //  setup serial
    Serial.println("Starting...");

    
    delay( 1000 ); // power-up safety delay
    FastLED.addLeds<LED_TYPE, LED_PIN, COLOR_ORDER>(leds, NUM_LEDS).setCorrection( TypicalLEDStrip );
    FastLED.setBrightness(  BRIGHTNESS );
    
    currentBlending = NOBLEND;
    SetupTachPalette();

    colorMap["green"] = 1; 
    colorMap["yellow"] = 2;
    colorMap["red"] = 3;
}


int val = 0;
int count = 0.0;
uint32_t lastUpdate = micros();
uint32_t now;

float rpm = 0.0;
float averageRpm = 0.0;
uint32_t lastTrigger = 0;
uint32_t lastLEDUpdate = 0;
int currentTriggerState = 0;

uint8_t btBuffer[256];
uint8_t btThisChar;
int bufferIndex;

int previousReadings[16] = {};
int previousReadingsSize = 0;
int group1Sum = 0;
float group1Avg;
int group2Sum = 0;
float group2Avg;
boolean detectEdge()
{
  // roll forward all past readings
  for(int i = 15; i > 0; i--)
  {
    previousReadings[i] = previousReadings[i - 1];
  }
  // set latest reading
  previousReadings[0] = currentTriggerState;
  if (previousReadingsSize < 16) {
    previousReadingsSize++;
  }
  if (previousReadingsSize > 15) { // this condition basically also debounces for us
    group1Sum = 0;
    group2Sum = 0;
    for(int i = 0; i < 5; i++)
    {
      group1Sum += previousReadings[i];
    }
    for(int i = 5; i < 15; i++)
    {
      group2Sum += previousReadings[i];
    }
    group1Avg = group1Sum / 5.0;
    group2Avg = group2Sum / 10.0;
  
    if (group1Avg < ANALOG_THRESHOLD && group2Avg > ANALOG_THRESHOLD) {
      // reset
      previousReadingsSize = 0;
      return true;
    }
  }
  return false;
}


boolean edgeDetected;
void loop()
{
    now = micros();
    if (now - lastUpdate > 1000000) {
//      Serial.print(averageRpm);Serial.println("RPM");
//      Serial.print(count);Serial.println("Hz");Serial.println("");
     
      lastUpdate = now;
      count = 0;
    }
    count++;

    currentTriggerState = (int)analogRead(SPARK_INPUT_PIN);
    edgeDetected = detectEdge();
    if (edgeDetected) {
      // 2 revolutions per spark, 60 seconds per minute, 1,000,000 micros per second
      rpm = (2.0 * 60.0 * 1000000.0) / (now - lastTrigger); 
      averageRpm = (0.5 * averageRpm) + (0.5 * rpm);
      lastTrigger = now;
    }
         
    static uint8_t startIndex = 0;

    if (now - lastLEDUpdate > 50000) {
      FillLEDs();
      FastLED.show();
      lastLEDUpdate = now;
      
      //Serial.write(SerialBT.read());Serial.println();
//        switch (selectedColor)
//        {
//          case 1: 
//            outColor = CHSV( HUE_GREEN, 255, 255);
//            break;
//          case 2: 
//            outColor = CHSV( HUE_YELLOW, 255, 255);
//            break;
//          case 3: 
//            outColor = CHSV( HUE_RED, 255, 255);
//            break;
//        }
    }
    //FastLED.delay(1000 / UPDATES_PER_SECOND);
}

void FillLEDs()
{
    uint8_t brightness = 64;
    
    float correctedRpm = averageRpm - START_RPM;
    float correctedRedline = REDLINE - START_RPM;
    float index = 0.0;
    
    // SHIFT LIGHT
    if (correctedRpm > correctedRedline) {
      if ((millis() % 200) > 50) {
        for( uint8_t i = 0; i < NUM_LEDS; i++) {
          leds[i] = CRGB::Red;
        }
      } else {
        for( uint8_t i = 0; i < NUM_LEDS; i++) {
          leds[i] = CRGB::White;
        }        
      }
      return;
    }

    // TACHOMETER
    for( uint8_t i = 0; i < NUM_LEDS; i++) {
        if ((correctedRpm / correctedRedline) > (index / NUM_LEDS)) {
          if ((correctedRpm / correctedRedline) > ((index + 1) / NUM_LEDS)) {
            // dimly illuminate all LEDs below current tach position
            leds[i] = ColorFromPalette( currentPalette, i * 16, 2, NOBLEND);
          }
          else {
            // set current tach position to full brightness
            leds[i] = ColorFromPalette( currentPalette, i * 16, brightness, NOBLEND);
          }
        } else {
          // Beyond the current tach position, turn off led
          leds[i] = CRGB::Black;          
        }
        index++;
    }
}

void FillLEDsFromPaletteColors( uint8_t colorIndex)
{
    uint8_t brightness = 255;
    
    for( int i = 0; i < NUM_LEDS; i++) {
        leds[i] = ColorFromPalette( currentPalette, colorIndex, brightness, currentBlending);
        colorIndex += 1;
    }
}

void SetupTachPalette()
{
    for( int i = 0; i < 4; i++) {
        currentPalette[i] = CRGB::Green;
    }
    for( int i = 3; i < 8; i++) {
        currentPalette[i] = CRGB::Yellow;
    }
    for( int i = 6; i < 12; i++) {
        currentPalette[i] = CRGB::OrangeRed;
    }
    for( int i = 9; i < 16; i++) {
        currentPalette[i] = CRGB::Red;
    }
}


// There are several different palettes of colors demonstrated here.
//
// FastLED provides several 'preset' palettes: RainbowColors_p, RainbowStripeColors_p,
// OceanColors_p, CloudColors_p, LavaColors_p, ForestColors_p, and PartyColors_p.
//
// Additionally, you can manually define your own color palettes, or you can write
// code that creates color palettes on the fly.  All are shown here.

void ChangePalettePeriodically()
{
    uint8_t secondHand = (millis() / 1000) % 60;
    static uint8_t lastSecond = 99;
    
    if( lastSecond != secondHand) {
        lastSecond = secondHand;
        if( secondHand ==  0)  { currentPalette = RainbowColors_p;         currentBlending = LINEARBLEND; }
        if( secondHand == 10)  { currentPalette = RainbowStripeColors_p;   currentBlending = NOBLEND;  }
        if( secondHand == 15)  { currentPalette = RainbowStripeColors_p;   currentBlending = LINEARBLEND; }
        if( secondHand == 20)  { SetupPurpleAndGreenPalette();             currentBlending = LINEARBLEND; }
        if( secondHand == 25)  { SetupTotallyRandomPalette();              currentBlending = LINEARBLEND; }
        if( secondHand == 30)  { SetupBlackAndWhiteStripedPalette();       currentBlending = NOBLEND; }
        if( secondHand == 35)  { SetupBlackAndWhiteStripedPalette();       currentBlending = LINEARBLEND; }
        if( secondHand == 40)  { currentPalette = CloudColors_p;           currentBlending = LINEARBLEND; }
        if( secondHand == 45)  { currentPalette = PartyColors_p;           currentBlending = LINEARBLEND; }
        if( secondHand == 50)  { currentPalette = myRedWhiteBluePalette_p; currentBlending = NOBLEND;  }
        if( secondHand == 55)  { currentPalette = myRedWhiteBluePalette_p; currentBlending = LINEARBLEND; }
    }
}

// This function fills the palette with totally random colors.
void SetupTotallyRandomPalette()
{
    for( int i = 0; i < 16; i++) {
        currentPalette[i] = CHSV( random8(), 255, random8());
    }
}

// This function sets up a palette of black and white stripes,
// using code.  Since the palette is effectively an array of
// sixteen CRGB colors, the various fill_* functions can be used
// to set them up.
void SetupBlackAndWhiteStripedPalette()
{
    // 'black out' all 16 palette entries...
    fill_solid( currentPalette, 16, CRGB::Black);
    // and set every fourth one to white.
    currentPalette[0] = CRGB::White;
    currentPalette[4] = CRGB::White;
    currentPalette[8] = CRGB::White;
    currentPalette[12] = CRGB::White;
    
}

// This function sets up a palette of purple and green stripes.
void SetupPurpleAndGreenPalette()
{
    CRGB purple = CHSV( HUE_PURPLE, 255, 255);
    CRGB green  = CHSV( HUE_GREEN, 255, 255);
    CRGB black  = CRGB::Black;
    
    currentPalette = CRGBPalette16(
                                   green,  green,  black,  black,
                                   purple, purple, black,  black,
                                   green,  green,  black,  black,
                                   purple, purple, black,  black );
}


// This example shows how to set up a static color palette
// which is stored in PROGMEM (flash), which is almost always more
// plentiful than RAM.  A static PROGMEM palette like this
// takes up 64 bytes of flash.
const TProgmemPalette16 myRedWhiteBluePalette_p PROGMEM =
{
    CRGB::Red,
    CRGB::Gray, // 'white' is too bright compared to red and blue
    CRGB::Blue,
    CRGB::Black,
    
    CRGB::Red,
    CRGB::Gray,
    CRGB::Blue,
    CRGB::Black,
    
    CRGB::Red,
    CRGB::Red,
    CRGB::Gray,
    CRGB::Gray,
    CRGB::Blue,
    CRGB::Blue,
    CRGB::Black,
    CRGB::Black
};



// Additional notes on FastLED compact palettes:
//
// Normally, in computer graphics, the palette (or "color lookup table")
// has 256 entries, each containing a specific 24-bit RGB color.  You can then
// index into the color palette using a simple 8-bit (one byte) value.
// A 256-entry color palette takes up 768 bytes of RAM, which on Arduino
// is quite possibly "too many" bytes.
//
// FastLED does offer traditional 256-element palettes, for setups that
// can afford the 768-byte cost in RAM.
//
// However, FastLED also offers a compact alternative.  FastLED offers
// palettes that store 16 distinct entries, but can be accessed AS IF
// they actually have 256 entries; this is accomplished by interpolating
// between the 16 explicit entries to create fifteen intermediate palette
// entries between each pair.
//
// So for example, if you set the first two explicit entries of a compact 
// palette to Green (0,255,0) and Blue (0,0,255), and then retrieved 
// the first sixteen entries from the virtual palette (of 256), you'd get
// Green, followed by a smooth gradient from green-to-blue, and then Blue.
