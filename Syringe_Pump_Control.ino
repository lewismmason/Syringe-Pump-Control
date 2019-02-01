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
int totalVol = 2500;     //total volume of the syringe in microLitres
int speedTOP = 10;      //delay in ms between each step

float uLPerStep;      //ratio of microLitres per step on the syringe
float stepPeruL;      //ratio of steps per microLitre on the syringe
boolean up;           //true if the current direction is up
boolean halfStepping = false;
int pos = 0;

//This value is used to correct the volume of the syringe.
//The reasoning for this came from repeated tests that resulted in a volume too large.
//Ex: 4mL gave 4.2mL. so (4-4.2)/4 is the correction factor. Negative means produce less, positive means
//produce more. If the value is zero then there is no correction.
float correctionFactor = 0; //-0.05

//variables for motBot
int state = 0;        //provides what state the three way valve is in
int speedBOT = 4;     //delay in ms between each step (needs to be around 10 due to torsional friction)

//variables for both
#define motTOP true
#define motBOT false

//directional constants for calling functions. Note that CW corresponds to up and CCW corresponds to down on motTOP
#define UP true
#define DOWN false
#define CW true
#define CCW false

//control constants, corresponding to arduino pins that control each of the L297's inputs
#define RESETBOT 2    //sets the top motor to L297 datasheet state ABCD = 0101, rising edge
#define CLKBOT 13     //clk signal, rising edge
#define CWCCWBOT 11   //sets the direction, true = CW, false = CCW
#define ENABLEBOT 3   //enables motor rotation
#define RESETTOP 2
#define CLKTOP 12
#define CWCCWTOP 11
#define ENABLETOP 3

//analog inputs from the photoresistors (encoders)
int encoderBOT = A1;

//pinouts defined for the LCD display's operation
LiquidCrystal lcd(8, 9, 4, 5, 6, 7);

int lcd_key     = 0;
int adc_key_in  = 0;
int screenPos = 0;              //home is defined as 0. Each screenPos has both an even and odd screen (first and second)
int cursorPos = 0;
boolean recentChange = false;   //this is used to determine when to print new screens to the LCD display
boolean evenScreen = true;      //used to determine what polarity of screen the user is on
boolean returnFlag = false;
float useruL = 0;               //the user entered volume for manual operations
float userRate = 0;             //the user entered flow rate for manual operations
int runningCounter = 0;         //used to step the top motor at specific speeds
float stepDelay = 0;
int totalSteps = 0;
int menuDelay = 150;            //used to determine how long between each button input

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
int userState = 0;

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
  lcd.begin(16, 2);              // start LCD library
  lcd.createChar(0, rightArrow);
  lcd.createChar(1, leftArrow);
  recentChange = true;

  //setting up the syringe
  findHome();


  //the code used for interupt setup is found here: //https://www.instructables.com/id/Arduino-Timer-Interrupts/
  cli();  //stop interupts

  //set timer 2 interrupt at 5kHz
  TCCR2A = 0; //set the entire TCCR2A register to 0
  TCCR2B = 0; //same for TCCR2B
  TCNT2 = 0;  //initialize counter value to 0
  //set compare match register for 10kHz increments
  OCR2A = 49; // = (16*10^6)/(10000*64) - 1 (must be < 256)
  //turn on CTC mode
  TCCR2A |= (1 << WGM21);
  // set CS22 bit for 64 prescaler
  TCCR2B |= (1 << CS22);

  sei(); //allow interupts
} //end setup


ISR(TIMER2_COMPA_vect) {  //timer2 interrupt 5KHz determines when step the top motor
  runningCounter++; //represents x number of 0.2ms increments (if runningCounter = 5, 1ms has passed)
  if ((float) runningCounter / (float) 5 >= stepDelay ) {
    runningCounter = 0;
    digitalWrite(CLKTOP, LOW);
    digitalWrite(CLKTOP, HIGH);
    totalSteps++;
    movPos();
  }
}

//********************************************MAIN LOOP**********************************************//
//the main menu screen
void loop() {

  //this is used to update the LCD display when the page has been scrolled or if the cursor position has moved
  if (recentChange) {
    printScreen(0, "    Operate     ", 0, "   Calibrate    ");
    recentChange = false;

    switch (cursorPos) {
      case 0:
        printArrows(0, 3, 11);
        removeArrows(1, 2, 12);
        break;
      case 1:
        removeArrows(0, 3, 11);
        printArrows(1, 2, 12);
        break;
      default:
        break;
    }
    delay(menuDelay);
  }

  lcd_key = read_LCD_buttons(); //figure out what button was just pressed

  //determine what to do when a button is pressed
  switch (lcd_key) {
    case btnRIGHT:
      moveCursor(inc);
      break;
    case btnLEFT:
      moveCursor(dec);
      break;
    case btnUP:
      moveCursor(dec);
      break;
    case btnDOWN:
      moveCursor(inc);
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
  cursorBounds(-1, 2);
}
//********************************************MAIN LOOP END SIDE LOOPS BEGIN*******************************//

//This loop controls the "operate" selection on the menu
void operate() {
  shiftScreen(RIGHT, false);
  while (true) {


    if (recentChange) {
      screenBounds(-1, 2);
      switch (screenPos) {

        case 0: printFullScreen("<     Back     >", "  Press Select  ", "<  Operation   >", "      Menu       ");
          break;

        case 1: printFullScreen("<  Automatic   >", "     Manual     ", "                ", "                ");
          cursorBounds(-1, 2);
          switch (cursorPos) {
            case 0:
              printArrows(0, 2, 12);
              removeArrows(1, 4, 11);
              break;
            case 1:
              removeArrows(0, 2, 12);
              printArrows(1, 4, 11);
              break;
            default:
              break;
          }
          break;

        default:
          break;
      }

      recentChange = false;
      delay(menuDelay);
    }

    lcd_key = read_LCD_buttons(); //figure out what button was just pressed

    switch (lcd_key) {
      case btnRIGHT:
        shiftScreen(RIGHT, true);
        break;

      case btnLEFT:
        shiftScreen(LEFT, true);
        break;

      case btnUP:
        moveCursor(dec);
        break;
      case btnDOWN:
        moveCursor(inc);
        break;

      case btnSELECT:
        switch (screenPos) {
          case 0:
            if (evenScreen) {
              //exit operation menu
              resetLCDVars();
              return;
            } else {
              //title screen do nothing here
            }
            break;
          case 1:
            if (evenScreen) {
              switch (cursorPos) {
                case 0:
                  resetLCDVars();
                  automaticMoveSyringe();
                  break;
                case 1:
                  resetLCDVars();
                  manualMoveSyringe();
                  break;
              }
            } else {
              //temp do nothing
            }
            break;
        }
        break;

      default:
        break;
    }
  }
}

//calibrate menu pages
void calibrate() {
  shiftScreen(RIGHT, false);
  while (true) {

    if (recentChange) {
      screenBounds(-1, 2);
      switch (screenPos) {
        case 0: printFullScreen("<     Back     >", "  Press Select  ", "< Calibration  >", "      Menu       ");
          break;

        case 1:
          printLine(16, 1, BLANKSCREEN);
          printFullScreen("<   Set Vol    >", "    Set Zero    ", "<  Approx Vol  >", "   Set Range   ");
          cursorBounds(-1, 2);
          if (evenScreen) {
            switch (cursorPos) {
              case 0:
                printArrows(0, 3, 11);
                removeArrows(1, 3, 12);
                break;
              case 1:
                removeArrows(0, 3, 11);
                printArrows(1, 3, 12);
                break;
              default:
                break;
            }
          } else {
            switch (cursorPos) {
              case 0:
                printArrows(0, 2 + 16, 13 + 16);
                removeArrows(1, 2 + 16, 13 + 15);
                break;
              case 1:
                removeArrows(0, 2 + 16, 13 + 16);
                printArrows(1, 2 + 16, 13 + 15);
                break;
              default:
                break;
            }
          }
          break;
      }
      recentChange = false;
      delay(menuDelay);
    }

    //read the next key pressed
    lcd_key = read_LCD_buttons(); //figure out what button was just pressed

    switch (lcd_key) {
      case btnRIGHT:
        shiftScreen(RIGHT, true);
        break;

      case btnLEFT:
        shiftScreen(LEFT, true);
        break;

      case btnUP:
        moveCursor(dec);
        break;

      case btnDOWN:
        moveCursor(inc);
        break;

      case btnSELECT:
        switch (screenPos) {
          case 0:
            if (evenScreen) {
              //exit calibration menu
              resetLCDVars();
              return;
            } else {
              //title screen
            }
            break;

          case 1:
            if (evenScreen) {
              switch (cursorPos) {
                case 0:
                  recentChange = true;
                  setTotalVol();
                  break;
                case 1:

                  //continuously move down until the user presses select again
                  setDir(motTOP, DOWN);
                  recentChange = false;
                  speedTOP = 15;
                  printLine(0,0,"  Press Select  ");
                  printLine(0,1,"  To Set Zero   ");
                  delay(menuDelay);
                  while (recentChange != true) {
                    lcd_key = read_LCD_buttons(); //figure out what button was just pressed
                    switch (lcd_key) {
                      case btnSELECT:
                        recentChange = true;
                        break;
                      default:
                        break;
                    }
                    takeStep(motTOP);
                  }

                  pos = 0;
                  printLine(0, 1, "    Success     ");
                  recentChange = true;
                  delay(400);
                  break;
              }
            } else {
              switch (cursorPos) {
                case 0:
                  printLine(0, 1, "     Moving      ");
                  speedTOP = 15;
                  setDir(motTOP, UP);
                  if (halfStepping) {
                    takeSteps(2000, motTOP);
                  } else {
                    takeSteps(1000, motTOP);
                  }
                  break;
                case 1:
                  resetRange();
                  printLine(16, 0, "    Success     ");
                  printLine(16, 1, BLANKSCREEN);
                  printLine(16, 1, "New Range:" + (String) range);
                  delay(700);
                  recentChange = true;
                  break;
                default:
                  break;
              }
            }
            break;
        }
        break;

      default:
        break;
    }
  }
}

void setTotalVol() {
  printLine(0, 0, " Setting Volume  ");
  printLine(0, 1, BLANKSCREEN);
  lcd.blink();
  lcd.cursor();

  while (true) {
    if (recentChange) {
      cursorBounds(-1, 4);

      //volume bounds
      if (totalVol >= 10000) {
        totalVol = 0;
      } else if (totalVol <= -1) {
        totalVol = 9999;
      }

      printFloat(5, 1, totalVol, ' ');
      lcd.setCursor(9, 1);
      lcd.print("uL");

      //display the cursor
      switch (cursorPos) {
        case 0:
          lcd.setCursor(5, 1);
          break;
        case 1:
          lcd.setCursor(6, 1);
          break;
        case 2:
          lcd.setCursor(7, 1);
          break;
        case 3:
          lcd.setCursor(8, 1);
          break;
      }

      delay(menuDelay);
    }

    lcd_key = read_LCD_buttons(); //figure out what button was just pressed

    switch (lcd_key) {
      case btnRIGHT:
        moveCursor(inc);
        recentChange = true;
        break;

      case btnLEFT:
        moveCursor(dec);
        recentChange = true;
        break;

      case btnUP:
        recentChange = true;
        switch (cursorPos) {
          case 0:
            totalVol += 1000;
            break;
          case 1:
            totalVol += 100;
            break;
          case 2:
            totalVol += 10;
            break;
          case 3:
            totalVol += 1;
            break;
          default:
            break;
        }
        break;

      case btnDOWN:
        recentChange = true;
        switch (cursorPos) {
          case 0:
            totalVol -= 1000;
            break;
          case 1:
            totalVol -= 100;
            break;
          case 2:
            totalVol -= 10;
            break;
          case 3:
            totalVol -= 1;
            break;
          default:
            break;
        }
        break;


      case btnSELECT:
        lcd.noBlink();
        lcd.noCursor();
        range = range + range * correctionFactor;
        uLPerStep = (float) totalVol / (float) range;
        stepPeruL = (float) range / (float) totalVol;
        recentChange = true;
        return;
      default:
        break;
    }
  }
}

void manualMoveSyringe() {
  while (true) {

    if (recentChange) {
      screenBounds(-1, 2);
      userStateBounds(-1, 4);

      switch (screenPos) {
        case 0: printFullScreen("<     Back     >", "  Press Select  ", "<  Valve (" + (String)state + ")   >", "                ");
          if (evenScreen == false) {
            switch (userState) {
              case 0: printLine(16, 1, "0  NE            ");
                break;
              case 1: printLine(16, 1, "1     NW         ");
                break;
              case 2: printLine(16, 1, "2        SW      ");
                break;
              case 3: printLine(16, 1, "3           SE   ");
                break;
            }
          }
          break;

        case 1:
          printFullScreen("< Single Step  >", "      Mode      ", "<  Multi Step  >", "      Mode        ");
          break;
      }

      recentChange = false;
      delay(menuDelay);
    }

    //read the next key pressed
    lcd_key = read_LCD_buttons(); //figure out what button was just pressed

    switch (lcd_key) {
      case btnRIGHT:
        shiftScreen(RIGHT, true);
        break;

      case btnLEFT:
        shiftScreen(LEFT, true);
        break;

      case btnUP:
        moveCursor(dec);
        if (screenPos == 0 && evenScreen == false) {
          userState++;
        }
        break;

      case btnDOWN:
        moveCursor(inc);
        if (screenPos == 0 && evenScreen == false) {
          userState--;
        }
        break;

      case btnSELECT:
        switch (screenPos) {
          case 0:
            if (evenScreen == true) {
              resetLCDVars();
              return;
            } else {
              printLine(16, 1, "    Moving      ");
              toState(userState);
              recentChange = true;
            }
            break;
          case 1:
            if (evenScreen == true) {
              resetLCDVars();
              singleStepMode();
            } else {
              resetLCDVars();
              multiStepMode();
            }
            break;
          default: break;
        }
        break;
    }
  }
}

void singleStepMode() {
  while (true) {

    if (recentChange) {
      printScreen(0, (String)stepsTouL(pos) + "/" + (String)totalVol + "uL", 0, (String)pos + "/" + (String)range + "steps");
      recentChange = false;
      delay(menuDelay);
    }

    lcd_key = read_LCD_buttons(); //figure out what button was just pressed

    switch (lcd_key) {
      case btnUP:
        setDir(motTOP, UP);
        takeStep(motTOP);
        recentChange = true;
        break;
      case btnDOWN:
        setDir(motTOP, DOWN);
        takeStep(motTOP);
        recentChange = true;
        break;
      case btnSELECT:
        resetLCDVars();
        screenPos = 1;
        return;

      default:
        break;
    }
  }
}

void multiStepMode() {
  while (true) {
    if (recentChange) {
      cursorBounds(-1, 11);  //put the cursor in the correct bounds
      printScreen(0, "uL:", 0, "Rate:");
      lcd.setCursor(11, 1);
      lcd.write("uL/s");
      printFloat(3, 0, useruL, ' ');
      printFloat(5, 1, userRate, 'u');

      //some screen management
      lcd.setCursor(11, 0);
      lcd.write(' ');
      lcd.setCursor(12, 0);
      lcd.print("Move");

      //make the cursor visible
      lcd.cursor();//show the cursor
      lcd.blink(); //blink the cursor

      //print the cursor
      switch (cursorPos) {
        case 0:
          lcd.setCursor(3, 0);
          break;
        case 1:
          lcd.setCursor(4, 0);
          break;
        case 2:
          lcd.setCursor(5, 0);
          break;
        case 3:
          lcd.setCursor(6, 0);
          break;
        case 4:
          lcd.setCursor(8, 0);
          break;
        case 5:
          lcd.setCursor(5, 1);
          break;
        case 6:
          lcd.setCursor(6, 1);
          break;
        case 7:
          lcd.setCursor(7, 1);
          break;
        case 8:
          lcd.setCursor(8, 1);
          break;
        case 9:
          lcd.setCursor(10, 1);
          break;
        case 10:
          lcd.noCursor();
          lcd.noBlink();
          printArrow(0, 11, RIGHT);
          break;
        default:
          break;
      }

      recentChange = false;
      delay(menuDelay);
    }

    lcd_key = read_LCD_buttons(); //figure out what button was just pressed

    switch (lcd_key) {

      case btnUP:
        recentChange = true;
        switch (cursorPos) {
          case 0:
            useruL += 1000;
            break;
          case 1:
            useruL += 100;
            break;
          case 2:
            useruL += 10;
            break;
          case 3:
            useruL += 1;
            break;
          case 4:
            useruL += 0.1;
            break;
          case 5:
            userRate += 1000;
            break;
          case 6:
            userRate += 100;
            break;
          case 7:
            userRate += 10;
            break;
          case 8:
            userRate += 1;
            break;
          case 9:
            userRate += 0.1;
            break;
          case 10:
            lcd.noCursor();
            lcd.noBlink();
            setDir(motTOP, UP);
            interactiveMoveuL(useruL, userRate);
            lcd.cursor();
            lcd.blink();
            break;
          default:
            break;
        }

        uLBounds(-1, totalVol + 1);
        break;

      case btnDOWN:
        recentChange = true;
        switch (cursorPos) {
          case 0:
            useruL -= 1000;
            break;
          case 1:
            useruL -= 100;
            break;
          case 2:
            useruL -= 10;
            break;
          case 3:
            useruL -= 1;
            break;
          case 4:
            useruL -= 0.1;
            break;
          case 5:
            userRate -= 1000;
            break;
          case 6:
            userRate -= 100;
            break;
          case 7:
            userRate -= 10;
            break;
          case 8:
            userRate -= 1;
            break;
          case 9:
            userRate -= 0.1;
            break;
          case 10:
            lcd.noCursor();
            lcd.noBlink();
            setDir(motTOP, DOWN);
            interactiveMoveuL(useruL, userRate);
            lcd.cursor();
            lcd.blink();
            break;
          default:
            break;
        }

        uLBounds(-1, totalVol + 1);
        break;

      case btnLEFT:
        moveCursor(dec);
        break;

      case btnRIGHT:
        moveCursor(inc);
        break;

      case btnSELECT:
        //returns to the correct page on the menu
        recentChange = true;
        cursorPos = 0;
        delay(250);
        screenPos = 1;
        evenScreen = false;
        scrollNum(16, 0, RIGHT);
        lcd.noCursor();
        lcd.noBlink();
        return;

      default:
        break;
    }
  }
}


void automaticMoveSyringe() {
  while (true) {
    break;
  }
}


//moves a specific volume and displays key information on the screen
void interactiveMoveuL(float uL, float flowRate) {

  //determine the number of steps required
  int numSteps = uLToSteps(uL);
  stepDelay = flowRateToDelay(flowRate);
  totalSteps = 0;
  int minsLeft;
  int secsLeft;

  //print lcd functionality
  lcd.clear();
  printFullScreen("      uL/s MM:SS", "              ", "      /" + (String) totalVol + "uL", "       /     uL");
  scrollNum(16, 0, RIGHT);
  evenScreen = false;

  enableTimer2ISR();

  //print LCD functionality while the timer 2 interupt moves the motor
  while (totalSteps < numSteps) {
    //reading user inputs
    lcd_key = read_LCD_buttons();

    switch (lcd_key) {
      case btnNONE:
        break;
      case btnRIGHT:
        shiftScreen(RIGHT, false);
        delay(menuDelay);
        break;
      case btnLEFT:
        shiftScreen(LEFT, false);
        delay(menuDelay);
        break;
      case btnSELECT:
        //print E-stop message
        disableTimer2ISR();
        printFullScreen("   Emergency    ", "      Stop       ", "   Emergency    ", "      Stop       ");
        delay(1000);
        holdingScreen();
        resetLCDVars();
        evenScreen = false;
        return;
        break;
      default:
        break;
    }

    //Updating LCD display with valid information

    //time tracking
    minsLeft =  (int)( (useruL - stepsTouL(totalSteps)) / userRate ) /  60 ;
    secsLeft =  (int)( (useruL - stepsTouL(totalSteps)) / userRate ) %  60 ;
    printInt(11, 0, minsLeft);
    printInt(14, 0, secsLeft);

    //updating LCD with desired info
    printFloat(0, 0, flowRate, 'u');
    printFloat(16, 0, stepsTouL(pos), '/');
    printFloat(16, 1, stepsTouL(totalSteps), '/');
    printFloat(23, 1, useruL, 'u');
  }

  disableTimer2ISR();

  //print that it was successful
  printFullScreen("    Process      ", "    Finished     ", "    Process      ", "    Finished     ");
  holdingScreen();
  resetLCDVars();
  evenScreen = false;
  return;
}
//------------------------//
//     LCD functions      //
//------------------------//

void enableTimer2ISR() {
  //enable timer compare interrupt
  TIMSK2 |= (1 << OCIE2A);
}

void disableTimer2ISR() {
  //disable timer compare interrupt
  TIMSK2 &= (0 << OCIE2A);
}

//Prints a two digit integer value on the screen
void printInt(int start, int row, int num) {
  if (num < 10) {
    lcd.setCursor(start, row);
    lcd.write('0');
    lcd.setCursor(start + 1, row);
  } else {
    lcd.setCursor(start, row);
  }
  lcd.print((String) num);
}

//stops all user inputs until select is pressed
void holdingScreen() {
  recentChange = false;
  while (true) {
    if (recentChange) {
      break;
    }
    lcd_key = read_LCD_buttons();
    switch (lcd_key) {
      case btnSELECT:
        recentChange = true;
        break;
      default:
        break;
    }
  }
}

float flowRateToDelay(float flowRate) {
  //(1000msec/sec) * uL/Step *sec/uL = msec/step
  return 1000 * uLPerStep / flowRate;
}

//prints a float value in the following format: xxxx.yF where xxxx.y is the float accurate to one decimal, and F is the filler to remove the second decimal (space or units)
void printFloat(int start, int row, float val, char filler) {
  if (val < 10) {
    lcd.setCursor(start, row);
    lcd.write('0');
    lcd.setCursor(start + 1, row);
    lcd.write('0');
    lcd.setCursor(start + 2, row);
    lcd.write('0');
    lcd.setCursor(start + 3, row);
  } else if (val < 100) {
    lcd.setCursor(start, row);
    lcd.write('0');
    lcd.setCursor(start + 1, row);
    lcd.write('0');
    lcd.setCursor(start + 2, row);
  } else if (val < 1000) {
    lcd.setCursor(start, row);
    lcd.write('0');
    lcd.setCursor(start + 1, row);
  } else {
    lcd.setCursor(start, row);
  }
  lcd.print((String) val);
  lcd.setCursor(start + 6, row);
  lcd.write(filler);
}

void uLBounds(int low, int high) {
  if (useruL <= low) {
    useruL = -1 * useruL;
  }
  if (useruL >= high) {
    useruL -= totalVol;
  }
}

void printArrow(int row, int col, boolean direction) {
  lcd.setCursor(col, row);
  if (direction) {
    lcd.write(byte(0));
  } else {
    lcd.write(byte(1));
  }
}

void printArrows(int row, int x0, int x1) {
  lcd.setCursor(x0, row);
  lcd.write(byte(0));
  lcd.setCursor(x1, row);
  lcd.write(byte(1));
}

void removeArrows(int row, int x0, int x1) {
  lcd.setCursor(x0, row);
  lcd.write(' ');
  lcd.setCursor(x1, row);
  lcd.write(' ');
}


void cursorBounds(int low, int high) {
  if (cursorPos <= low ) {
    cursorPos = high - 1;
  } else if (cursorPos >= high) {
    cursorPos = 0;
  }
}


void userStateBounds(int low, int high) {
  if (userState <= low) {
    userState = high - 1;
  } else if (userState >= high) {
    userState = 0;
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
  range = 2020 / 2;
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
    screenPos = big - 1;
    evenScreen = false;
  } else if (screenPos >= big) {
    screenPos = 0;
    evenScreen = true;
  }
}

//shifts the screen to the next page
void shiftScreen(boolean direction, boolean Delay) {
  int del = 0;
  if (Delay) {
    del = 20;
  } else {
    del = 0;
  }
  switch (direction) {
    case RIGHT:
      scrollNum(16, del, RIGHT);
      if (!evenScreen) {
        scrollNum(8, 0, RIGHT);
        screenPos++;
      }
      evenScreen = !evenScreen;
      break;

    case LEFT:
      scrollNum(16, del, LEFT);
      if (evenScreen) {
        scrollNum(8, 0, LEFT);
        screenPos--;
      }
      evenScreen = !evenScreen;
      break;

    default:
      break;
  }
  recentChange = true;
  cursorPos = 0;
}

//scrolls the text on the LCD
void scrollNum(int num, int scrollDelay, boolean direction) {
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
  lcd.setCursor(x0, 0);
  lcd.print(label0);
  lcd.setCursor(x1, 1);
  lcd.print(label1);
}

void printLine(int x, int y, String message) {
  lcd.setCursor(x, y);
  lcd.print(message);
}

//prints 2 pages (full screen);
void printFullScreen (String label0, String label1, String label2, String label3) {
  printScreen(0, label0, 0, label1);
  printScreen(16, label2, 16, label3);
}

//determines which button has been pressed on the LCD screen
int read_LCD_buttons() {
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

    //calcSpeed();
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

//increments or decrements the position of the top motor with respect to Home (pos = 0)
void movPos() {
  if (up) {
    pos++;
  } else if (!up) {
    pos--;
  }
  if (pos < 0) {
    pos = 0;
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

float stepsTouL(int steps) {
  return (float) (steps * uLPerStep);
}

//moves a specific volume
void moveuL(float uL) {
  int numSteps = uLToSteps(uL);

  takeSteps(numSteps, motTOP);
}

//traverses from the current position to a specified volume
void goToVol(float uL) {

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


