/*

    DIY Candy Grapper Version 2.0
    Stepper Controller (Arduino Mega) Version 1.0
    17 August 2022 Florian Güldenpfennig

    This code was designed to be run on an Arduino Mega to control
    the candy grapper's stepper motors for moving the claw

    Working principle:
    On boot-up the grapper's z-axis is calibrated and the user then
    is free to use the controls for moving the claw

    Materials:

    - stepper motors, servo-motor
    - end-stops
    - hall sensor
    - joystick, buttons
    - neopixel strips
    - (bluetooth module for using a smart phone as input device --> disabled in this version)
*/


#include <Arduino.h>
#include "BasicStepperDriver.h"
#include <Servo.h>
#include <Wire.h>
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
#include <avr/power.h>
#endif

Servo myservo;

// Motor steps per revolution. Most steppers are 200 steps or 1.8 degrees/step
#define MOTOR_STEPS 200
#define RPM 30
#define MICROSTEPS 128

#define DIRX 5
#define STEPX 2
#define DIRY 6
#define STEPY 3
#define DIRZ 7
#define STEPZ 4

#define SLEEP 8

// joystick/controls
#define buttonYPlus 47
#define buttonYMinus 46
#define buttonXPlus 49
#define buttonXMinus 44
#define buttonZPlus 27
#define buttonZMinus 25

// end stops
#define buttonZStop 28 // hall sensor
#define buttonX1StopMinus 34
#define buttonX1StopPlus 24
#define buttonX2StopMinus 32
#define buttonX2StopPlus 22
#define buttonYStopMinus 26
#define buttonYStopPlus 30

// controlling and moving the claw
#define buttonServo 23
#define servoSignal 45

boolean hitZStop = false;
boolean servoOpen = false;
long servoStamp;

int lastMotor = 3; // helper varialble to allow only one stepper at a time to be used (to limit current)
                   // 1-->x-motor   2-->y-motor   3-->z-motor
long lastMove = 0;

BasicStepperDriver stepperX(MOTOR_STEPS, DIRX, STEPX, SLEEP);
BasicStepperDriver stepperY(MOTOR_STEPS, DIRY, STEPY, SLEEP);
BasicStepperDriver stepperZ(MOTOR_STEPS, DIRZ, STEPZ, SLEEP);

// for stearing and calibrating the z-axis
const int chainLength = 121; // length of chain
int chainCounter = 0;

// bluetooth module for future versions
// source: https://funduino.de/tutorial-hc-05-und-hc-06-bluetooth
char blueToothVal; //Werte sollen per Bluetooth gesendet werden
char lastValue;   //speichert den letzten Status der LED (on/off)

// LED strips
Adafruit_NeoPixel strip1 = Adafruit_NeoPixel(48, 33, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip2 = Adafruit_NeoPixel(48, 35, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip3 = Adafruit_NeoPixel(48, 37, NEO_GRB + NEO_KHZ800);
Adafruit_NeoPixel strip4 = Adafruit_NeoPixel(48, 39, NEO_GRB + NEO_KHZ800);


void setup() {
  strip1.begin();
  strip1.show(); // Initialize all pixels to 'off'
  strip2.begin();
  strip2.show(); // Initialize all pixels to 'off'
  strip3.begin();
  strip3.show(); // Initialize all pixels to 'off'
  strip4.begin();
  strip4.show(); // Initialize all pixels to 'off'
  colorWipe(strip1.Color(255, 240, 0), 5, 1, true);
  colorWipe(strip2.Color(255, 240, 0), 5, 2, true);
  colorWipe(strip3.Color(255, 240, 0), 5, 3, true);
  colorWipe(strip4.Color(255, 240, 0), 5, 4, true);


  servoStamp = 0;
  myservo.attach(servoSignal);

  stepperX.begin(RPM, MICROSTEPS);
  stepperY.begin(RPM, MICROSTEPS);
  stepperZ.begin(RPM / 3, MICROSTEPS);

  // if using enable/disable on ENABLE pin (active LOW) instead of SLEEP uncomment next line
  stepperX.setEnableActiveState(LOW);
  stepperY.setEnableActiveState(LOW);
  stepperZ.setEnableActiveState(LOW);

  // energize coils - the motor will hold position
  stepperX.enable();
  stepperY.enable();
  stepperZ.enable();

  pinMode(buttonXPlus, INPUT);
  digitalWrite(buttonXPlus, HIGH);
  pinMode(buttonXMinus, INPUT);
  digitalWrite(buttonXMinus, HIGH);
  pinMode(buttonYPlus, INPUT);
  digitalWrite(buttonYPlus, HIGH);
  pinMode(buttonYMinus, INPUT);
  digitalWrite(buttonYMinus, HIGH);
  pinMode(buttonZPlus, INPUT);
  digitalWrite(buttonZPlus, HIGH);
  pinMode(buttonZMinus, INPUT);
  digitalWrite(buttonZMinus, HIGH);
  pinMode(buttonZStop, INPUT);
  digitalWrite(buttonZStop, HIGH);
  pinMode(buttonX1StopMinus, INPUT);
  digitalWrite(buttonX1StopMinus, HIGH);
  pinMode(buttonX1StopPlus, INPUT);
  digitalWrite(buttonX1StopPlus, HIGH);
  pinMode(buttonX2StopMinus, INPUT);
  digitalWrite(buttonX2StopMinus, HIGH);
  pinMode(buttonX2StopPlus, INPUT);
  digitalWrite(buttonX2StopPlus, HIGH);
  pinMode(buttonYStopMinus, INPUT);
  digitalWrite(buttonYStopMinus, HIGH);
  pinMode(buttonYStopPlus, INPUT);
  digitalWrite(buttonYStopPlus, HIGH);
  pinMode(buttonServo, INPUT_PULLUP);
  pinMode(45, INPUT_PULLUP);

  Serial.begin(9600);
  Serial3.begin(9600);

  // calibrate z-axis
  while ( digitalRead(buttonZStop) == LOW) {
    moveOneZ(false);
  }
  chainCounter = 122;
  delay(500);
  // 121 equals total length of chain/z-axis
  for (int i = 0; i <= 25; i++) {
    moveOneZ(true);
  }

  #if defined (__AVR_ATtiny85__)
  if (F_CPU == 16000000) clock_prescale_set(clock_div_1);
  #endif
  // End of trinket special code


  hitZStop = false;
  servoOpen = false;
  Serial.println("setup complete");
  
}

void loop() {


  // check all input buttons/endstops/joystick etc. for input signals one after the other
  if (digitalRead(buttonZStop) == HIGH) {
    //moveOneZ(true);
    hitZStop = true;
  }

  if (digitalRead(buttonServo) == LOW && abs(millis() - servoStamp) > 500) {
    Serial.println("claw");
    toggleServo();
    servoStamp = millis();
  }

  if (digitalRead(buttonXPlus) == LOW && digitalRead(buttonX1StopPlus) == HIGH && digitalRead(buttonX2StopPlus) == HIGH) {
    Serial.println("xPlus");
    moveOneX(true);
  }
  if (digitalRead(buttonXMinus) == LOW && digitalRead(buttonX1StopMinus) == HIGH && digitalRead(buttonX2StopMinus) == HIGH) {
    moveOneX(false);
    Serial.println("xMinus");
  }
  // only one end stop in each direction for the y-axis
  if (digitalRead(buttonYPlus) == LOW && digitalRead(buttonYStopPlus) == HIGH) {
    Serial.println("yPlus");
    moveOneY(true);
  }
  if (digitalRead(buttonYMinus) == LOW && digitalRead(buttonYStopMinus) == HIGH) {
    Serial.println("yMinus");
    moveOneY(false);
  }
  if (digitalRead(buttonZPlus) == LOW && chainCounter > 0) {
    //if (digitalRead(buttonZPlus) == LOW && digitalRead(buttonZStop) == LOW) {
    Serial.println("zPlus");
    hitZStop = false;
    moveOneZ(true);
  }
  if (digitalRead(buttonZMinus) == LOW && digitalRead(buttonZStop) == LOW && hitZStop == false && chainCounter <= 121) {
    Serial.println("zMinus");
    moveOneZ(false);
  }

  // bluetooth for connecting to smart phones etc.
  // for future versions
  if (Serial3.available() > 0) {
    blueToothVal = Serial3.read(); //..sollen diese ausgelesen werden
    Serial.println(blueToothVal);
  }

  if (blueToothVal == 'p' && digitalRead(buttonX1StopPlus) == HIGH && digitalRead(buttonX2StopPlus) == HIGH) {
    Serial.println("x increase");
    moveOneX(true);
  }
  if (blueToothVal == 'o' && digitalRead(buttonX1StopMinus) == HIGH && digitalRead(buttonX2StopMinus) == HIGH) {
    Serial.println("x decrease");
    moveOneX(false);
  }

  if (blueToothVal == 'q' && digitalRead(buttonYStopPlus && digitalRead(buttonYStopPlus) == HIGH)) {
    Serial.println("y increase");
    moveOneY(true);
  }
  if (blueToothVal == 'a' && digitalRead(buttonYStopMinus && digitalRead(buttonYStopMinus) == HIGH)) {
    Serial.println("y decrease");
    moveOneX(false);
  }

  if (blueToothVal == 'g' && abs(millis() - servoStamp) > 500) {
    toggleServo();
    servoStamp = millis();
  }
}

/*
    Helper functions
*/

// The boolean parameters of the move-functions are used to specify the direction, for example, 'true'--> move one unit to the left and 'false'--> move one unit to the right
void moveOneX(boolean dir) {
  if (lastMotor==1 || abs(millis()-lastMove)>500) {
    lastMotor = 1;
    lastMove = millis();
    //stepperX.enable();
    if (dir == true)
      stepperX.move(1 * MICROSTEPS);
    else
      stepperX.move(-1 * MICROSTEPS);
    //stepperX.disable();
  }
}



void moveOneY(boolean dir) {
  if (lastMotor==2 || abs(millis()-lastMove)>500) {
    lastMotor = 2;
    lastMove = millis();  
    //stepperY.enable();
    if (dir == true)
      stepperY.move(1 * MICROSTEPS);
    else
      stepperY.move(-1 * MICROSTEPS);
    //stepperY.disable();
  }
}

void moveOneZ(boolean dir) {
  //Serial.print("Z-axis: "); Serial.print("Last Motor: "); Serial.print(lastMotor); Serial.print(" lastMove: "); Serial.println(abs(millis()-lastMove));
  if (lastMotor==3 || abs(millis()-lastMove)>500) {
    lastMotor = 3;
    lastMove = millis();
        
    //stepperZ.enable();
    if (dir == true) {
      stepperZ.move(1 * MICROSTEPS);
      chainCounter = chainCounter - 1;
      if (chainCounter < 0)
        chainCounter = 0;
  
      // highlight position of claw on LED strips
      colorPixel(strip4.Color(210, 20, 7), strip1.Color(255, 240, 0), 1, map(chainCounter, 0, 121, 0, 38));
      colorPixel(strip4.Color(210, 20, 7), strip1.Color(255, 240, 0), 2, map(chainCounter, 0, 121, 0, 38));
      colorPixel(strip4.Color(210, 20, 7), strip1.Color(255, 240, 0), 3, map(chainCounter, 0, 121, 0, 38));
      colorPixel(strip4.Color(210, 20, 7), strip1.Color(255, 240, 0), 4, map(chainCounter, 0, 121, 0, 38));
    }
    else {
      stepperZ.move(-1 * MICROSTEPS);
      //stepperZ.disable();
      chainCounter = chainCounter + 1;
      if (chainCounter > chainLength)
        chainCounter = chainLength;
  
      colorPixel(strip4.Color(210, 20, 7), strip1.Color(255, 240, 0), 1, map(chainCounter, 0, 121, 0, 38));
      colorPixel(strip4.Color(210, 20, 7), strip1.Color(255, 240, 0), 2, map(chainCounter, 0, 121, 0, 38));
      colorPixel(strip4.Color(210, 20, 7), strip1.Color(255, 240, 0), 3, map(chainCounter, 0, 121, 0, 38));
      colorPixel(strip4.Color(210, 20, 7), strip1.Color(255, 240, 0), 4, map(chainCounter, 0, 121, 0, 38));
    }
  }
}

/*
   Open and close claw
*/

void toggleServo() {

  if (servoOpen == true) {
    myservo.attach(servoSignal);
    myservo.write(0);
    delay(500);
    myservo.detach();
    servoOpen = false;
    Serial.println("close");
    colorWipe(strip1.Color(255, 240, 0), 0, 2, true);
  } else {
    myservo.attach(servoSignal);
    myservo.write(10);
    myservo.write(20);
    myservo.write(30);
    myservo.write(40);
    delay(250);
    myservo.detach();
    servoOpen = true;
    Serial.println("open");
    colorWipe(strip1.Color(210, 20, 7), 0, 2, true);
  }
}


/*
   neopixel code taken from Adafruit and modified to make individual stripes addressable (id) and the direction (up/down) can be
   spezified
*/

// Fill the dots one after the other with a color
void colorWipe(uint32_t c, uint8_t wait, int id, boolean direction_) {
  if (direction_ == true) {
    for (uint16_t i = 0; i < strip1.numPixels(); i++) {
      if (id == 1) {
        strip1.setPixelColor(i, c);
        strip1.show();
      }
      if (id == 2) {
        strip2.setPixelColor(i, c);
        strip2.show();
      }
      if (id == 3) {
        strip3.setPixelColor(i, c);
        strip3.show();
      }
      if (id == 4) {
        strip4.setPixelColor(i, c);
        strip4.show();
      }
      if (id == 5) { //all
        strip1.setPixelColor(i, c);
        strip1.show();
        strip2.setPixelColor(i, c);
        strip2.show();
        strip3.setPixelColor(i, c);
        strip3.show();
        strip4.setPixelColor(i, c);
        strip4.show();
      }
      delay(wait);
    }
  }
  else {
    for (int i = strip1.numPixels(); i >= 0; i--) {
      if (id == 1) {
        strip1.setPixelColor(i, c);
        strip1.show();
      }
      if (id == 2) {
        strip2.setPixelColor(i, c);
        strip2.show();
      }
      if (id == 3) {
        strip3.setPixelColor(i, c);
        strip3.show();
      }
      if (id == 4) {
        strip4.setPixelColor(i, c);
        strip4.show();
      }
      if (id == 5) { //all
        strip1.setPixelColor(i, c);
        strip1.show();
        strip2.setPixelColor(i, c);
        strip2.show();
        strip3.setPixelColor(i, c);
        strip3.show();
        strip4.setPixelColor(i, c);
        strip4.show();
      }
      delay(wait);
    }
  }
}

// Fill the dots one after the other with a color
void colorPixel(uint32_t c, uint32_t gc, int id, int p) {
  if (id == 1) {
    strip1.setPixelColor(p + 1, gc);
    strip1.setPixelColor(p, c);
    strip1.setPixelColor(p - 1, gc);
    strip1.show();
  }
  if (id == 2) {
    strip2.setPixelColor(p + 1, gc);
    strip2.setPixelColor(p, c);
    strip2.setPixelColor(p - 1, gc);
    strip2.show();
  }
  if (id == 3) {
    strip3.setPixelColor(p + 1, gc);
    strip3.setPixelColor(p, c);
    strip3.setPixelColor(p - 1, gc);
    strip3.show();
  }
  if (id == 4) {
    strip4.setPixelColor(p + 1, gc);
    strip4.setPixelColor(p, c);
    strip4.setPixelColor(p - 1, gc);
    strip4.show();
  }
}
