#define BOARD_V3_0_WHITTIER

/**
 * To switch between RAM and MAG gate
 */
#define RAM_GATE
//#define MAG_GATE

/**
 * A very long debounce delay, so if the user accidentally
 * presses the button multiple time, it's not interpreted 
 * as multiple presses
 */
#define DEBOUNCE_DELAY 1000

/**
 * Firmware Version number
 */
#define FIRMWARE_VERSION "2.0.0"

/**
 * The time in millis before green 0-120 are valid
 */
#define GATE_RELEASE_ADJUSTMENT 70

#ifdef BOARD_V3_0_WHITTIER 
  #define GATE 6
  #define LIGHT_RED 5
  #define LIGHT_YELLOW_1 3
  #define LIGHT_YELLOW_2 4
  #define LIGHT_GREEN 2
  #define SPEAKER 9
  #define START 7
  #define TIMER 0
#endif

#ifdef BOARD_V3_0
  #define GATE 6
  #define LIGHT_RED 2
  #define LIGHT_YELLOW_1 5
  #define LIGHT_YELLOW_2 3
  #define LIGHT_GREEN 4
  #define SPEAKER 9
  #define START 7
  #define TIMER 0
#endif


#ifdef BOARD_V2_3 
  #define GATE 6
  #define LIGHT_RED 5 
  #define LIGHT_YELLOW_1 4
  #define LIGHT_YELLOW_2 3
  #define LIGHT_GREEN 2
  #define SPEAKER 9
  #define START 12
  #define TIMER 0
#endif


#ifdef BOARD_V2_2 
  #define GATE 2
  #define LIGHT_RED 3 
  #define LIGHT_YELLOW_1 4
  #define LIGHT_YELLOW_2 5
  #define LIGHT_GREEN 6
  #define SPEAKER 9
  #define START 7
  #define TIMER 0
#endif

/**
 * Changable for boards where UART is a SerialX
 */
#define SerialPort Serial


