#include <stdint.h>
#include <avr/interrupt.h>
#include <avr/io.h>

#include <ByteBuffer.h>
#include <EE24CCxxx.h>
#include <TMRpcm.h>
#include <Wire.h>

#include "configuration.h"

#define GATE_POSITION_DOWN  0
#define GATE_POSITION_UP  1

#define STARTED 1
#define OK_RIDERS 2
#define RIDERS_READY 3
#define RANDOM_PAUSE 4
#define RED_LIGHT 5
#define YELLOW1_LIGHT 6
#define YELLOW2_LIGHT 7
#define DROP_GATE 8
#define GREEN_LIGHT 9
#define GATE_RESET 10
 

/**
 * Serial input buffer
 */
byte serialInput[100];
int serialSize = 0;

long nextStart = 0; //testing for continous run
long nextTime = 0; //next time a cadence event is triggered
long startTime = 0; //the time when the gate droppered. used by timer
int cadenceStarted = false; //whether the cadence has started
int timerStarted = false; //whether the timer has started
int gatePosition = GATE_POSITION_DOWN ; //the current position of the gate
long lastButtonRead = 0;  //last time push button was read
int cadenceState = 0; //the current state of the cadence
int gateReleaseAdjustment = GATE_RELEASE_ADJUSTMENT; //the time to prerelease the gate

/**
 * Recalibrated each start, used as a threadhold to indicate timer was triped
 */
int MAT_SENSOR_THRESHOLD = 0;

/**
 * Audio PWM player
 */
TMRpcm trmpcm;

void setup() {
  // initialize serial communication for debug
  SerialPort.begin(9600);

  //Turn on i2c and set to 400Hz
  Wire.begin();
  TWBR = 12;

  //setup pwvm audio
  trmpcm.speakerPin = SPEAKER;
  trmpcm.setVolume(6);

  //set pin modes
  pinMode(GATE, OUTPUT);
  pinMode(LIGHT_RED, OUTPUT);
  pinMode(LIGHT_YELLOW_1, OUTPUT);
  pinMode(LIGHT_YELLOW_2, OUTPUT);
  pinMode(LIGHT_GREEN, OUTPUT);
  pinMode(START, INPUT_PULLUP);

  //disable hum of speaker before first cadence
  pinMode(SPEAKER, OUTPUT);
  digitalWrite(SPEAKER, LOW);

  //ensure ram is off
  digitalWrite(GATE, LOW);

  //performance light show to indicate box is on
  digitalWrite(LIGHT_RED, HIGH);
  delay(250);
  digitalWrite(LIGHT_YELLOW_1, HIGH);
  delay(250);
  digitalWrite(LIGHT_YELLOW_2, HIGH);
  delay(250);
  digitalWrite(LIGHT_GREEN, HIGH);
  delay(250);
  for (int i = 0;i<5;i++){
    digitalWrite(LIGHT_RED, HIGH);  
    digitalWrite(LIGHT_YELLOW_1, HIGH);
    digitalWrite(LIGHT_YELLOW_2, HIGH);
    digitalWrite(LIGHT_GREEN, HIGH);
    delay(100);
    digitalWrite(LIGHT_RED, LOW);
    digitalWrite(LIGHT_YELLOW_1, LOW);
    digitalWrite(LIGHT_YELLOW_2, LOW);
    digitalWrite(LIGHT_GREEN, LOW);
    delay(100);
  }

}

/**
 * Sends event via serial, aka blueooth
 *
 * Message Format: BMX99CMD99ARGS
 * BMX = start of message indicator
 * 99 = number of byte of command <- a single byte
 * CMD = the command sent
 * 99 = number of bytes of args <- a single byte 0x00 - 0xFF
 * ARGS = the args for this command
 * 
 */
void sendEvent(String cmd, String args){
  int cmdSize = cmd.length();
  int argSize = args.length();

  SerialPort.print("BMX");
  SerialPort.print((char)cmdSize);
  SerialPort.print(cmd);
  SerialPort.print((char)argSize);
  SerialPort.print(args);
}

/**
 * Returns the software version number
 */
void version() {
  sendEvent("SW_VERSION",FIRMWARE_VERSION);
}

int continousBurnIn(int pin){

  long currentTime = millis();
  if (currentTime > nextStart) {
    nextStart+=15000;
    return HIGH;
  }

  return LOW;
}
  
int checkButtonPress(int pin){
  long currentTime = millis();
  if ((currentTime - lastButtonRead) > DEBOUNCE_DELAY) {
    if (!digitalRead(pin)) {
      lastButtonRead = currentTime;
      return HIGH;
    }
  }

  return LOW;
}

/**
 * Main loop
 */
void loop() {

  //process any serial commands
  processSerialInput();

  int buttonPushed = checkButtonPress(START);
  
  /**
   * Take action on buttonPushed
   */
  if (buttonPushed) {

   if (cadenceStarted) {
      cancelCadence();
    } else if (gatePosition == GATE_POSITION_DOWN ) {
      raiseGate();
    } else if (gatePosition == GATE_POSITION_UP ) {
      startCadence();
    }
  }


  /**
   * If candence is running process events
   */
  if (cadenceStarted) {
    runCadence();
  }
  
//  /*
//   * Check Sensor Matt
//   */
//  if (timerStarted) {
//    int timerMat = analogRead(TIMER);
//    if ( timerMat > MAT_SENSOR_THRESHOLD ){
//      sendEvent("EVNT_TIMER_1", String(millis()-startTime));
//      timerStarted = false;
//    }
//  }


}

void runCadence() {

  long currentTime = millis();

  if (currentTime > nextTime) {
    
    if (cadenceState == STARTED) {

//      calibrateTimer();

      nextTime = currentTime + 50;

      cadenceState = OK_RIDERS;
    } else if (cadenceState == OK_RIDERS) {

      // "OK RIDERS RANDOM START" : 1:50+1:80 sec
      sendEvent("EVNT_OK_RIDERS","");
      trmpcm.play(0,13224);

      nextTime += 3300;
      
      cadenceState = RIDERS_READY;
    } else if (cadenceState == RIDERS_READY) {

      // "RIDERS READY - WATCH THE GATE" : 2.00 sec
      sendEvent("EVNT_RIDERS_READY","");
      trmpcm.play(32000,17535);

      nextTime += 2110;

      cadenceState = RANDOM_PAUSE;
    } else if (cadenceState == RANDOM_PAUSE) {
      trmpcm.disable();

      //Random delay .1 - 2.7 sec
      //offical delay of .1 was to fast, using .250
      nextTime += random(550, 2700);
      cadenceState = RED_LIGHT;
      
    } else if (cadenceState == RED_LIGHT) {
       //Red Light
      lightOn(LIGHT_RED, 60);
      sendEvent("EVNT_RED_LIGHT","");

      nextTime += 120;

      cadenceState = YELLOW1_LIGHT;
    } else if (cadenceState == YELLOW1_LIGHT) {
      //Yellow Light
      lightOn(LIGHT_YELLOW_1,60);
      sendEvent("EVNT_YELLOW_1_LIGHT","");

      nextTime += 120;

      cadenceState = YELLOW2_LIGHT;
    } else if (cadenceState == YELLOW2_LIGHT) {
      //Yellow Light
      lightOn(LIGHT_YELLOW_2,60);
      sendEvent("EVNT_YELLOW_2_LIGHT","");

      nextTime += (120 - gateReleaseAdjustment);
 
      cadenceState = DROP_GATE;
    } else if (cadenceState == DROP_GATE) {
      //drop gate
      digitalWrite(GATE, LOW);
      gatePosition = GATE_POSITION_DOWN ;

      nextTime += gateReleaseAdjustment;

      cadenceState = GREEN_LIGHT;
    } else if (cadenceState == GREEN_LIGHT) {
      //Green light
      lightOn(LIGHT_GREEN, 2250);
      startTime = millis();
      timerStarted = true;
      sendEvent("EVNT_GREEN_LIGHT","");

      nextTime += 3000;

      cadenceState = GATE_RESET;
    } else if (cadenceState == GATE_RESET ) {
      //3 seconds after gate drops reset all lights
      resetLights();
      cadenceStarted = false;
    }

  }  
}

//Turns off the lights and magnet
void resetLights() {
  digitalWrite(LIGHT_RED, LOW);
  digitalWrite(LIGHT_YELLOW_1, LOW);
  digitalWrite(LIGHT_YELLOW_2, LOW);
  digitalWrite(LIGHT_GREEN, LOW);
}

//Raise gate
void raiseGate() {
  for (int i = 0; i<5; i++){
    tone(SPEAKER, 1150, 250);
    delay(500);
  }

  digitalWrite(GATE, HIGH);
  gatePosition = GATE_POSITION_UP;
}

void lightOn(int light, int msec){
  digitalWrite(light, HIGH);
  tone(SPEAKER, 632, msec);
}

void startCadence() {
  cadenceStarted = true;
  cadenceState = STARTED;
  sendEvent("EVNT_CADENCE_STARTED","");
}

/**
 * For safety, the stop button can only be pressed up to the end of the second
 * voice cadence after to abort the sequence. 
 */
void cancelCadence() {
  if (cadenceState ==  STARTED ||
      cadenceState ==  OK_RIDERS ||
      cadenceState ==  RIDERS_READY ||
      cadenceState == RANDOM_PAUSE) {
    tone(SPEAKER, 740, 220);
    delay(225);
    noTone(SPEAKER);
    tone(SPEAKER, 680, 440);
    cadenceStarted = false;
  }
}

void shiftArray(byte data[], int offset, int length){

  for (int i = offset;i<length;i++){
    data[i-offset] = data[i];
  }

}

String toStr(byte data[], int offset, int length){
  int endOffset = offset+length;
  String value = "";
  value.reserve(length);

  for (int i = offset ; i<endOffset ; i++){
    value += (char)data[i];
  }

  return value;
}

void performCommand(String cmd, String args){
  if (cmd.equals("GET")){
    if (args.equals("SW_VERSION")) {
      version();
    }
  } 
  else if (cmd.equals("START_CADENCE")) {
    if (cadenceStarted == false){
      startCadence();
    }
  } 
  else if (cmd.equals("CALIBRATE")) {
//      gateCalibrate();
  } 
}


/**
 * Resets the serial buffer, clears out the input stream and 
 * resets previously read data
 **/
void resetSerial(){
  while (SerialPort.available()) {
    SerialPort.read(); 
  }
  serialSize = 0;
}  

void processSerialInput() {

  //read everyting we have recieved adn buffer
  while (SerialPort.available()) {
    serialInput[serialSize++] = SerialPort.read(); 
    if (serialSize>100){
      sendEvent("ERROR","BUFFER_LIMIT");
      resetSerial();
      return;
    }   
  }

  //not a valid cmd, yet
  if(serialSize<5)
    return;

  //look for cmd seq
  for (int i = 0; i < serialSize; i++){

    //found cmd seq
    if (serialInput[i] == 'B' && serialInput[i+1] == 'M' && serialInput[i+2] == 'X'){
      i += 3;

      //read cmd
      if (i >= serialSize) return;
      int cmdSize = 0 | serialInput[i++];

      if (i+cmdSize > serialSize) return;
      String cmd = toStr(serialInput, i, cmdSize);
      i += cmdSize;

      //read args
      if (i >= serialSize) return;
      int argSize = 0 | serialInput[i++];

      if (i+argSize > serialSize) return;
      String args = toStr(serialInput, i, argSize);
      i += argSize;

      performCommand(cmd, args);

      //reposition unread to start of buffer
      shiftArray(serialInput, i, serialSize);
      serialSize = serialSize-i;

      return;
    } 
  }

  //no valid command found, wiping out data
  serialSize = 0;
}

//void calibrateTimer(){
//  //Get base reading for timing matt
//  int reading = 0;
//  for (int i = 0; i<5; i++){
//    reading += analogRead(TIMER);
//    delay(20);
//  }
//  MAT_SENSOR_THRESHOLD = reading/5+300;
//
//}

//void gateCalibrate() {
//  long startTime = 0;
//  long splitTime = 0;
//  
//  //make a bunch of noise
//  for (int i = 0; i<5; i++){
//    tone(SPEAKER, 1150, 500);
//    delay(750);
//  }
//
//  //raise the gate
//  digitalWrite(GATE, HIGH);
//
//  //loop
//  for (int i = 0; i<3; i++){
//    //calibrateTimer();
//
//    //wait 5sec
//    delay(5000);
//    
//    //drop gate with warning
//    tone(SPEAKER, 1150, 500);
//    delay(2000);
//    startTime = millis();
//    digitalWrite(GATE, LOW);
//
////    //check time
////    int timerMat = analogRead(TIMER);
////    if ( timerMat > MAT_SENSOR_THRESHOLD ){
////      splitTime += millis() - startTime - 310;
////    }
//
//  }
//
//  byte gateDelay = splitTime/3;
//  if (gateDelay > 120)
//    gateDelay = 120;
//  if (gateDelay < 0 )
//    gateDelay = 0;
//
//  //set the new gateReleaseAdjustment number
//  gateReleaseAdjustment = gateDelay;
//
//}


