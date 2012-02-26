/*
 * IR pulse decoder
 *
 * This program uses the Arduno and a three-pin IR receiver (like a
 * PNA4602 or IRM-3638N3 or similar part) to decode IR pulses (typically
 * transmitted by a remote control.
 *
 * Based on https://raw.github.com/adafruit/Raw-IR-decoder-for-Arduino/master/rawirdecode.pde
 *
 * Johan Herland <johan@herland.net>
 */

// Where is the Vout of the IR sensor connected?
const int sensorPin = 3; // Either digital pin 2 or digital pin 3
const int interrupt = sensorPin - 2; // Interrupt number at sensorPin
const int MaxCommandLength = 100; // Max number of pulses in IR command
const int MaxCommandTime = 100; // Max time of IR command (in msecs)

volatile unsigned long t1, t2;
volatile unsigned long timings[MaxCommandLength * 3];
volatile unsigned int i1, i2;

void irChange(void)
{
	t2 = micros();
	timings[i1] = t2 - t1;
	i1 = (i1 + 1) % (MaxCommandLength * 3);
	t1 = t2;
}

void setup(void)
{
	Serial.begin(9600);
	Serial.println("Ready");
	t1 = 0;
	i1 = 0;
	attachInterrupt(interrupt, irChange, CHANGE);
}

void loop(void)
{
	if ((i1 && i1 == i2) || i1 > MaxCommandLength * 2) {
		/*
		 * There are pulses, but none of them have arrived in the
		 * last period. Assume what's there constitutes a single
		 * IR command.
		 */
		Serial.print("Received IR command with ");
		Serial.print(i1 - 1);
		Serial.print(" pulses: BEGIN");
		for (unsigned int i = 1; i < i1; i++) {
			Serial.print(" ");
			Serial.print(timings[i]);
		}
		Serial.println(" END");
		i1 = 0;
	}
	i2 = i1;
	delay(MaxCommandTime);
}
