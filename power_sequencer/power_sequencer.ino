/*
 * Simple sketch for sequencing power-on of external equipment
 *
 * Drives output pins 2 - 12, then A0 - A5 high with half-second delays.
 *
 * Author: Johan Herland <johan@herland.net>
 * License: GNU GPL v2 or later
 */

#include <Ports.h> // Need Sleepy::*

// Utility macros
#define ARRAY_LENGTH(a) ((sizeof (a)) / (sizeof (a)[0]))

// pins A0-A5 == 14-19 in digitalWrite() speak.
int pins[] = {2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 14, 15, 16, 17, 18, 19};
int interval = 500; // msecs

void setup(void)
{
	// Set all pins to output low
	DDRB = 0xff;
	PORTB = 0x00;
	DDRC = 0xff;
	PORTC = 0x00;
	DDRD = 0xff;
	PORTD = 0x00;

	for (int i = 0; i < ARRAY_LENGTH(pins); i++) {
		digitalWrite(pins[i], HIGH);
		Sleepy::loseSomeTime(interval);
	}
	Sleepy::powerDown();
}

ISR(WDT_vect)
{
	Sleepy::watchdogEvent();
}

void loop(void)
{
}
