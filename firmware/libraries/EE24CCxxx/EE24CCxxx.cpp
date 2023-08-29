#include <Arduino.h>
#include <Wire.h>

#include "EE24CCxxx.h"

File::File() {
}

void File::open(uint16_t startAddr, uint16_t size) {

	startAddress=startAddr;
	length=size;
	currentAddress=startAddress;
	readCounter=0;


	//initalize ringBuffer and fill with data
	ringBuffer.deAllocate();
	ringBuffer.init(RING_BUFFER_SIZE);
	fillBuffer(RING_BUFFER_SIZE);

}

/**
 * Initializes the ring buffer with data
 */
void File::fillBuffer(int bytes){
	int endAddress;

	//protected from ringBuffer overflow
	bytes = (bytes < ringBuffer.getCapacity())?bytes:ringBuffer.getCapacity();

	endAddress = currentAddress + bytes;

	while (currentAddress < endAddress) {
		Wire.beginTransmission(EEPROM_SD);
	  Wire.write( (currentAddress >> 8) & 0xFF);
	 	Wire.write( (currentAddress >> 0) & 0xFF);
	 	Wire.endTransmission();

	 	Wire.requestFrom(EEPROM_SD, MAX_READ_SIZE);

	 	while (Wire.available()) {
			ringBuffer.put(Wire.read());
			currentAddress++;
	  }
	}

}

/**
 * Returns a buffered read and if space avaiable in the ringBuffer adds MAX_READ_SIZE bytes
 */
int File::read() {
  	if ( (ringBuffer.getCapacity() - ringBuffer.getSize()) > MAX_READ_SIZE)
  	{
  		fillBuffer(MAX_READ_SIZE);
	}

	readCounter++;
	return ringBuffer.get();
}

uint16_t File::available() {

  uint16_t n = size() - position();

  return n > 0X7FFF ? 0X7FFF : n;
}

boolean File::seek(uint16_t pos) {
	//protected from overflow
	pos = (pos > length)?length:pos;

	//no seeking backwards
	pos = (pos < readCounter)?readCounter:pos;

	//very ineffecent seek
	uint16_t seekAddress = startAddress + pos;
	while ( (startAddress+readCounter) < seekAddress){
		read();
	}

  return true;
}

uint16_t File::position() {
  return readCounter;
}

uint16_t File::size() {
  return length;
}
