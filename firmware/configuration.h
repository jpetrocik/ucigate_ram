#define BOARD_V4_X

/**
 * A very long debounce delay, so if the user accidentally
 * presses the button multiple time, it's not interrupted 
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

#ifdef BOARD_V4_X
  #define GATE 6
  #define LIGHT_RED 5
  #define LIGHT_YELLOW_1 4
  #define LIGHT_YELLOW_2 3
  #define LIGHT_GREEN 2
  #define SPEAKER 9
  #define START 7
  #define TIMER_1 23
  #define TIMER_2 24
#endif

#ifdef BOARD_V3_X_WHITTIER 
  #define GATE 6
  #define LIGHT_RED 5
  #define LIGHT_YELLOW_1 3
  #define LIGHT_YELLOW_2 4
  #define LIGHT_GREEN 2
  #define SPEAKER 9
  #define START 7
  #define TIMER 0
#endif

#ifdef BOARD_V3_X
  #define GATE 6
  #define LIGHT_RED 2
  #define LIGHT_YELLOW_1 5
  #define LIGHT_YELLOW_2 3
  #define LIGHT_GREEN 4
  #define SPEAKER 9
  #define START 7
  #define TIMER 0
#endif


/**
 * Changable for boards where UART is a SerialX
 */
#define SerialPort Serial


