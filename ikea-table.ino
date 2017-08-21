// #include <SoftPWM.h>
// #include <SoftPWM_timer.h>
// #include <Wire.h>
#include <i2c_t3.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "glcdfont.c"
#include "TomThumb.h"
#include "Picopixel.h"
#include "Tiny3x3a2pt7b.h"
#include "Org_01.h"

// #include <U8g2lib.h>
// #include <U8x8lib.h>

// U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, 4, 19, 18);
// U8G2_SSD1306_128X64_NONAME_1_SW_I2C u8g2(U8G2_R0, 19, 18);

#if !defined(__INT_MAX__) || (__INT_MAX__ > 0xFFFF)
 #define pgm_read_pointer(addr) ((void *)pgm_read_dword(addr))
#else
 #define pgm_read_pointer(addr) ((void *)pgm_read_word(addr))
#endif

// Modes
// mode 1
// a moving dot. Turtlewalk where last turn isn't allowed
// keep position when not sensor active.

// mode 2
// also turtle walk. But move when sensor changes state. Keep activated pixel for a determined time

// mode 3
// Timeline över dagen. Activate pixels if there was a state change. Timeline goes left to right, top to bottom.
// dom 24 pixlarna motsvarar timmar

// mode 4
// visa digital-klocka

// mode 5
// Skriv en text i appen

// Appen
// Byta mellan bordets olika lägen
// Har tre funktioner. Visa 1 eller 0 för sensorer.
// Skriv text i en textruta.
// Servera tiden till bordet.

#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);

//Diagnosis stuff
bool pinDiagnosis = false;
int previousDiagnosisX = 0;
int diagnosisX = 0;
int previousDiagnosisY = 0;
int diagnosisY = 0;


const bool autoIncrementSinkPin = true;
const bool autoIncrementPattern = false;
bool decreasePixels = true;

//PINOUTS!!!
// const int buttonPin = 29;

const bool useShiftRegisters = true;
// const int sourcePins[] = {8, 9, 10};//, 11, 12, 14, 15, 16};
const int sourcePins[] = {30,31,32,33,34,35,36,37,38,39,14,15,16,17,22,23};
const int nrOfSourcePins = sizeof(sourcePins)/sizeof(sourcePins[0]);
int currentSourceIndex = 0;
int currentSourcePin = sourcePins[currentSourceIndex];

struct shiftRegister {
  // const int gnd;
  // const int vcc;
  const int clockPin;
  const int latchPin;
  const int dataPin;
  const int nrOfLinkedRegisters;
  int pins[64];
};

struct shiftRegister shifters[2] = {
  {2,3,4, 2},
  {5,6,7, 2}
};

const bool useMultiplexers = false;
const int sinkPins[] = {23,22,17,16,15,14,39,38,37,36,35,34,33,11,12,24,25,26,27,28,29,30,31,32};
const int nrOfSinkPins = sizeof(sinkPins)/sizeof(sinkPins[0]);
const int nrOfCycledSinkPins = nrOfSinkPins / 2;
// const int nrOfSinkPins = 24;
int currentSinkIndex = 0;
int currentSinkPin = sinkPins[currentSinkIndex];

//analog multiplexer
struct analogMultiplexer {
  const int gnd;// = 0;
  const int vcc;// = 1;
  const int en;// = 2;
  const int bits[4];// = {3,4,5,6};
  const int z;// = 7;
};

struct analogMultiplexer plexers[2] = {
  {0,1,2,{3,4,5,6},7},
  {8,9,10,{11,12,24,25},26}
};

//Interfacing stuff and Modes
String inputString;
char string[6] = "bajs!";
GFXfont* currentFont = NULL;

enum mode { CLOCKMODE, TEXTMODE, TURTLEWHILESITTING, TURTLEONSTATECHANGE, TIMELINE, PIXELSCREEN };
int currentMode = PIXELSCREEN;

//PATTERNS
// int patterns[][nrOfSourcePins][nrOfSinkPins] = {
//   { {1,0,0,0,1,1,1,0},
//     {0,1,0,1,0,0,0,0},
//     {0,0,1,0,0,0,0,0},
//     {0,0,0,0,0,0,0,0},
//     {0,0,0,0,0,0,0,0},
//     {0,0,0,0,0,0,0,0},
//     {0,0,0,0,0,0,0,0},
//     {0,0,0,0,0,0,0,0} },
//
//   { {1,1,1,1,1,1,1,1},
//     {1,0,0,0,0,0,0,1},
//     {1,0,1,1,1,1,0,1},
//     {1,0,1,0,0,1,0,1},
//     {1,0,1,0,0,1,0,1},
//     {1,0,1,1,1,1,0,1},
//     {1,0,0,0,0,0,0,1},
//     {1,1,1,1,1,1,1,1} },
// };

//turtle stuff
int turtlePatterns[2][nrOfSourcePins][nrOfSinkPins];
// int turtlePattern2[nrOfSourcePins][nrOfSinkPins];


enum directions { UP, RIGHT, DOWN, LEFT };
struct turtle_t {
  int currentDirection;
  int currentX;
  int currentY;
  int tracksLifespan;
  elapsedMillis sinceMove;
};

turtle_t turtles[2];


//text stuff
String currentText;
int textPattern[nrOfSourcePins][nrOfSinkPins];

//clock stuff
int clockPattern[nrOfSourcePins][nrOfSinkPins];
elapsedMillis clockCounter;
String clockString = "1337";
int clockHour = 13;
int clockMinute = 37;

//timeline stuff
int timelinePattern[nrOfSourcePins][nrOfSinkPins];

//pixel stuff
int pixelPattern[nrOfSourcePins][nrOfSinkPins];

// const int nrOfPatterns = sizeof(patterns)/sizeof(patterns[0]);
// int currentPatternIndex = 0;
int (*currentPattern) [nrOfSourcePins][nrOfSinkPins] = &turtlePatterns[1];

//Timing stuff
elapsedMillis sinceDisplay;
unsigned long displayInterval = 5000;
elapsedMillis sinceIncSinkPin;
unsigned long incSinkPinInterval = 2;

elapsedMillis sincePatternChange;
unsigned long patternChangeInterval = 60000;

void setup() {
  Serial.begin(115200);
  Serial1.begin(115200);

  initShiftRegister(&shifters[0]);
  initShiftRegister(&shifters[1]);

  // delay(4000);

  setFont(&Picopixel);

  //For safety. Set all pins to low!!!
  for(int i = 2; i < 40; ++i){
    pinMode(i, OUTPUT);
    digitalWrite(i, LOW);
  }

  //WUUUUHUUUUUU. Let's have some nice randomness!!
  randomSeed(analogRead(A22));


  //initialize turtle 0
  placeTurtleRandomly(&turtles[0], nrOfSinkPins, nrOfSourcePins);
  clearPattern(turtlePatterns[0]);
  turtles[0].tracksLifespan = 1000*60*3;
  turtlePatterns[0][turtles[0].currentY][turtles[0].currentX] = turtles[0].tracksLifespan;

  //initialize turtle 1
  placeTurtleRandomly(&turtles[1], nrOfSinkPins, nrOfSourcePins);
  clearPattern(turtlePatterns[1]);
  turtles[1].tracksLifespan = 100*60*3;
  turtlePatterns[1][turtles[1].currentY][turtles[1].currentX] = turtles[1].tracksLifespan;

  //initialize text
  clearPattern(textPattern);

  //initialize clockPattern
  clearPattern(clockPattern);

  //initialize timeline
  clearPattern(timelinePattern);

  //initialize pixelsPattern
  clearPattern(pixelPattern);


  //Lets have a button!!!
  // pinMode(buttonPin, INPUT_PULLUP);

  // initMultiplexer(plexers[0]);
  // initMultiplexer(plexers[1]);

  //configure display pins
  pinMode(21, OUTPUT);
  digitalWrite(21, LOW);
  pinMode(20, OUTPUT);
  digitalWrite(20, HIGH);
  // start the display
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3D (for the 128x64)
  // // init done
  // // Show image buffer on the display hardware.
  // // Since the buffer is intialized with an Adafruit splashscreen
  // // internally, this will display the splashscreen.
  // // Clear the buffer.
  display.clearDisplay();
  display.setCursor(0,0);
  display.setTextSize(5);
  display.setTextColor(WHITE);
  display.printf("IKEA");
  display.display();
  delay(200);

  // u8g2.setI2CAddress(0x3C);
  // u8g2.begin();

  // for(int i = 0; i < nrOfSourcePins; i++){
  //   pinMode(sourcePins[i], OUTPUT);
  //   digitalWrite(sourcePins[i], LOW);
  // }
  // digitalWrite(currentSourcePin, HIGH);

  for(int i = 0; i < nrOfSinkPins; i++){
    // pinMode(sinkPins[i], OUTPUT);
    // digitalWrite(sinkPins[i], LOW);
    setSinkPin(i, LOW);
  }

  // setSinkPin(currentSinkIndex, HIGH);

  // make it update the pin state immediately
  sinceIncSinkPin = incSinkPinInterval;

  // SoftPWMBegin(SOFTPWM_NORMAL);

  initializeCurrentMode();
}

elapsedMillis increaser;
unsigned long deltaTime = 0;

void loop() {
  //Check hardware serial
  if(Serial1.available()){
    inputString = Serial1.readStringUntil('\n');
    Serial.print("Received hardware string: ");
    Serial.println(inputString);
    handleReceivedString();
  }

  //Weird thing to have a kind of global counter for decreasing the patterns that need it.
  //Basically we let the increaser grow to a nice value before we actually apply it. Like filling upp a bucket and then pour from it.
  if(increaser > 10){
    deltaTime = increaser;
    increaser = 0;
  }else{
    deltaTime = 0;
  }

  //TODO: FIX so we no longer use multiplexer (the new bool). Use shiftregisters on sourcePins
  // bool buttonPressed = !digitalRead(buttonPin);
  // bool buttonPressed = false;
  if(sinceIncSinkPin > incSinkPinInterval && autoIncrementSinkPin){
    // if(buttonPressed){
    //   delay(400);
    // }
    sinceIncSinkPin = 0;

    if(!pinDiagnosis){
      //Turn off first bank
      setSinkPin(currentSinkIndex, LOW);
      //Turn off second bank
      setSinkPin(currentSinkIndex+nrOfCycledSinkPins, LOW);

      //Increment that shit
      currentSinkIndex++;
      currentSinkIndex %= nrOfCycledSinkPins;

      //First bank of sinkPins
      setSinkPin(currentSinkIndex, HIGH);

      for (size_t i = 0; i < nrOfSourcePins; i++) {
        // currentSourcePin = sourcePins[i];
        int shouldBeOn = (*currentPattern)[i][currentSinkIndex];
        setSourcePin(0, i, shouldBeOn);
        // digitalWrite(currentSourcePin, shouldBeOn);
      }
      shiftOutPinStates(&shifters[0]);

      //Second bank of sinkPins
      int currentSinkIndexSecondBank = currentSinkIndex + nrOfCycledSinkPins;
      setSinkPin(currentSinkIndexSecondBank, HIGH);

      for (size_t i = 0; i < nrOfSourcePins; i++) {
        // currentSourcePin = sourcePins[i];
        int shouldBeOn = (*currentPattern)[i][currentSinkIndexSecondBank];
        setSourcePin(1, i, shouldBeOn);
        // digitalWrite(currentSourcePin, shouldBeOn);
      }
      shiftOutPinStates(&shifters[1]);

    }else{
      setSinkPin(previousDiagnosisX, LOW);

      setSourcePin(0, previousDiagnosisY, LOW);
      setSourcePin(0, diagnosisY, HIGH);

      shiftOutPinStates(&shifters[0]);


      setSourcePin(1, previousDiagnosisY, LOW);
      setSourcePin(1, diagnosisY, HIGH);

      shiftOutPinStates(&shifters[1]);

      // if(currentSinkIndex == nrOfSinkPins){
      //   digitalWrite(currentSourcePin, LOW);
      //   currentSourceIndex++;
      //   currentSourceIndex %= nrOfSourcePins;
      //   currentSourcePin = sourcePins[currentSourceIndex];
      //   digitalWrite(currentSourcePin, HIGH);
      // }
      setSinkPin(diagnosisX, HIGH);
    }
  }

  if (sincePatternChange > patternChangeInterval && autoIncrementPattern) {
    // delay(250);
    // sincePatternChange = 0;
    // currentPatternIndex++;
    // currentPatternIndex %= nrOfPatterns;
    //
    // currentPattern = &patterns[currentPatternIndex];
  }

  //Handle all the different "modes"

  //sitting screen
  updateTurtleWhileSitting();
  decreasePattern(turtlePatterns[0], deltaTime);
  //transition screen
  decreasePattern(turtlePatterns[1], deltaTime);

  //Clock screen
  clearPattern(clockPattern);
  drawClockString(clockString, clockPattern);
  updateClock();
  updateTimeline();

  //text screen
  clearPattern(textPattern);
  drawString(0,0,currentText, 1, textPattern);

  //Display shiiiit!
  if(sinceDisplay >= displayInterval){
    // u8g2.firstPage();
    // u8g2.setFont(u8x8_font_chroma48medium8_r);
    // u8g2.drawStr(0,20,"Hello World!");
    // Serial.printf("Sending to display\n");

    sinceDisplay = 0;


    display.clearDisplay();

    int circleSize = display.height()/nrOfSinkPins/2;

    if(!pinDiagnosis){
      drawPattern(0,0,display.width(),display.height(), *currentPattern);
    }else{
      display.setCursor(0,0);
      display.setTextSize(2);
      display.printf("diagnosis!\n");
      // int incDriverCountdown = (incSinkPinInterval - sinceIncSinkPin)/1000;
      // display.printf("I:%i \n", incDriverCountdown);
      // display.printf("src (Y):%i\n", diagnosisY);
      // display.printf("snk (X):%i\n", diagnosisX);
    }
    display.display();
    int time = sinceDisplay;
    Serial.printf("lcd stuff took %i ms\n", time);
  }
}

// TODO: make it clear timeline when new day. Or. Implement clear pixel when timeline jumps to new x,y.

int currentTimelineX;
int currentTimelineY;
int timelineLifespan = 30;
void updateTimeline(){
  currentTimelineX = clockHour;
  // TODO: Check if we scale the minutes to pixels properly. Not suuuper important as long as it's in the right ball park...
  currentTimelineY = clockMinute * 16 / 60;
}

void markActivityTimeline(){
  timelinePattern[nrOfSourcePins-1-currentTimelineY][currentTimelineX] = timelineLifespan;
}

void unmarkActivityTimeline(){
  timelinePattern[nrOfSourcePins-1-currentTimelineY][currentTimelineX] = 0;
}

void updateClock(){
  if(clockCounter > 60*1000){
    clockCounter -= (60 * 1000);

    clockMinute++;
    if(clockMinute >= 60){
      clockMinute = 0;
      clockHour++;
      clockHour %= 24;


      clockString.setCharAt(0, '0' + (clockHour/10));
      if(clockHour < 10){
        clockString.setCharAt(1, '0'+clockHour);
      }else{
        clockString.setCharAt(1, '0' + clockHour%10);
      }
    }

    clockString.setCharAt(2, '0' + (clockMinute/10));
    if(clockMinute < 10){
      clockString.setCharAt(3, '0'+clockMinute);
    }else{
      clockString.setCharAt(3, '0' + clockMinute%10);
    }

    // updateTimeline();

    Serial.printf("new minute! clock is %i:%i\n", clockHour, clockMinute);
  }
}

void initializeCurrentMode(){
  switch (currentMode) {
    case TEXTMODE:
      Serial.printf("text screen\n");
      currentPattern = &textPattern;
      break;
    case CLOCKMODE:
      Serial.printf("clock screen\n");
      currentPattern = &clockPattern;
      break;
    case TURTLEWHILESITTING:
      Serial.printf("sitting screen\n");
      currentPattern = &turtlePatterns[0];
      break;
    case TURTLEONSTATECHANGE:
      Serial.printf("state change screen\n");
      currentPattern = &turtlePatterns[1];
      break;
    case TIMELINE:
      Serial.printf("timeline screen\n");
      currentPattern = &timelinePattern;
      break;
    case PIXELSCREEN:
      Serial.printf("pixel screen\n");
      currentPattern = &pixelPattern;
      break;
  }
}
bool isSitting = false;
void updateTurtleWhileSitting(){
  int turtleIndex = 0;
  if(turtles[turtleIndex].sinceMove > 1000*60){
    turtles[turtleIndex].sinceMove -= 1000*60;
    if(isSitting){

      updateTurtleDirection(&turtles[turtleIndex]);
      moveTurtle(&turtles[turtleIndex], nrOfSinkPins, nrOfSourcePins);
      // clearPattern(turtlePatterns[turtleIndex]);
      Serial.printf("turtle[%i] position: %i,%i \tdirection: %i \n",turtleIndex, turtles[turtleIndex].currentX, turtles[turtleIndex].currentY, turtles[turtleIndex].currentDirection);
      turtlePatterns[turtleIndex][turtles[turtleIndex].currentY][turtles[turtleIndex].currentX] = turtles[turtleIndex].tracksLifespan;
    }
    //We should have the tracksLifespan keep active even when sitting still.
    //Hmm. I think. Maybe not....

  }
}

void updateTurtleOnStateChange(){
  int turtleIndex = 1;
  updateTurtleDirection(&turtles[turtleIndex]);
  moveTurtle(&turtles[turtleIndex], nrOfSinkPins, nrOfSourcePins);
  // clearPattern(turtlePatterns[turtleIndex]);
  turtlePatterns[turtleIndex][turtles[turtleIndex].currentY][turtles[turtleIndex].currentX] = turtles[turtleIndex].tracksLifespan;
  Serial.printf("turtle[%i] position: %i,%i \tdirection: %i \n",turtleIndex, turtles[turtleIndex].currentX, turtles[turtleIndex].currentY, turtles[turtleIndex].currentDirection);
}

void handleReceivedString(){
  char command = inputString[0];
  inputString.remove(0, 1);
  int x, y;
  switch (command) {
    case 'm':
      currentMode = inputString.toInt();
      char str[15];
      inputString.toCharArray(str, 15);
      Serial.printf("inputString is %s. Changing mode to: %i \n", str, currentMode);
      initializeCurrentMode();
      break;
    case 's':
      // drawString(0,7,inputString, 1, textPattern);
      currentText = inputString;
      Serial.print("print requested: ");
      Serial.println(inputString);
      break;
    case 'i':
      if(inputString[0] == '1'){
        isSitting = !isSitting;
        updateTurtleWhileSitting();
        Serial.printf("isSitting is: %i\n", (int)isSitting);
      }else if(inputString[0] == '2'){
        updateTurtleOnStateChange();
      }else if(inputString[0] == '3'){
        markActivityTimeline();
      }
      break;
    case 'c':
      clockString = inputString;
      clockCounter = 0;
      clockHour = clockString.substring(0, 2).toInt();
      clockMinute = clockString.substring(2, 4).toInt();
      Serial.printf("Setting clock!\n");
      Serial.printf("hours: %i\n", clockHour);
      Serial.printf("minute: %i\n", clockMinute);
      break;
    case 'f':
      if(inputString[0] == '1'){
        setFont(NULL);
      }else if(inputString[0] == '2'){
        setFont(&Org_01);
      }else if(inputString[0] == '3'){
        setFont(&TomThumb);
      }else if(inputString[0] == '4'){
        setFont(&Picopixel);
      }else if(inputString[0] == '5'){
        setFont(&Tiny3x3a2pt7b);
      }

      break;
    case 'p':
      x = inputString.substring(0, 2).toInt();
      y = inputString.substring(2, 4).toInt();
      bool state;
      if(inputString.length() > 4){
        state = inputString.substring(4, 5).toInt();
      }else{
        // Retrieve the current state from the pixelPattern and use the inverted value as input
        state = !pixelPattern[y][x];
      }
      setPixel(x, y, state);
      Serial.printf("Setting pixel %i, %i to %i", x,y,state);
      break;
    case 'd':
      if(inputString.length() < 1){
        pinDiagnosis = !pinDiagnosis;
        Serial.printf("Setting pinDiagnosis to %i\n", pinDiagnosis);
        break;
      }
      Serial.printf("Changing diangosis pins!\n");
      x = inputString.substring(0, 2).toInt();
      y = inputString.substring(2, 4).toInt();
      previousDiagnosisX = diagnosisX;
      previousDiagnosisY = diagnosisY;
      diagnosisX = x;
      diagnosisY = y;
      break;
    case 'x':
      clearPattern(pixelPattern);
      Serial.printf("Clearing pixelpattern\n");
      break;
  }

  // inputString.toCharArray(command, sizeof(command));
  // drawChar(0,0, command[0], 1);
  // drawString(0,0,inputString, 1);
  // Serial.println(inputString);

  // while (Serial.available()) {
  //   // get the new byte:
  //   char inChar = (char)Serial.read();
  //   // add it to the inputString:
  //   inputString += inChar;
  //   // if the incoming character is a newline, set a flag so the main loop can
  //   // do something about it:
  //   if (inChar == '\n') {
  //     stringComplete = true;
  //   }
  // }
}

void serialEvent() {
  inputString = Serial.readStringUntil('\n');
  Serial.print("Received string: ");
  Serial.println(inputString);
  handleReceivedString();

}

void decreasePattern(int pattern[nrOfSourcePins][nrOfSinkPins], int amount){
  for (size_t i = 0; i < nrOfSourcePins; i++) {
    for (size_t j = 0; j < nrOfSinkPins; j++) {
      pattern[i][j] -= amount;
      if(pattern[i][j] < 0){
        pattern[i][j] = 0;
      }
    }
  }
}

void clearPattern(int pattern[nrOfSourcePins][nrOfSinkPins]){
  for (size_t i = 0; i < nrOfSourcePins; i++) {
    for (size_t j = 0; j < nrOfSinkPins; j++) {
      pattern[i][j] = 0;
    }
  }
}

//visualize pattern
//Let's have sinkPins along x axis and sourcePins along y axis
void drawPattern(int originX, int originY, int width, int height, int pattern[nrOfSourcePins][nrOfSinkPins]){
  int circleSize = min(width/nrOfSinkPins, height/nrOfSourcePins)/2;//height/nrOfSourcePins/2;

  // display.drawCircle(124, 60, 4, WHITE);

  // int y = originY + circleSize;
  //loop through rows (y axis)
  for (size_t i = 0; i < nrOfSourcePins; i++) {
    // int x = originX + circleSize;

    //loop through columns (x axis)
    for (size_t j = 0; j < nrOfSinkPins; j++) {

      //Rather than adding an offset for each index, we calculate the position every time. This is to avoid growing rounding errors
      //order here is important. multiply by j and i first. Then divide by nr of pins. Otherwise rounding error will propagate.
      int x = originX + circleSize + j*width/nrOfSinkPins;
      int y = originY + circleSize + i*height/nrOfSourcePins;

      if(pattern[i][j]){
        display.fillCircle(x, y, circleSize, WHITE);
      }else{
        // display.drawCircle(x, y, circleSize, WHITE);
      }

      // x += width/nrOfSourcePins;
    }

    // y += height/nrOfSinkPins;
  }
}

void drawString(int x, int y, String str, uint8_t size, int pattern[nrOfSourcePins][nrOfSinkPins]){
  // drawChar(x,y,str[0],size);
  // return;
  clearPattern(pattern);

  int startX = x;
  if(currentFont){//If custom font we should offset y to baseline for the font
    y += (uint8_t)pgm_read_byte(&currentFont->yAdvance) - 1;
  }
  for(int i = 0; i < str.length(); i++){
    char c = str[i];
    // Serial.printf("printing char code: %i\n", (uint8_t) c);
    // delay(500);
    if(!currentFont){


      if(c == '\n'){
        y += size*8;
        x = startX;
      }else if(c == '\t'){
        x += size*6;
      }else if(c != '\r'){
        drawChar(x, y, c, size, pattern);
        x += size*6;
      }
    }else { // Custom font
      if(c == '\n') {
        x  = startX;
        y += size * (uint8_t)pgm_read_byte(&currentFont->yAdvance);
      } else if(c != '\r') {
        uint8_t first = pgm_read_byte(&currentFont->first);
        if((c >= first) && (c <= (uint8_t)pgm_read_byte(&currentFont->last))) {
          uint8_t   c2    = c - pgm_read_byte(&currentFont->first);
          GFXglyph *glyph = &(((GFXglyph *)pgm_read_pointer(&currentFont->glyph))[c2]);
          uint8_t   w     = pgm_read_byte(&glyph->width),
                    h     = pgm_read_byte(&glyph->height);
          if((w > 0) && (h > 0)) { // Is there an associated bitmap?
            int16_t xo = (int8_t)pgm_read_byte(&glyph->xOffset); // sic
            if((x + size * (xo + w)) >= nrOfSinkPins) {
              // Serial.printf("wrapping! x: %i, size: %i, xo: %i, w: %i, nrOfSinkPins: %i\n", x, size, xo, w, nrOfSinkPins);
              // Drawing character would go off right edge; wrap to new line
              x  = 0;
              y += size * (uint8_t)pgm_read_byte(&currentFont->yAdvance);
            }
            drawChar(x, y, c, size, pattern);
          }
          x += pgm_read_byte(&glyph->xAdvance) * size;
        }
      }
    }
  }
}

//Expects four numbers in a string. They will be printed in the format XX:XX
void drawClockString(String str, int pattern[nrOfSourcePins][nrOfSinkPins]){
  // drawChar(x,y,str[0],size);
  // return;
  //We want a specific font and layout for the clockj visualization
  GFXfont* tempFont = currentFont;
  setFont(&TomThumb);
  if (str.length() < 4) {
    Serial.printf("invalid clockstring. It must be at least of length 4. Length is %i\n", str.length());
    return;
  }

  int x = 0;
  int y = 10;


  drawChar(x, y, str[0], 1, pattern);
  x += 5;
  drawChar(x, y, str[1], 1, pattern);
  x += 10;

  drawChar(x, y, str[2], 1, pattern);
  x += 5;
  drawChar(x, y, str[3], 1, pattern);

  int commaXPosition = nrOfSinkPins/2 - 1;
  drawChar(commaXPosition, y, ':', 1, pattern);

  setFont(tempFont);

}


// Draw a character
// TODO: Check so we don't go out of bounds
// TODO: Make it draw from top left of font.
// TODO: Handle different size arguments
// TODO: Handle that tomThumbs is off by one pixel on y-axis.
void drawChar(int16_t x, int16_t y, unsigned char c, uint8_t size, int pattern[nrOfSourcePins][nrOfSinkPins]) {
  if(!currentFont){
    if((x >= nrOfSinkPins)            || // Clip right
       (y >= nrOfSourcePins)           || // Clip bottom
       ((x + 6 * size - 1) < 0) || // Clip left
       ((y + 8 * size - 1) < 0))   // Clip top
      return;

    // if(!_cp437 && (c >= 176)) c++; // Handle 'classic' charset behavior

    for(int8_t i=0; i<6; i++ ) {
      if(x+i >= nrOfSinkPins){
        return;
      }
      uint8_t line;
      if(i < 5) line = pgm_read_byte(font+(c*5)+i);
      else      line = 0x0;
      for(int8_t j=0; j<8; j++, line >>= 1) {
        if(y+j >= nrOfSourcePins){
          return;
        }
        if(line & 0x1) {
          // if(size == 1) drawPixel(x+i, y+j, color);
          // else          fillRect(x+(i*size), y+(j*size), size, size, color);
          if(x+i < nrOfSinkPins && y+i < nrOfSourcePins) pattern[y+j][x+i] = true;
          // else          fillRect(x+(i*size), y+(j*size), size, size, color);
        }else{
          if(x+i < nrOfSinkPins && y+i < nrOfSourcePins) pattern[y+j][x+i] = false;
        }
      }
    }
  }else { // Custom font
    // Serial.printf("drawing custom font\n");

   // Character is assumed previously filtered by write() to eliminate
   // newlines, returns, non-printable characters, etc.  Calling drawChar()
   // directly with 'bad' characters of font may cause mayhem!

   c -= pgm_read_byte(&currentFont->first);
   GFXglyph *glyph  = &(((GFXglyph *)pgm_read_pointer(&currentFont->glyph))[c]);
   uint8_t  *bitmap = (uint8_t *)pgm_read_pointer(&currentFont->bitmap);

   uint16_t bo = pgm_read_word(&glyph->bitmapOffset);
   uint8_t  w  = pgm_read_byte(&glyph->width),
            h  = pgm_read_byte(&glyph->height) /*,
            xa = pgm_read_byte(&glyph->xAdvance) */;
   int8_t   xo = pgm_read_byte(&glyph->xOffset),
            yo = pgm_read_byte(&glyph->yOffset);
   uint8_t  xx, yy, bits = 0, bit = 0;
   int16_t  xo16 = 0, yo16 = 0;

   if(size > 1) {
     xo16 = xo;
     yo16 = yo;
   }

   // Todo: Add character clipping here

   // NOTE: THERE IS NO 'BACKGROUND' COLOR OPTION ON CUSTOM FONTS.
   // THIS IS ON PURPOSE AND BY DESIGN.  The background color feature
   // has typically been used with the 'classic' font to overwrite old
   // screen contents with new data.  This ONLY works because the
   // characters are a uniform size; it's not a sensible thing to do with
   // proportionally-spaced fonts with glyphs of varying sizes (and that
   // may overlap).  To replace previously-drawn text when using a custom
   // font, use the getTextBounds() function to determine the smallest
   // rectangle encompassing a string, erase the area with fillRect(),
   // then draw new text.  This WILL infortunately 'blink' the text, but
   // is unavoidable.  Drawing 'background' pixels will NOT fix this,
   // only creates a new set of problems.  Have an idea to work around
   // this (a canvas object type for MCUs that can afford the RAM and
   // displays supporting setAddrWindow() and pushColors()), but haven't
   // implemented this yet.

   for(yy=0; yy<h; yy++) {
    //  bit = 0;
    if(currentFont == &TomThumb){
      bits = pgm_read_byte(&bitmap[bo++]);
    }
    for(xx=0; xx<w; xx++) {
       if(!(currentFont == &TomThumb) && !(bit++ & 7)) {// super cool masking to only render true when coming to last bit...
         bits = pgm_read_byte(&bitmap[bo++]);
       }
       if(bits & 0x80) {
         if(x+xo+xx < nrOfSinkPins && y+yo+yy < nrOfSourcePins) {
          pattern[y+yo+yy][x+xo+xx] = true;
         } else {
          //  fillRect(x+(xo16+xx)*size, y+(yo16+yy)*size, size, size, color);
         }
       }else{
         if(x+xo+xx < nrOfSinkPins && y+yo+yy < nrOfSourcePins) pattern[y+yo+yy][x+xo+xx] = false;
       }
       bits <<= 1;
     }
   }

 } // End classic vs custom font
}

void setFont(const GFXfont *f) {
  // if(f) {          // Font struct pointer passed in?
  //   if(!currentFont) { // And no current font struct?
  //     // Switching from classic to new font behavior.
  //     // Move cursor pos down 6 pixels so it's on baseline.
  //     cursor_y += 6;
  //   }
  // } else if(currentFont) { // NULL passed.  Current font struct defined?
  //   // Switching from new to classic font behavior.
  //   // Move cursor pos up 6 pixels so it's at top-left of char.
  //   cursor_y -= 6;
  // }
  currentFont = (GFXfont *)f;
}

void setPixel(int x, int y, bool state){
  pixelPattern[y][x] = state;
}

void initShiftRegister(shiftRegister* shifter){
  pinMode(shifter->clockPin, OUTPUT);
  pinMode(shifter->dataPin, OUTPUT);
  pinMode(shifter->latchPin, OUTPUT);
  uint32_t initData = 85;

  digitalWrite(shifter->clockPin, LOW);
  for (int i = 0; i < shifter->nrOfLinkedRegisters; i++) {
    shiftOut(shifter->dataPin, shifter->clockPin, MSBFIRST, initData);
  }
  digitalWrite(shifter->latchPin, LOW);
  delayMicroseconds(5);
  digitalWrite(shifter->latchPin, HIGH);
  delayMicroseconds(5);
  digitalWrite(shifter->latchPin, LOW);
}

void setShiftRegisterPin(shiftRegister* shifter, int index, int state){
  //Safety measure
  if(!(index < shifter->nrOfLinkedRegisters*8)){
    Serial.printf("index higher than shiftRegister size!\n");
    return;
  }
  shifter->pins[index] = state;
  // Serial.printf("shift struct pin %i set to %i\n",index, shifter->pins[index]);
}

void shiftOutPinStates(shiftRegister* shifter){
  digitalWrite(shifter->clockPin, LOW);
  for (int i = 0; i < shifter->nrOfLinkedRegisters; i++) {
    //Shiftout one linked register here!
    uint8_t data = 0;
    for (int j = 0; j < 8; j++) {
      // Serial.printf("bitshifting the data uint with %i from index %i\n", shifter->pins[j+8*i], j+8*i);
      data = (data << 1) + (uint8_t) shifter->pins[j+8*i];
    }
    shiftOut(shifter->dataPin, shifter->clockPin, MSBFIRST, data);
    // Serial.printf("shifting out %i to shifter with clockPin %i\n", data, shifter->clockPin);
  }
  digitalWrite(shifter->latchPin, LOW);
  delayMicroseconds(5);
  digitalWrite(shifter->latchPin, HIGH);
  delayMicroseconds(5);
  digitalWrite(shifter->latchPin, LOW);
}

void setSourcePin(int sourceBank, int pinIndex, int state){
  if(useShiftRegisters){
    if(sourceBank == 0){
      setShiftRegisterPin(&shifters[0], pinIndex, state);
      // Serial.printf("setting register 0 shiftpin %i to %i\n", pinIndex, state);
    }else{
      setShiftRegisterPin(&shifters[1], pinIndex, state);
      // Serial.printf("setting register 1 shiftpin %i to %i\n", pinIndex, state);
    }

  }
}

void initMultiplexer(analogMultiplexer plexer){
  pinMode(plexer.gnd, OUTPUT);
  digitalWrite(plexer.gnd, LOW);
  pinMode(plexer.vcc, OUTPUT);
  digitalWrite(plexer.vcc, HIGH);
  pinMode(plexer.en, OUTPUT);
  digitalWrite(plexer.en, LOW);

  for (size_t i = 0; i < 4; i++) {
    pinMode(plexer.bits[i], OUTPUT);
    digitalWrite(plexer.bits[0], LOW);
  }

  pinMode(plexer.z, OUTPUT);
  digitalWrite(plexer.z, LOW);
}

void setMultiplexerPin(int pin){
  int plexerIndex = pin/16;
  // Serial.printf("plexerIndex: %i\n", plexerIndex);
  //subtract 16 for each time we use next 16 channel multiplexer

  pin -= plexerIndex * 16;
  // Serial.printf("plexerPin: %i\n", pin);
  for (size_t i = 0; i < 4; i++) {
    if(pin & 0x1){
      digitalWrite(plexers[plexerIndex].bits[i], HIGH);
      // Serial.printf("setting plexerbit %i high (pin %i)\n", i, plexers[plexerIndex].bits[i]);
    }else{
      digitalWrite(plexers[plexerIndex].bits[i], LOW);
      // Serial.printf("setting plexerbit %i low (pin %i)\n", i, plexers[plexerIndex].bits[i]);
    }
    pin >>= 1;
  }
}

void multiplexerWrite(int pinIndex, int state){
  int plexerIndex = pinIndex/16;
  digitalWrite(plexers[plexerIndex].z, LOW);
  setMultiplexerPin(pinIndex);
  digitalWrite(plexers[plexerIndex].z, state);
}

void setSinkPin(int pinIndex, int state){
  if(!(pinIndex < nrOfSinkPins)){
    Serial.printf("invalid pinIndex for sinkPin. %i\n", pinIndex);
    return;
  }

  if(useMultiplexers){
    multiplexerWrite(pinIndex, state);
    return;
  }

  //Normal mode with direct pin interaction
  digitalWrite(sinkPins[pinIndex], state);
}

void placeTurtleRandomly(turtle_t *turtle, int width, int height){
  turtle->currentX = random(width);
  turtle->currentY = random(height);
  turtle->currentDirection = random(4);
}

//TODO: Handle whether we should avoid wall or wrap around.
void updateTurtleDirection(turtle_t *turtle){
  turtle->currentDirection += (random(3)-1);
  if(turtle->currentDirection < 0){
    turtle->currentDirection = 3;
  }else if(turtle->currentDirection > 3){
    turtle->currentDirection = 0;
  }
}

void moveTurtle(turtle_t *turtle, int width, int height){
  switch (turtle->currentDirection) {
    case UP:
      turtle->currentY--;
      break;
    case DOWN:
      turtle->currentY++;
      break;
    case RIGHT:
      turtle->currentX++;
      break;
    case LEFT:
      turtle->currentX--;
      break;
  }
  //As of now, we wrap around. Maybe would be cooler with bounce
  turtle->currentX < 0 ? turtle->currentX += width : turtle->currentX %= width;
  turtle->currentY < 0 ? turtle->currentY += height : turtle->currentY %= height;
}
