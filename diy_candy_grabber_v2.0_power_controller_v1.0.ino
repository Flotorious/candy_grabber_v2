/*

    DIY Candy Grapper Version 2.0
    Power Controller (Arduino Uno) Version 1.0 
    15 August 2022 Florian Güldenpfennig

    This code was designed to be run on an Arduino UNO to control
    a bigger machine - an Arduino Mega incl. stepper motors. 
    Underlying purpose: to save energy, heat, noise 

    Working principle:
    After a pause of 15 minutes the Arduino UNO will allow the user
    to power up the Arduino Mega incl. stepper motors by pushing a button.

    The user can then play a game for 60 seconds, that is, fetch as many items
    from the grapper plate as he or she can.

    After 60 seconds the Arduino Mega will be switched off again an cannot be switched
    on for the next 15 minutes.

    This code can also control the LED stripes, which actually belong to the Arduino Mega setup
    in the first line.

    Materials:

    - LCD Display for showing the two countdowns
    - 3 Relais for controlling the Arduino Mega incl. stepper motors and neopixels
    - RFID reader for setting different modes with 'admin RFID chips' (e.g. unlocking free game)
    - (RTC module for displaying and keeping the time --> disabled in this version)
*/


#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27, 16, 2);
#include <Adafruit_NeoPixel.h>
#ifdef __AVR__
#include <avr/power.h>
#endif
#include <MFRC522.h>

// RTC module for displaying time and time-keeping in future version
//#include <Wire.h>
//#include "RTClib.h"


// RFID reader
// SPI-Pins for RFID reader
#define SS_PIN 10
#define RST_PIN 9
MFRC522 rfid(SS_PIN, RST_PIN); // Instance of the class
MFRC522::MIFARE_Key key;
// Init array that will store new NUID
byte nuidPICC[4];

// Relais Pins
const int RelaisPinLED = 4;
const int RelaisPin12V = 5;
const int RelaisPin5V = 6;
//const int RelaisPin4 = 7;

// Button for starting game if free game is available
const int INTERRUPT_PIN = 2;
const int BUTTONLIGHT = 3;

// duration play time
const int gameLength = 68; // set to 68 seconds
// duration pause between games
const int gamePause = 900; // set to 900 seconds --> player has to wait 15 minutes before next free game

// Time stamps for timing the application
//RTC_DS1307 RTC;
unsigned long stamp = 0;
unsigned long lastInterrupt = 0;
unsigned long whenFreeGameStarted = 0;

// Controlling the Arduino Mega
boolean freeGameAvailable = false;
boolean MegaIsOn = false;

// one neopixel strip object controlls four physical strips in my setup --> 4 physical stripes are connected to pin 8
const int neoPin = 8;
const int numberOfPixels = 48;
Adafruit_NeoPixel strip1 = Adafruit_NeoPixel(numberOfPixels, neoPin, NEO_GRB + NEO_KHZ800);

// Helper variable to make sure LCD only gets updated when content has changed
String lastMessage = "";

void setup () {
  pinMode(RelaisPin12V, OUTPUT);
  pinMode(RelaisPin5V, OUTPUT);
  pinMode(RelaisPinLED, OUTPUT);
  digitalWrite(RelaisPin12V, HIGH); // LOW wenn ADK gepowert werden soll
  digitalWrite(RelaisPin5V, HIGH); // LOW wenn ADK gepowert werden soll
  digitalWrite(RelaisPinLED, HIGH);
  delay(1000);

  // prepare neopixel strip(s)
  strip1.begin();
  strip1.show(); // Initialize all pixels to 'off'
  colorWipe(strip1.Color(0, 0, 0),5);

  pinMode(BUTTONLIGHT, OUTPUT);
  digitalWrite(BUTTONLIGHT, LOW);


  Serial.begin(9600);

  /* Counterdown with RTC -- deactivated for this version
    Wire.begin();
    RTC.begin();
    DateTime now = RTC.now();
    // stamp = now.unixtime() + 15 * 60;
    stamp = now.unixtime() + 15;

    if (! RTC.isrunning()) {
    Serial.println("RTC is NOT running!");
    // following line sets the RTC to the date & time this sketch was compiled
    RTC.adjust(DateTime(__DATE__, __TIME__));
    } */

  // taking current time stamp and adding some time (gamePause) --> free game will be available then
  stamp =  millis() + gamePause* (long) 1000;

  // Serial.print("Stamp: "); Serial.println(stamp);

  // small trick to make sure that no free game is available on boot-up
  whenFreeGameStarted =  -(gameLength * (long) 1000);

  // start LCD display and show hello message
  lcd.init();
  lcd.backlight();
  print2LCD("Hello!");
  lastInterrupt = millis();

  SPI.begin(); // Init SPI bus
  rfid.PCD_Init(); // Init MFRC522

  delay(500);
  // attach interrupt to start-game-button
  pinMode(INTERRUPT_PIN, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(INTERRUPT_PIN), myISR, FALLING); // trigger when button pressed, but not when released.
}

void loop () {

  checkRFID();

  megaController();


  /*Serial.print("Countdown: ");
    Serial.println(stamp - now.unixtime());
    DateTime now = RTC.now();
    long tmp = stamp - now.unixtime();*/
  
  long haveToWait = (stamp/1000 - (long)millis()/1000);
  
  /*Serial.print("stamp: "); Serial.println(stamp);
  Serial.print("Have to wait: "); Serial.println(haveToWait);*/

  /* If the user has waited long enough for a new game
     && there is currently no game in process */
  if (haveToWait <= 0 && MegaIsOn == false) {
    freeGameAvailable = true;
    digitalWrite(BUTTONLIGHT, HIGH);
    print2LCD("PUSH GREEN BUT- TON TO PLAY !!!");
    /* Waiting time is not over yet */
  } else if (haveToWait > 0 ) {
    print2LCD("Next free game: " + String(haveToWait) + " seconds");
    /* Game is in progress */
  } else if (MegaIsOn == true) {
    long playTimeRemaining = gameLength - abs(millis() - whenFreeGameStarted) / (long)1000;
    if (playTimeRemaining > gameLength-9) {
      print2LCD("Calibrating ... Please Wait");
    } else {
      print2LCD("Remaining Time: " + String(playTimeRemaining) + " seconds");
    }
  }
  
  delay(100);


  /*DateTime now = RTC.now();
    Serial.print(now.year(), DEC);
    Serial.print('/');
    Serial.print(now.month(), DEC);
    Serial.print('/');
    Serial.print(now.day(), DEC);
    Serial.print(' ');
    Serial.print(now.hour(), DEC);
    Serial.print(':');
    Serial.print(now.minute(), DEC);
    Serial.print(':');
    Serial.print(now.second(), DEC);
    Serial.println();*/
}


/*          Helper functions         */
/* ********************************* */


/* switches Arduino Mega off if no more game-time is left */
void megaController() {
  /*Serial.print("whenFreeGameStarted: ");
  Serial.println(whenFreeGameStarted);
  Serial.print("millis(): ");
  Serial.println(millis());*/
  
 // If game-time still remaining
 if (millis() < whenFreeGameStarted +  (gameLength * (long)1000)) {
    Serial.println("Game in Progess / Arduino Mega is powered up");
  } else { 
    if (MegaIsOn) {
      MegaIsOn = false;
      Serial.println("Switching Arduino Mega off");
      freeGameAvailable = false;
      switchMegaOn(false);
      /*DateTime now = RTC.now();
        stamp = now.unixtime() + 15;*/
      stamp = millis() + gamePause * (long) 1000;
      delay(500);
      colorWipe(strip1.Color(0, 0, 0), 5);
    }
  }
}

/* interrupt routine to monitor push button (start game button) */
void myISR() {
  if (abs(millis() - lastInterrupt) < 1000) {
  } else {
    lastInterrupt = millis();

    if (freeGameAvailable == true) {
      freeGameAvailable = false;
      switchMegaOn(true);
      /*switchMegaOn(true);
      whenFreeGameStarted = millis();*/
    }
  }
}


/* switch Arduino Mega on or off -- parameter 'true' --> switch on */
void switchMegaOn(boolean switchOn_) {
  if (switchOn_) { 
    Serial.println("Switching on Arduino Mega");
    MegaIsOn = true;
    digitalWrite(BUTTONLIGHT, LOW); // Turn off illumination of Start Button, because game already started
    whenFreeGameStarted = millis();
    digitalWrite(RelaisPin12V, LOW); // LOW when Mega is to be powered
    digitalWrite(RelaisPin5V, LOW); // LOW when Mega is to be powered
    digitalWrite(RelaisPinLED, LOW);
  } else  { // switch off the device (Arduino Mega)
    Serial.println("Switching off Arduino Mega");
    MegaIsOn = false;
    digitalWrite(RelaisPin12V, HIGH); // HIGH LOW when Mega is to switched off
    digitalWrite(RelaisPin5V, HIGH); // HIGH LOW when Mega is to switched off
    digitalWrite(RelaisPinLED, HIGH);
    delay(100);
    colorWipe(strip1.Color(0, 0, 0), 5);
  }
}

/* helper function to print lettes on LCD display */
void print2LCD(String s) {
  if (s!=lastMessage) {
    // clear screen
    for (int i = 0; i < 16; i++) {
      lcd.setCursor(i, 0);
      lcd.print(" ");
      lcd.setCursor(i, 1);
      lcd.print(" ");
    }
  
    for (int i = 0; i < s.length(); i++) {
      if (i < 16) {
        lcd.setCursor(i, 0);
        lcd.print(s[i]);
      } else {
        lcd.setCursor(i - 16, 1);
        lcd.print(s[i]);
      }
    }
  }
  lastMessage = s;

}


/* RFID reader functions taken from the MFRC522 demo */
void checkRFID() {
  //Serial.println("Checking RFID");
  // RFID Reader
  // Reset the loop if no new card present on the sensor/reader. This saves the entire process when idle.
  if ( ! rfid.PICC_IsNewCardPresent())
    return;

  // Verify if the NUID has been readed
  if ( ! rfid.PICC_ReadCardSerial())
    return;

  //Serial.print(F("PICC type: "));
  MFRC522::PICC_Type piccType = rfid.PICC_GetType(rfid.uid.sak);
  //Serial.println(rfid.PICC_GetTypeName(piccType));

  // Check is the PICC of Classic MIFARE type
  if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI &&
      piccType != MFRC522::PICC_TYPE_MIFARE_1K &&
      piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
    Serial.println(F("Your tag is not of type MIFARE Classic."));
    return;
  }


  printDec(rfid.uid.uidByte, rfid.uid.size);

  rfid.PICC_HaltA();

  // Stop encryption on PCD
  rfid.PCD_StopCrypto1();
}

/**
   Helper routine to dump a byte array as hex values to Serial.
*/
void printHex(byte *buffer, byte bufferSize) {
  for (byte i = 0; i < bufferSize; i++) {
    Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], HEX);
  }
}

/**
   Helper routine to dump a byte array as dec values to Serial.
*/
void printDec(byte *buffer, byte bufferSize) {
  Serial.println("Printing RFID Tag");
  String RFIDstr = "";

  for (byte i = 0; i < bufferSize; i++) {
    //Serial.print(buffer[i] < 0x10 ? " 0" : " ");
    Serial.print(buffer[i], DEC);
    RFIDstr = RFIDstr + String(buffer[i]);
  }

  Serial.println();

  // if RFID tag equals particular RFID 
  
  // unlock free game cheat tag
  if (RFIDstr == "16910576194") {
    Serial.println("unlocking free game");
    stamp = millis();
    freeGameAvailable = true;
    digitalWrite(BUTTONLIGHT, HIGH);
    print2LCD("PUSH GREEN BUT- TON TO PLAY !!!");
  }
}

// neopixel helper function
void colorWipe(uint32_t c, uint8_t wait) {
  for (uint16_t i = 0; i < strip1.numPixels(); i++) {
    strip1.setPixelColor(i, c);
    strip1.show();
    delay(wait);
  }
}
