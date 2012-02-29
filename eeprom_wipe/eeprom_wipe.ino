/*
 * Wipe the EEPROM back to factory contents (all 0xff)
 */

#include <EEPROM.h>

// EEPROM size (1024 bytes on ATmega328
const size_t EEPROM_SIZE = 1024;


void setup()
{
	Serial.begin(9600);
	Serial.println("Wiping EEPROM");
	for (size_t i = 0; i < EEPROM_SIZE; i++) {
		EEPROM.write(i, 0xff);
		if (i % 100 == 0)
			Serial.println(i, DEC);
	}
	Serial.println("Done");
}

void loop()
{
}

