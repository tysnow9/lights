// Title: ESP8266 OSC to LED Light Strip Out
// Author: Tyler Wood
// Date: 2020.01.23
// Purpose: ESP8266 receives OSC data over Wifi and sends LED output (WS2812b).
// (Can be used with ESP32 as well, minor modifications needed)

// Wifi imports
#ifdef ESP8266
#include <ESP8266WiFi.h>
#else
#include <WiFi.h>
#endif

// OSC imports
#include <WiFiUdp.h>
#include <OSCMessage.h>
#include <OSCBundle.h>
#include <OSCData.h>

// ArtNet imports
#include <ArtnetWifi.h>

// FastLED imports
//#define FASTLED_ALLOW_INTERRUPTS 0
#include <FastLED.h>

// Wifi definition
const char* ssid = "WoodNot-g";
//const char* ssid = "WoodNot-Lights-g";
const char* password = "YesYouWood009";
//const char* ssid = "Crystalcabin";
//const char* password = "Cryst@lC@bin";
const int thisIp[] = {192, 168, 1, 133};

// Initializations
#define LED_BUILTIN 16
ArtnetWifi artnet;
WiFiUDP Udp;                          // A UDP instance to let us send and receive packets over UDP
const unsigned int outPort = 9000;    // remote port (not needed for receive)
const unsigned int localPort = 10000; // local port to listen for UDP packets (here's where we send the packets)
OSCMessage msg;
OSCErrorCode error;
#define LED_PIN     4
#define NUM_LEDS    200
#define LED_TYPE    WS2812
#define COLOR_ORDER GRB
#define FRAMES_PER_SECOND  120
CRGB leds[NUM_LEDS];
int level=70, red, green=50, blue, mix=NUM_LEDS;
int pLevel=70, pRed, pGreen=50, pBlue;
bool ledBumpEnabled = false;
int rate = FRAMES_PER_SECOND;
// Fire settings
bool gReverseDirection = false;
CRGBPalette16 gPal;
#define COOLING  55
#define SPARKING 120

// Serial debug
#define SERIAL_BAUDRATE 115200

// ---------------------------------------
// ---------- Setup ----------------------

void setup() {
  
  // Init serial port for debug
  Serial.begin(SERIAL_BAUDRATE);
  Serial.println("Initilizing setup...");

  // Connect WiFi
  ConnectWifi();
  pinMode(LED_BUILTIN, OUTPUT);     // Initialize the LED_BUILTIN pin as a WiFi indicator

  // OSC setup
  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.print("Local port: ");
#ifdef ESP32
  Serial.println(localPort);
#else
  Serial.println(Udp.localPort());
#endif

  // ArtNet - This will be called for each packet received
  artnet.setArtDmxCallback(onDmxFrame);
  artnet.begin();

  // Initialize LED variables, and setup LEDs (uses pin D4)
  FastLED.addLeds<LED_TYPE,LED_PIN,COLOR_ORDER>(leds, NUM_LEDS).setCorrection(TypicalLEDStrip);
  // set master brightness control
  FastLED.setBrightness(level);

  // Fire color palette
  gPal = HeatColors_p;
}

// ---------------------------------------
// ---------- Main Loop ------------------

// List of light patterns.  Each is defined as a separate function below.
typedef void (*SimplePatternList[])();
SimplePatternList gPatterns = { manual, touchDesigner, rainbow, rainbowWithGlitter, confetti, sinelon, juggle, bpm, fire };
int gCurrentPatternNumber = 0; // Index number of which pattern is current
uint8_t gHue = 0; // rotating "base color" used by many of the patterns

void loop() {
  
  // Reset Comms LED
  digitalWrite(LED_BUILTIN, HIGH); // Active low on ESP8266

  if (!ledBumpEnabled) {
    // ArtNet - Call the read function to receive ArtNet data
    // Only if in TouchDesigner mode (index=1)
    if (gCurrentPatternNumber == 1) {
      artnet.read();
    }
  }
  
  // Receive OSC data
  //  // TODO - "oscRead();" here?
  OSCMessage msg;
  int size = Udp.parsePacket();
  if (size > 0) {
    while (size--) {
      msg.fill(Udp.read());
    }
    if (!msg.hasError()) {
      digitalWrite(LED_BUILTIN, LOW); // Set Comms LED
      msg.dispatch("/1/fader1", ledLevel);
      msg.dispatch("/1/fader2", ledRed);
      msg.dispatch("/1/fader3", ledGreen);
      msg.dispatch("/1/fader4", ledBlue);
      msg.dispatch("/1/bump", ledBump); // Bump LEDs
      msg.dispatch("/1/rotary1", ledPattern); // Pattern/Mode switching
      msg.dispatch("/1/rotary2", ledRate);
    } else {
      error = msg.getError();
      Serial.print("error: ");
      Serial.println(error);
    }
  }

  // Call the current pattern/mode function once, updating the 'leds' array
  if (ledBumpEnabled)
    gPatterns[0]();
  else
    gPatterns[gCurrentPatternNumber]();
    

  // Fire - Add entropy to random number generator; we use a lot of it.
  random16_add_entropy(random16());

  // Send the 'leds' array out to the actual LED strip
  FastLED.setBrightness(level);
  FastLED.show();  
  // Insert a delay to keep the framerate modest
//  FastLED.delay(1000/FRAMES_PER_SECOND); 
  if (gCurrentPatternNumber != 1)
    FastLED.delay(1000/rate); 
  else
    delay(2);

  // Do some periodic updates
  EVERY_N_MILLISECONDS( 40 ) { gHue++; } // slowly cycle the "base color" through the rainbow
}

// ---------------------------------------
// ---------- ArtNet Callback ------------

void onDmxFrame(uint16_t universe, uint16_t length, uint8_t sequence, uint8_t* data)
{
//  // Comment/Uncomment to print out for debugging
//  boolean tail = false;
//  Serial.print("DMX: Univ: ");
//  Serial.print(universe, DEC);
//  Serial.print(", Seq: ");
//  Serial.print(sequence, DEC);
//  Serial.print(", Data (");
//  Serial.print(length, DEC);
//  Serial.print("): ");
//  if (length > 16) {
//    length = 16;
//    tail = true;
//  }
//  // print out the buffer
//  for (int i = 0; i < length; i++)
//  {
////    Serial.print(data[i], HEX);
//    Serial.print(data[i]);
////    float temp = data[i]/255.0;
////    Serial.print(temp*red);
//    Serial.print(" ");
//  }
//  if (tail) {
//    Serial.print("...");
//  }
//  Serial.println();

  // Update LEDs
  if (universe == 0)
  { // Do something different...?
  }
  
  if (universe == 1)
  {
    // Set Comms LED
    digitalWrite(LED_BUILTIN,LOW);
    float ledGain = 0.0;
    for( int i = 0; i < NUM_LEDS; i++) {
//      leds[i] = CRGB( data[3*i], data[3*i+1], data[3*i+2]);
//      leds[i] = ColorFromPalette(palette, gHue+(i*2), beat-gHue+(i*10));
      ledGain = data[i]/255.0;
      leds[i] = CRGB(ledGain*red, ledGain*green, ledGain*blue);
//      Serial.print(leds[i].r);
//      Serial.print(" ");
    }
//    Serial.println();
  }
}

// ---------------------------------------
// ---------- LED faders -----------------

void ledLevel(OSCMessage &msg) {
  level = (int)(255*msg.getFloat(0));
  pLevel = level;
}

void ledRed(OSCMessage &msg) {
//  Serial.println((int)(255*msg.getFloat(0)));
  red = (int)(255*msg.getFloat(0));
  pRed = red;
}

void ledGreen(OSCMessage &msg) {
  green = (int)(255*msg.getFloat(0));
  pGreen = green;
}

void ledBlue(OSCMessage &msg) {
  blue = (int)(255*msg.getFloat(0));
  pBlue = blue;
}

// ---------------------------------------
// ---------- LED Bump Buttons -----------

void ledBump(OSCMessage &msg) {
  Serial.println((int)msg.getFloat(0));
  switch ((int)msg.getFloat(0)) {
    case 1:
      pLevel = level;
      level = 255;
      ledBumpEnabled = true;
      break;
    case 2:
      ledBumpEnabled = true;
      pRed = red;
      red = 255;
      break;
    case 3:
      pGreen = green;
      green = 255;
      ledBumpEnabled = true;
      break;
    case 4:
      pBlue = blue;
      blue = 255;
      ledBumpEnabled = true;
      break;
    default:
      level = pLevel;
      red = pRed;
      green = pGreen;
      blue = pBlue;
      ledBumpEnabled = false;
      break;
  }
}

// ---------------------------------------
// ---------- Pattern Switching ----------

#define ARRAY_SIZE(A) (sizeof(A) / sizeof((A)[0]))

void ledPattern(OSCMessage &msg)
{
//  Serial.println((int)msg.getFloat(0));
  if ((int)msg.getFloat(0) < ARRAY_SIZE( gPatterns))
    gCurrentPatternNumber = (int)msg.getFloat(0);
}

void ledRate(OSCMessage &msg)
{
  rate = (int)(254*msg.getFloat(0)) + 1;
}

// ---------------------------------------
// ---------- Patterns (Modes) -----------

void manual() 
{
  for (int i = 0; i < mix; i++) {
    leds[i] = CRGB(red, green, blue);
  }
}

void touchDesigner() 
{
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
  addGlitter(80);
}

void addGlitter( fract8 chanceOfGlitter) 
{
  if( random8() < chanceOfGlitter) {
    leds[ random16(NUM_LEDS) ] += CRGB::White;
  }
}

void confetti() 
{
  // random colored speckles that blink in and fade smoothly
  fadeToBlackBy( leds, NUM_LEDS, 10);
  int pos = random(NUM_LEDS);
  leds[pos] += CHSV( gHue + random8(64), 200, 255);
}

void sinelon()
{
  // a colored dot sweeping back and forth, with fading trails
  fadeToBlackBy( leds, NUM_LEDS, 20);
  int pos = beatsin16( 13, 0, NUM_LEDS-1 );
  leds[pos] += CHSV( gHue, 255, 255);
}

void bpm()
{
  // colored stripes pulsing at a defined Beats-Per-Minute (BPM)
  uint8_t BeatsPerMinute = 62;
  CRGBPalette16 palette = PartyColors_p;
  uint8_t beat = beatsin8( BeatsPerMinute, 64, 255);
  for( int i = 0; i < NUM_LEDS; i++) { //9948
    leds[i] = ColorFromPalette(palette, gHue+(i*2), beat-gHue+(i*10));
  }
}

void juggle() {
  // eight colored dots, weaving in and out of sync with each other
  fadeToBlackBy( leds, NUM_LEDS, 20);
  byte dothue = 0;
  for( int i = 0; i < 8; i++) {
    leds[beatsin16( i+7, 0, NUM_LEDS-1 )] |= CHSV(dothue, 200, 255);
    dothue += 32;
  }
}

void fire()
{
// Array of temperature readings at each simulation cell
  static byte heat[NUM_LEDS];

  // Step 1.  Cool down every cell a little
    for( int i = 0; i < NUM_LEDS; i++) {
      heat[i] = qsub8( heat[i],  random8(0, ((COOLING * 10) / NUM_LEDS) + 2));
    }
  
    // Step 2.  Heat from each cell drifts 'up' and diffuses a little
    for( int k= NUM_LEDS - 1; k >= 2; k--) {
      heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2] ) / 3;
    }
    
    // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
    if( random8() < SPARKING ) {
      int y = random8(7);
      heat[y] = qadd8( heat[y], random8(160,255) );
    }

    // Step 4.  Map from heat cells to LED colors
    for( int j = 0; j < NUM_LEDS; j++) {
      // Scale the heat value from 0-255 down to 0-240
      // for best results with color palettes.
      byte colorindex = scale8( heat[j], 240);
      CRGB color = ColorFromPalette( gPal, colorindex);
      int pixelnumber;
      if( gReverseDirection ) {
        pixelnumber = (NUM_LEDS-1) - j;
      } else {
        pixelnumber = j;
      }
      leds[pixelnumber] = color;
    }
}

// ---------------------------------------
// ---------- Utility Functions ----------

void ConnectWifi()
{
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);
  
  IPAddress ip(thisIp[0],thisIp[1],thisIp[2],thisIp[3]);
  IPAddress gateway(192, 168, 1, 1);
  IPAddress subnet(255, 255, 255, 0);
  WiFi.config(ip, gateway, subnet);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    digitalWrite(LED_BUILTIN, LOW);
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  digitalWrite(LED_BUILTIN, HIGH);
}
