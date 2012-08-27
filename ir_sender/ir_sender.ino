/*
 * This sketch will send out an IR command.
 *
 * See the full tutorial at http://www.ladyada.net/learn/sensors/ir.html
 * This code is public domain, please enjoy!
 */

// Where is the LED connected?
#define IR_LED_port PORTB
#define IR_LED_bit PORTB5
const unsigned int IR_LED_pin = 13;

const long timings[] = { // pulse lengths [µsecs]
	2376, 594, 1188, 594, 594, 594, 1188, 594, 594, 594, 1188, 594,
	594, 594, 594, 594, 1188, 594, 594, 594, 594, 594, 594, 594, 594,
	25700,
};

const size_t timings_len = sizeof(timings)/sizeof(timings[0]);
const size_t repeats = 2; // How many times to run the above timings.
const size_t interval = 3 * 1000; // delay between sending command [msecs]


void setup(void)
{
	pinMode(IR_LED_pin, OUTPUT);
	Serial.begin(9600);
}


// Send 38KHz pulses to the IR_LED_pin for the given number of µsecs
void pulseIR(long usecs)
{
	while (usecs > 0) {
		// 38 kHz is about 13 µsecs high and 13 µsecs low
		IR_LED_port |= _BV(IR_LED_bit);
		delayMicroseconds(13);
		IR_LED_port &= ~_BV(IR_LED_bit);
		delayMicroseconds(12);

		// so 26 µsecs altogether
		usecs -= 26;
	}
}


// Send the IR pulses described in the timings array.
void sendIRcode(void)
{
	boolean on = true;

	cli(); // turn off background interrupts

	for (size_t i = 0; i < timings_len; i++) {
		if (on)
			pulseIR(timings[i]);
		else
			delayMicroseconds(timings[i]);
		on = !on;
	}

	sei(); // turn on background interrupts
}


void loop(void)
{
	Serial.println("Sending IR signal");
	for (size_t i = 0; i < repeats; i++)
		sendIRcode();

	delay(interval);
}
