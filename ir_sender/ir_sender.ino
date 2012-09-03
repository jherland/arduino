/*
 * This sketch will send out an IR command.
 *
 * Set up Timer1 in CTC mode, with TOP == OCR1A, and make it trigger at a rate
 * of 2 x 38 KHz, and then toggle OC1A/PB1 (Arduino pin 9) at every trigger.
 * This generates a 38 KHz (50% duty cycle) square wave on that pin.
 *
 * We then modulate that wave using the IR command timings below.
 */

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
	Serial.begin(115200);

	cli(); // Disable interrupts while setting up

	// Set up:
	//  - DDRB1 = 1 (PORTB1/OC1A in output mode)
	DDRB |= B10; // DDRB7:0

	//  - Timer1:
	//     - COM1A1:0 = 0b10 (Clear OC1A on Compare Match (see IR_on below))
	//     - WGM13:0 = 0b0100 (CTC mode, reset on OCR1A Match)
	//     - CS12:0 = 0b001 (no prescaling)
	//  - No interrupts
	TCCR1A = B10000000; // COM1A1 COM1A0 COM1B1 COM1B0 - - WGM11 WGM10
	TCCR1B = B00001001; // ICNC1 ICES1 - WGM13 WGM12 CS12 CS11 CS10
	TCCR1C = B00000000; // FOC1A FOC1B - - - - - -
	TIMSK1 = B00000000; // - - ICIE1 - - OCIE1B OCIE1A TOIE1

	//  - Output Compare Register 1 A:
	//     - OCR1A = 211 (match every 13.16 usec (== 2 x 38 KHz)
	//  - Reset Timer/Counter1
	OCR1A = 209;
	TCNT1 = 0x0000;

	//  - Reset interrupt flags (by writing a logical 1 into them)
	TIFR1 = B00100111; // - - ICF1 - - OCF1B OCF1A TOV1

	sei(); // Re-enable interrupts
}

/*
 * Modulate the IR carrier by switching between
 *  - COM1A1:0 == 0b01 (Toggle OC1A on compare match, i.e. carrier present)
 *  - COM1A1:0 == 0b10 (Clear OC1A on compare match, i.e. carrier absent)
 */
#define IR_on()  (TCCR1A = B01000000)
#define IR_off() (TCCR1A = B10000000)

void loop(void)
{
	unsigned long t;

	Serial.println("Sending IR signal");
	Serial.flush();
	t = millis();
	cli(); // turn off background interrupts
	for (size_t r = 0; r < repeats; r++) {
		for (size_t i = 0; i < timings_len; i++) {
			i % 2 == 0 ? IR_on() : IR_off();
			// Split time_left in 16 msec chunks, since
			// delayMicroseconds() cannot handle >16.383 msec delays
			size_t time_left = timings[i];
			while (time_left > 16000) {
				delayMicroseconds(16000);
				time_left -= 16000;
			}
			delayMicroseconds(time_left);
		}
	}
	sei(); // turn on background interrupts
	t = millis() - t;
	Serial.print("Sent IR signal in ");
	Serial.print(t);
	Serial.println(" msecs");

	delay(interval);
}
