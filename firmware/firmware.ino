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
long nextEventTime = 0; //next time a cadence event is triggered
long startTime = 0; //the time when the gate droppered. used by timer
int cadenceStarted = false; //whether the cadence has started
int nextSplitTimer = -1; //whether the timer has started
int timerStarted = 0;
int gatePosition = GATE_POSITION_DOWN ; //the current position of the gate
long lastButtonRead = 0;  //last time push button was read
int cadenceState = 0; //the current state of the cadence
int gateReleaseAdjustment = GATE_RELEASE_ADJUSTMENT; //the time to prerelease the gate

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
  pinMode(TIMER_1, INPUT);
  pinMode(TIMER_2, INPUT);
  pinMode(TIMER_3, INPUT);
  pinMode(TIMER_4, INPUT);

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
  
int checkButton(int pin){
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

  
  //Take action when buttonPushed
  int isPressed = checkButton(START);
  if (isPressed) {
    buttonPressed();
  }


  /**
   * If candence is running process events
   */
  if (cadenceStarted) {
    runCadence();
  }
  
  /*
   * Check timers in sequence
   */
   if (timerStarted){
    checkNextTimer();
   }

}

/*
 * Called when the physical button is pressed
 * or over bluetooth
 */ 
void buttonPressed() {
     if (cadenceStarted) {
      cancelCadence();
    } else if (gatePosition == GATE_POSITION_DOWN ) {
      raiseGate();
    } else if (gatePosition == GATE_POSITION_UP ) {
      startCadence();
    }
}

void checkNextTimer() {
  if (nextSplitTimer == TIMER_1) {
    if (checkTimer(TIMER_1)) {
      nextSplitTimer == TIMER_2;
    }
  } else if (nextSplitTimer == TIMER_2) {
    if (checkTimer(TIMER_2)) {
      nextSplitTimer == TIMER_3;
    }
  } else if (nextSplitTimer == TIMER_3) {
    if (checkTimer(TIMER_3)) {
      nextSplitTimer == TIMER_4;
    }
  } else if (nextSplitTimer == TIMER_4) {
    if (checkTimer(TIMER_4)) {
      nextSplitTimer == 0;
    }
  }  
}

long checkTimer(int whichTimer){
    int timerStatus = digitalRead(whichTimer);
    if ( timerStatus == HIGH  ){
      long split = millis()-startTime;
      sendEvent("EVNT_TIMER", String(split));
      return split;
    } 
    return -1;
}

void runCadence() {
  long currentTime = millis();

  //check whether the currentTime is passed the time
  //when the next event will occur
  if (currentTime > nextEventTime) {
    
    if (cadenceState == STARTED) {

      nextEventTime = currentTime + 50;

      cadenceState = OK_RIDERS;
    } else if (cadenceState == OK_RIDERS) {

      // "OK RIDERS RANDOM START" : 1:50+1:80 sec
      sendEvent("EVNT_OK_RIDERS","");
      trmpcm.play(0,13224);

      nextEventTime += 3300;
      
      cadenceState = RIDERS_READY;
    } else if (cadenceState == RIDERS_READY) {

      // "RIDERS READY - WATCH THE GATE" : 2.00 sec
      sendEvent("EVNT_RIDERS_READY","");
      trmpcm.play(32000,17535);

      nextEventTime += 2110;

      cadenceState = RANDOM_PAUSE;
    } else if (cadenceState == RANDOM_PAUSE) {
      trmpcm.disable();

      //Random delay .1 - 2.7 sec
      //offical delay of .1 was to fast, using .250
      nextEventTime += random(550, 2700);
      cadenceState = RED_LIGHT;
      
    } else if (cadenceState == RED_LIGHT) {
       //Red Light
      lightOn(LIGHT_RED, 60);
      sendEvent("EVNT_RED_LIGHT","");

      nextEventTime += 120;

      cadenceState = YELLOW1_LIGHT;
    } else if (cadenceState == YELLOW1_LIGHT) {
      //Yellow Light
      lightOn(LIGHT_YELLOW_1,60);
      sendEvent("EVNT_YELLOW_1_LIGHT","");

      nextEventTime += 120;

      cadenceState = YELLOW2_LIGHT;
    } else if (cadenceState == YELLOW2_LIGHT) {
      //Yellow Light
      lightOn(LIGHT_YELLOW_2,60);
      sendEvent("EVNT_YELLOW_2_LIGHT","");

      nextEventTime += (120 - gateReleaseAdjustment);
 
      cadenceState = DROP_GATE;
    } else if (cadenceState == DROP_GATE) {
      //drop gate
      digitalWrite(GATE, LOW);
      gatePosition = GATE_POSITION_DOWN ;

      nextEventTime += gateReleaseAdjustment;

      cadenceState = GREEN_LIGHT;
    } else if (cadenceState == GREEN_LIGHT) {
      //Green light
      lightOn(LIGHT_GREEN, 2250);
      startTime = millis();
      nextSplitTimer = TIMER_1;
      timerStarted = true;
      sendEvent("EVNT_GREEN_LIGHT","");

      nextEventTime += 3000;

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
  timerStarted = false;
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
      buttonPressed();
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



