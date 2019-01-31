/*
   Author: Lewis Mason
   This Code is used to control a "Innovadyne Syringe Drive"
   motTOP corresponds
    to the motor that controls the syringe.
   motBOT corresponds to
    the motor that controls the three way valve.
*/

#include <LiquidCrystal.h> //used to control the LCD display

//variables for motTop
int range = 2020 / 2;   //the number of steps from 0uL to 250uL (2020/2 for full stepping)
int totalVol = 250;   //total volume of the syringe in microLitres
int speedTOP = 10;     //delay in ms between each step

float uLPerStep;      //ratio of microLitres per step on the syringe
float stepPeruL;      //ratio of steps per microLitre on the syringe
boolean up;           //true if the current direction is up
int pos = 0;
//float flowRate = DEFAULTFLOWRATE;
#define DEFAULTFLOWRATE = 1; //to be changed

//This value is used to correct the volume of the syringe.
//The reasoning for this came from repeated tests that resulted in a volume too large.
//Ex: 4mL gave 4.2mL. so (4-4.2)/4 is the correction factor. Negative means produce less, positive means
//produce more. If the value is zero then there is no correction.
float correctionFactor = 0; //-0.05

//variables for motBot
int state = 0;        //provides what state the three way valve is in
int speedBOT = 4;    //delay in ms between each step (needs to be around 10 due to torsional friction)

//variables for both
#define motTOP true
#define motBOT false

//directional constants for calling functions. Note that CW corresponds to up and CCW corresponds to down on motTOP
#define UP true
#define DOWN false
#define CW true
#define CCW false

//control constants, corresponding to arduino pins that control each of the L297's inputs
#define RESETBOT 2  //sets the top motor to L297 datasheet state ABCD = 0101, rising edge
#define CLKBOT 3    //clk signal, rising edge
#define CWCCWBOT 4  //sets the direction, true = CW, false = CCW
#define ENABLEBOT 5 //enables motor rotation
#define RESETTOP 6
#define CLKTOP 7
#define CWCCWTOP 8
#define ENABLETOP 9

//analog inputs from the photoresistors (encoders)
int encoderBOT = A1;

//pinouts defined for the LCD display's operation
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

int lcd_key     = 0;
int adc_key_in  = 0;
int screenPos = 0;   //home is defined as 0. Each screenPos has both an even and odd screen (first and second)
int cursorPos = 0;
boolean recentChange = false; //this is used to determine when to print new screens to the LCD display
boolean evenScreen = true;    //used to determine what polarity of screen the user is on
boolean returnFlag = false;
#define BLANKSCREEN "                "
#define RIGHT true
#define LEFT false
#define btnRIGHT  0
#define btnUP     1
#define btnDOWN   2
#define btnLEFT   3
#define btnSELECT 4
#define btnNONE   5

//states for the bottom motor
#define NE 0
#define NW 1
#define SW 2
#define SE 3
#define inc true
#define dec false

byte rightArrow[8] = {
  B01000,
  B01100,
  B01110,
  B01111,
  B01110,
  B01100,
  B01000,
};

byte leftArrow[8] = {
  B00010,
  B00110,
  B01110,
  B11110,
  B01110,
  B00110,
  B00010,
};

void setup() {

  Serial.begin(9600);

  //testing pin inputs
  pinMode(10, INPUT);
  pinMode(11, INPUT);
  pinMode(12, INPUT);
  pinMode(13, INPUT);

  //pins to control the L297's
  pinMode(RESETTOP, OUTPUT);
  pinMode(ENABLETOP, OUTPUT);
  pinMode(CWCCWTOP, OUTPUT);
  pinMode(CLKTOP, OUTPUT);
  pinMode(RESETBOT, OUTPUT);
  pinMode(ENABLEBOT, OUTPUT);
  pinMode(CWCCWBOT, OUTPUT);
  pinMode(CLKBOT, OUTPUT);

  digitalWrite(ENABLETOP, HIGH);  //enable motor
  digitalWrite(CWCCWTOP, HIGH);   //Set initial direction to CW
  digitalWrite(CLKTOP, HIGH);     //set clk high
  digitalWrite(RESETTOP, HIGH);   //set reset signal high

  digitalWrite(ENABLEBOT, HIGH);  //enable motor
  digitalWrite(CWCCWBOT, HIGH);   //Set initial direction to CW
  digitalWrite(CLKBOT, HIGH);     //set clk high
  digitalWrite(RESETBOT, HIGH);   //set reset signal high

  //volume corrections
  range = range + range * correctionFactor;
  uLPerStep = (float) totalVol / (float) range;
  stepPeruL = (float) range / (float) totalVol;

  //LCD operations
  lcd.createChar(0, rightArrow);
  lcd.createChar(1, leftArrow);
  lcd.begin(16, 2);              // start LCD library
  recentChange = true;
}

//********************************************MAIN LOOP**********************************************//
//the main menu screen
void loop() {
  lcd_key = read_LCD_buttons(); //figure out what button was just pressed

  //this is used to update the LCD display when the page has been scrolled or if the cursor position has moved
  if (recentChange) {
    printScreen(0,"<   Operate    >",0,"<  Calibrate   >");
    recentChange = false;

    switch (cursorPos) {
      case 0:
        printArrows(0,3,11);
        removeArrows(1,2,12);
        break;
      case 1:
        removeArrows(0,3,11);
        printArrows(1,2,12);
        break;
      default:
        break;
    }
    
  }

  //determine what to do when a button is pressed
  switch (lcd_key) {
    case btnRIGHT:
      moveCursor(inc);
      break;
    case btnLEFT:
      moveCursor(dec);
      break;
    case btnUP:
      moveCursor(inc);
      break;
    case btnDOWN:
      moveCursor(dec);
      break;
    case btnSELECT:
      switch (cursorPos) {
        case 0:
          resetLCDVars();
          operate();
          break;
        case 1:
          resetLCDVars();
          calibrate();
          break;
      }
      break;
      
    default:
      break;
  }

  cursorBounds(-1,2);
  Serial.println(cursorPos);
}
//********************************************MAIN LOOP END SIDE LOOPS BEGIN*******************************//

void operate() {
  while(true) {
    
    lcd_key = read_LCD_buttons(); //figure out what button was just pressed
  
    if (recentChange) {
     screenBounds(-1,3); 
      switch (screenPos) {
        case 0: printFullScreen("<  Operation   >","      Menu       ","<     Back     >","  Press Select  ");
          break;
        case 1: printFullScreen("<   Automatic  >","  Press Select  ","<    Manual    >","  Press Select  ");
          break;
        default:
          break;
      }
      recentChange = false;
    }

    switch (lcd_key) {
      case btnRIGHT:
        shiftScreen(RIGHT);
        break;

      case btnLEFT:
        shiftScreen(LEFT);
        break;

      case btnSELECT:
        switch (screenPos) {
          case 0:
            if (evenScreen) {
              //do nothing on this screen
            } else {
              //exit operation menu
              resetLCDVars();
              return;
            }
            break;
          case 1:
            if (evenScreen) {
              automaticMoveSyringe();
            } else {
              manualMoveSyringe();
            }
            break;
        }
        break;
      
      default:
        break;
    }
  }
}

void calibrate() {
  while(true) {
    
    lcd_key = read_LCD_buttons(); //figure out what button was just pressed
  
    if (recentChange) {
     screenBounds(-1,3); 
      switch (screenPos) {
        case 0: printFullScreen("< Calibration  >","      Menu       ","<     Back     >","  Press Select  ");
          break;
        case 1: printFullScreen("<   Default    >","  Press Select  ","< Move Syringe >","  Press Select  ");
          break;
        case 2:
          printLine(0,1,BLANKSCREEN);
          printLine(16,1,BLANKSCREEN);
          printFullScreen("<  Reset Zero  >"," Position = " + (String) pos,"< Reset Range  >","  Range = " + (String)range);
          break;
        default:
          break;
      }
      recentChange = false;
    }

    switch (lcd_key) {
      case btnRIGHT:
        shiftScreen(RIGHT);
        break;

      case btnLEFT:
        shiftScreen(LEFT);
        break;

      case btnSELECT:
        switch (screenPos) {
          case 0:
            if (evenScreen) {
              //do nothing on this screen
            } else {
              //exit calibration menu
              resetLCDVars();
              return;
            }
            break;
          case 1:
            if (evenScreen) {
              setDefaultSettings();
              printLine(0,1,"    Success     ");
              delay(500);
              recentChange = true;
              
            } else {
              manualMoveSyringe();
            }
            break;
          case 2:
            if (evenScreen) {
              resetZero();
              printLine(0,1,"    Success     ");
              delay(500);
              recentChange = true;
            } else {
              resetRange();
              printLine(16,1,"    Success     ");
              delay(500);
              recentChange = true;
            }
            break;
        }
        break;
      
      default:
        break;
    }
  }
}

void manualMoveSyringe() {
  
}

void automaticMoveSyringe() {
  
}
//------------------------//
//     LCD functions      //
//------------------------//

void printArrow(int row, int col, boolean direction) {
  lcd.setCursor(col,row);
  if (direction) {
    lcd.write(byte(0));
  } else {
    lcd.write(byte(1));
  }
}

void printArrows(int row, int x0, int x1) {
  lcd.setCursor(x0,row);
  lcd.write(byte(0));
  lcd.setCursor(x1,row);
  lcd.write(1);
}

void removeArrows(int row, int x0, int x1) {
  lcd.setCursor(x0,row);
  lcd.write(' ');
  lcd.setCursor(x1,row);
  lcd.write(' ');
}


void cursorBounds(int low, int high) {
  if (cursorPos <= low ) {
    cursorPos = high-1;
  } else if (cursorPos >= high) {
    cursorPos = 0;
  }
}

void moveCursor(boolean direction) {
  if (direction == inc) {
    cursorPos++;
  } else {
    cursorPos--;
  }
  recentChange = true;
}

void setDefaultSettings() {
  range = 2020/2;
  uLPerStep = (float) totalVol / (float) range;
  stepPeruL = (float) range / (float) totalVol;
  pos = 0;
//  flowRate = DEFAULTFLOWRATE;
}

void resetLCDVars() {
  recentChange = true;
  cursorPos = 0;
  evenScreen = true;
  screenPos = 0;
  delay(250);
  lcd.clear();
}

//the number of integers between small and big (small is always -1 for this application) determines the number of screens
void screenBounds (int small, int big) {
  //edge cases for wrapping the screen. The first value is always -1, the second value is determined
    //depending on how long the current menu list is. For example if there are 3 screen positions (6 pages)
    //then the second value will be 3 (0 indexed)
    if (screenPos <= small) {
      screenPos = big-1;
      evenScreen = false;
    } else if (screenPos >= big) {
      screenPos = 0;
      evenScreen = true;
    }
}

//shifts the screen to the next page
void shiftScreen(boolean direction) {
  switch (direction) {
    case RIGHT:
      scrollNum(16,20, RIGHT);
      if (!evenScreen) {
        scrollNum(8,0,RIGHT);
        screenPos++;
        recentChange = true;
      }
      evenScreen = !evenScreen;
      break;
      
    case LEFT:
      scrollNum(16,20,LEFT);
      if (evenScreen) {
        scrollNum(8,0,LEFT);
        screenPos--;
        recentChange = true;
      }
      evenScreen = !evenScreen;
      break;
      
    default:
      break;
  }
}

//scrolls the text on the LCD 
void scrollNum(int num,int scrollDelay, boolean direction) {
    if (direction == RIGHT) {
      for (int i = 0; i < num; i++) {
        lcd.scrollDisplayLeft();
        delay(scrollDelay);
      }
    } else {
      for (int i = 0; i < num; i++) {
        lcd.scrollDisplayRight();
        delay(scrollDelay);
      }
    }
}

//prints the desired screen
void printScreen(int x0, String label0, int x1, String label1) {
  lcd.setCursor(x0,0);
  lcd.print(label0);
  lcd.setCursor(x1,1);
  lcd.print(label1);
}

void printLine(int x, int y, String message) {
  lcd.setCursor(x,y);
  lcd.print(message);
}

//prints 2 pages (full screen);
void printFullScreen (String label0, String label1, String label2, String label3) {
  printScreen(0,label0,0,label1);
  printScreen(16,label2,16,label3);
}

//determines which button has been pressed on the LCD screen
int read_LCD_buttons()
{
 adc_key_in = analogRead(0);      // read the value from the sensor 
 if (adc_key_in > 1000) return btnNONE; 
 if (adc_key_in < 50)   return btnRIGHT;  
 if (adc_key_in < 195)  return btnUP; 
 if (adc_key_in < 380)  return btnDOWN; 
 if (adc_key_in < 555)  return btnLEFT; 
 if (adc_key_in < 790)  return btnSELECT;   
 return btnNONE;  
}

//------------------------//
//     Motor functions    //
//------------------------//

//takes a step with the selected motor
void takeStep(boolean MOT) {

  if (MOT == motTOP) {  //TOP

    calcSpeed();
    Serial.println(speedTOP);
    digitalWrite(CLKTOP, LOW);
    digitalWrite(CLKTOP, HIGH);
    delay(speedTOP);
    movPos();

  } else {    //BOT
    digitalWrite(CLKBOT, LOW);
    digitalWrite(CLKBOT, HIGH);
    delay(speedBOT);
  }
}

//takes a user specified number of steps on a specific motor
void takeSteps(int numSteps, boolean MOT) {
  if (MOT == motTOP) {
    for (int i = 0; i < numSteps; i++) {
      takeStep(motTOP);
    }

  } else {
    for (int i = 0; i < numSteps; i++) {
      takeStep(motTOP);
    }
  }
}

//sets the direction of the specified motor
void setDir(boolean MOT, boolean dir) {
  if (MOT == motTOP) {

    switch (dir) {
      case UP:
        digitalWrite(CWCCWTOP, LOW);
        up = true;
        break;
      case DOWN:
        digitalWrite(CWCCWTOP, HIGH);
        up = false;
        break;
      default:
        //error
        Serial.println("Error setting direction, invalid direction");
        break;
    }

  } else {
    switch (dir) {
      case CW:
        digitalWrite(CWCCWBOT, LOW);
        break;
      case CCW:
        digitalWrite(CWCCWBOT, HIGH);
        break;
      default:
        //error
        Serial.println("Error setting direction, invalid direction");
        break;
    }
  }
}

//sets the speed of the stepper motor
void setStepSpeed(boolean MOT, int newSpeed) {
  if (MOT = motTOP) {
    speedTOP = newSpeed;
  } else {
    speedBOT = newSpeed;
  }
}

//---------------------motTOP functions---------------------//
//calculates the speed of the motor depending on where the syringe is located. This is required for the higher values
//as a larger torque is needed due to friction.
int calcSpeed() {
  if (pos < uLToSteps(200)) {
    speedTOP = 6;
  } else {
    speedTOP = 10;
  }
}

///THIS DOES NOT WORK YET
void setFlowRate(float flowRate) {
  speedTOP = flowRate;
}

//increments or decrements the position of the top motor with respect to Home (pos = 0)
void movPos() {
  if (up) {
    pos++;
  } else if (!up) {
    pos--;
  }
}

//moves the top motor to Home (pos = 0)
void goToZero() {
  setDir(motTOP, DOWN);

  while (pos > 0) {
    takeStep(motTOP);
  }

  //safety steps to ensure fully zeroed, these will not damage the motor as its magnetically controlled
  for (int i = 0; i < 5; i++) {
    takeStep(motTOP);
  }
  resetZero();
}

//sets the current position to Home (pos = 0)
void resetZero() {
  pos = 0;
}

//takes the current position and sets that to the range of steps
void resetRange() {
  range = pos;
  uLPerStep = (float) totalVol / (float) range;
  stepPeruL = (float) range / (float) totalVol;
}

//turns a specified number of uL into the corresponding number of steps
int uLToSteps(float uL) {
  return  (int) (uL * stepPeruL);
}

//moves a specific volume
void moveuL(float uL) {
  int numSteps = uLToSteps(uL);

  takeSteps(numSteps, motTOP);
}

//traverses from the current position to a specified volume
void goToVol(float uL) {

  if (uL > 250 || uL < 0) {
    Serial.println("Error, volume not valid");
    return;
  }

  float currentuL = pos * uLPerStep;
  float difference = uL - currentuL;

  if (difference > 0) {
    setDir(motTOP, UP);
  } else if (difference < 0) {
    setDir(motTOP, DOWN);
    difference = 0 - difference;
  }

  moveuL(difference);
}

//changes the correction factor for the syringe
void changeCorrectionFactor(float cF) {
  correctionFactor = cF;
}

//---------------------motBOT functions---------------------//
//NOTE: because the program takes 10 steps for safety, the values 20 (small hole) and 50 (big hole)
//      will become closer to 10 and 40. Thus, to determine if on the big hole a value of 35 steps is safe.


//transitions to the next state
int nextState() {
  setDir(motBOT, CCW);
  int holeSteps = 0;

  //take the first few steps to get into an appropriate area
  for (int i = 0; i < 10; i++) {
    takeStep(motBOT);
  }


  //this while loop moves across the current hole and counts its steps
  while (analogRead(encoderBOT) > 500) {
    takeStep(motBOT);
    holeSteps++;
  }
  //this while loop moves across the dead zone to the next hole
  while (analogRead(encoderBOT) < 500) {
    takeStep(motBOT);
  }

  //change the state variable to the correct state
  state++;
  if (state > 3) {
    state = 0;
  }

  return holeSteps;
}

//transitions to the previous state
int prevState() {
  setDir(motBOT, CW);
  int holeSteps = 0;

  //take the first few steps to get into an appropriate area
  for (int i = 0; i < 10; i++) {
    takeStep(motBOT);
  }
  //this while loop moves the sensor off of the hole
  while (analogRead(encoderBOT) > 500) {
    takeStep(motBOT);
  }
  //this while loop moves the sensor onto the next hole (off of the dead zones)
  while (analogRead(encoderBOT) < 500) {
    takeStep(motBOT);
  }
  //this while loop moves across the desired hole and counts the number of steps
  while (analogRead(encoderBOT) > 500) {
    takeStep(motBOT);
    holeSteps++;
  }

  //change the state variable to the correct state
  state--;
  if (state < 0) {
    state = 3;
  }

  return holeSteps;
}

//finds the home position (state 0)
void findHome() {
  int holeSteps;

  //Note that for the smaller gaps holeSteps returns ~20, for the large hole it returns ~50 consistantly
  while (holeSteps < 40) {
    holeSteps = prevState();
  }
  Serial.println(holeSteps);

  state = 0;
}

//transitions to the selected state
void toState(int target) {
  if (target < 0 || target > 3) {
    Serial.println("Error, invalid state");
    return;
  }

  //corner cases and normal cases of finding the closest path to the desired state
  if (state == target) return;

  if (state == 0 && target == 3) {
    prevState();
  } else if (state == 3 && target == 0) {
    nextState();
  } else if (state + 1 == target) {
    nextState();
  } else if (state - 1 == target) {
    prevState();
  } else {
    while (state != target) {
      nextState();
    }
  }
}


