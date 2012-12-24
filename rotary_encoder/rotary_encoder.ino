/*
 * Driver for incremental rotary encoder w/support for pushbutton RGB LED.
 *
 * Objective:
 *
 * This program reads the incoming 2-bit pulses from an incremental rotary
 * encoder. The program also detects a 3rd pushbutton input. Finally, the
 * program drives 3 PWM outputs connected to an RGB LED. The program was
 * developed to work with Sparkfun's COM-10982 encoder.
 *
 *
 * Physical layout and hookup:
 *
 * The rotary encoder has the following layout:
 *
 *     1 2 3 4 5
 *     ^ ^ ^ ^ ^
 *     | | | | |
 *    -----------
 *   |           |
 *   |    ,-,    |
 *  <|   |   |   |>
 *   |    '-'    |
 *   |           |
 *    -----------
 *      |  |  |
 *      v  v  v
 *      A  C  B
 *
 * The pinout is as follows:
 *
 *  A: Rotary encoder bit A
 *  B: Rotary encoder bit B
 *  C: Ground (connected to A & B during rotation)
 *  1: Blue LED cathode
 *  2: Green LED cathode
 *  3: Pushbutton switch
 *  4: Red LED cathode
 *  5: Common anode for LEDs, and pushbutton switch
 *
 * Arduino hookup:
 *  - Encoder pin C to Arduino GND.
 *  - Encoder pins A and B to Arduino pins 2 and 3 (rotation code inputs;
 *    flip them to swap CW vs. CCW rotation).
 *  - Encoder pin 5 to Arduino Vcc (5V or 3.3V).
 *  - Encoder pin 3 to Arduino pin 4 with a 10K pull-down resistor
 *    (pushbutton input).
 *  - Encoder pins 4, 2 and 1 through current limiting resistors and on to
 *    Arduino pins 9, 10 and 11, respectively (PWM outputs for RGB LED).
 *
 * Diagram:
 *
 *   Encoder         Arduino
 *     ---            -----
 *    |  A|----------|2    |
 *    |  C|------*---|GND  |
 *    |  B|------+---|3    |
 *    |   |      |   |     |
 *    |   |      R4  |     |
 *    |   |      |   |     |
 *    |  1|--R3--+---|11   |
 *    |  2|--R2--+---|10   |
 *    |  3|------*---|4    |
 *    |  4|--R1------|9    |
 *    |  5|----------|Vcc  |
 *     ---            -----
 *
 * R1-R3: Current-limiting resistors, e.g. 330Ω
 * R4: Pull-down resistor, e.g. 10KΩ
 *
 * In the Arduino, the two rotary encoder inputs and the pusbutton input
 * trigger interrupts whose handler merely forwards the current state of
 * the inputs to the main loop by using a simple ring buffer. This keeps
 * the ISR very short and fast. The three PWM outputs are driven using
 * analogWrite() from the main loop.
 *
 * Author: Johan Herland <johan@herland.net>
 * License: GNU GPL v2 or later
 */

volatile byte ring_buffer[256] = { 0 };
volatile byte rb_write; // current write position in ring buffer
byte rb_read; // current read position in ringbuffer

byte pin_state = 0;
int encoder_value = 0;

void setup()
{
	Serial.begin(115200);

	cli(); // Disable interrupts while setting up

	// Set up input pins (Arduino pins 2/3/4 == PORTD pins 2/3/4).
	// Set PD2-4 as inputs:
	DDRD &= ~(_BV(DDD2) | _BV(DDD3) | _BV(DDD4));
	// Enable PD2-3 internal pull-up resistors
	PORTD |= _BV(PORTD2) | _BV(PORTD3);

	// Set up INT0/1 interrupts to trigger on changing pin 2/3.
	EICRA = B00000101; // - - - - ISC11 ISC10 ISC01 ISC00
	EIMSK = B00000011; // - - - - - - INT1 INT0
	EIFR  = B00000011; // - - - - - - INTF1 INTF0

	sei(); // Re-enable interrupts

	Serial.println(F("Ready"));
}

/*
 * INT0/1 interrupt vectors
 *
 * Append the current values of the relevant input pins to the ring buffer.
 */
ISR(INT0_vect)
{
  ring_buffer[rb_write++] = PIND;
}

ISR(INT1_vect, ISR_ALIASOF(INT0_vect));

void loop()
{
	if (rb_read == rb_write)
		return; // Nothing has been added to the ring buffer

	Serial.println((byte) rb_write - (byte) rb_read);
	// Process the next value in the ring buffer
	byte value = (ring_buffer[rb_read] >> 2) & 0b11;
	// Did the value actually change since last reading?
	if (value != (pin_state & 0b11)) {
		// Append to history of pin states
		pin_state = (pin_state << 2) | value;
		// Are we in a "rest" state?
		if (value == 0b11) {
			// Figure out how we got here
			switch (pin_state & 0b111111) {
			case 0b000111:
				// CCW
				encoder_value--;
				Serial.print("<- ");
				Serial.println(encoder_value);
				break;
			case 0b001011:
				// CW
				encoder_value++;
				Serial.print("-> ");
				Serial.println(encoder_value);
				break;
			}
		}
	}

	rb_read++;
}
