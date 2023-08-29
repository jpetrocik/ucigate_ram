#ifndef EE24CCxxx_H
#define EE24CCxxx_H

#include <Arduino.h>
#include "ByteBuffer.h"


#define EEPROM_SD 0x50
#define MAX_READ_SIZE 32
#define RING_BUFFER_SIZE 256

class File {
 private:
  //eeprom starting address
  uint16_t startAddress;  

  //number of bytes available to read
  uint16_t length;

  //current address read to fill ring_buffer
  volatile uint16_t currentAddress;

  //total number of bytes read, most be <length
  volatile uint16_t readCounter;

  //buffer to hold future reads
  ByteBuffer ringBuffer;

public:
  File(void);
  void open(uint16_t startAddr, uint16_t length);
  int read();
  uint16_t available();
  boolean seek(uint16_t pos);
  uint16_t position();
  uint16_t size();  

private:
	void fillBuffer(int bytes);
};


#endif