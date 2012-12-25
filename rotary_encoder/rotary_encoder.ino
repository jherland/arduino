/*
 * Driver for incremental rotary encoder w/built-in pushbutton and RGB LED.
 *
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
 *    Arduino pins 9, 10 and 11, respectively (PWM outputs for RGB LED
 *    hooked up to PB1/OC1A, PB2/OC1B and PB3/OC2A, respectively).
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
 * R1-R3: Current-limiting resistors, e.g. 220Ω
 * R4: Pull-down resistor, e.g. 10KΩ
 *
 *
 * Mode of operation:
 *
 * In the Arduino, the two rotary encoder inputs and the pusbutton input
 * trigger a pin change interrupt (PCINT2). The corresponding ISR merely
 * forwards the current state of the input pins to the main loop by using
 * a simple ring buffer. This keeps the ISR very short and fast, and
 * ensures that we miss as few interrupts as possible.
 *
 * The three PWM outputs are driven using analogWrite() from the main loop.
 *
 * The main loop implements a 3-channel LED controller, where the
 * pushbutton is used to cycle to Red/Green/Blue modes, and rotation is
 * used to adjust the level in each mode. The RGB LED displays the light
 * value of currently selected mode.
 *
 * Author: Johan Herland <johan@herland.net>
 * License: GNU GPL v2 or later
 */

volatile byte ring_buffer[256] = { 0 };
volatile byte rb_write; // current write position in ring buffer
byte rb_read; // current read position in ringbuffer

byte rot_state = 0;
bool button_state = 0;

byte rot_values[3] = { 0 }; // R/G/B mode values
byte mode = 0; // Index into above array

void setup(void)
{
	Serial.begin(115200);

	cli(); // Disable interrupts while setting up

	// Set up input pins (Arduino pins 2/3/4 == PORTD pins 2/3/4).
	// Set PD2-4 as inputs:
	DDRD &= ~B00011100;
	// Enable PD2-3 internal pull-up resistors
	PORTD |= B00001100;

	// Set up PCINT18..20 interrupt to trigger on changing pins 2/3/4.
	PCICR = B00000100; // - - - - - PCIE2 PCIE1 PCIE0
	PCMSK2 = B00011100; // PCINT23 .. PCINT16

	// Set up pins 9/10/11 (PB1..3) as output (for PWM)
	DDRB |= B00001110;
	// Initialize RGB LED to all black
	analogWrite( 9, 0xff);
	analogWrite(10, 0xff);
	analogWrite(11, 0xff);

	/*
	 * In addition to the above, we will use the Timer/Counter2
	 * overflow interrupt as a crude debouncing timer for the
	 * pushbutton: When a pushbutton event happens, we'll reset the
	 * Counter2 value to 0, and enable the overflow interrupt. If more
	 * pushbutton events happen, we'll reset the counter. When the
	 * overflow interrupt finally happens (after ~2.4ms, a side effect
	 * of how analogWrite() sets up the PWM-ing), we'll disable the
	 * interrupt, and do a "proper" reading of the pushbutton state.
	 */

	sei(); // Re-enable interrupts

	Serial.println(F("Ready"));
}

/*
 * PCINT2 interrupt vector
 *
 * Append the current values of the relevant input port to the ring buffer.
 */
ISR(PCINT2_vect)
{
	ring_buffer[rb_write++] = PIND & B00011100;
}

ISR(TIMER2_OVF_vect)
{
	TIMSK2 &= ~1; // Unset TOIE2 to disable Timer2 overflow interrupt
	ring_buffer[rb_write++] = (PIND & B00011100) | B00100000;
}

void start_debounce_timer(void)
{
	// Reset Counter2 value, and enable overflow interrupt
	TCNT2 = 0; // Reset Counter2
	TIFR2 |= 1; // Write 1 to TOV2 to clear Timer2 overflow flag
	TIMSK2 |= 1; // Set TOIE2 to enable Timer2 overflow interrupt
}

enum input_events {
	NO_EVENT = 0,
	ROT_CW   = 1, // Mutually exclusive with ROT_CCW.
	ROT_CCW  = 2, // Mutually exclusive with ROT_CW.
	BTN_DOWN = 4, // Mutually exclusive with BTN_UP.
	BTN_UP   = 8, // Mutually exclusive with BTN_DOWN.
};

/*
 * Check the ring buffer and return a bitwise-OR combination of the above
 * enum values.
 */
int process_inputs(void)
{
	int events = NO_EVENT;
	if (rb_read == rb_write)
		return NO_EVENT; // Nothing has been added to the ring buffer

	// Process the next input event in the ring buffer

	// Did the pushbutton change since last reading?
	bool debounced = ring_buffer[rb_read] & B100000;
	bool button_pin = ring_buffer[rb_read] & B10000;
	if (button_pin != button_state) {
		if (!debounced)
			start_debounce_timer();
		else {
			events |= button_pin ? BTN_DOWN : BTN_UP;
			button_state = button_pin;
		}
	}

	// Did the rotary encoder value change since last reading?
	byte rot_pins = (ring_buffer[rb_read] >> 2) & B11;
	if (rot_pins != (rot_state & B11)) {
		// Append to history of pin states
		rot_state = (rot_state << 2) | rot_pins;
		// Are we in a "rest" state?
		if (rot_pins == B11) {
			// Figure out how we got here
			switch (rot_state & B111111) {
			case B000111:
				events |= ROT_CCW;
				break;
			case B001011:
				events |= ROT_CW;
				break;
			}
		}
	}

	rb_read++;
	return events;
}

void next_mode()
{
	analogWrite(9 + mode, 0xff);
	++mode %= sizeof rot_values;
	analogWrite(9 + mode, 0xff - rot_values[mode]);
}

void update_value(int increment)
{
#define LIMIT(min, val, max) (min > val ? min : (max < val) ? max : val)
	int result = LIMIT(0x00, rot_values[mode] + increment, 0xff);
	rot_values[mode] = result;
	analogWrite(9 + mode, 0xff - result);
}

void print_state(char event)
{
	Serial.print(event);
	Serial.print(F(" "));
	Serial.print(mode);
	Serial.print(F(":"));
	Serial.println(rot_values[mode]);
}

void loop(void)
{
	int events = process_inputs();

	if (events & BTN_DOWN)
		print_state('v');
	else if (events & BTN_UP) {
		next_mode();
		print_state('^');
	}

#define MAX(x, y) ((x) > (y) ? (x) : (y))
	if (events & ROT_CW) {
		update_value(MAX(1, rot_values[mode] / 2));
		print_state('>');
	}
	else if (events & ROT_CCW) {
		update_value(-MAX(1, rot_values[mode] / 3));
		print_state('<');
	}

	// TODO: acceleration in rotation
	// TODO: Low power mode
	// TODO: RFM12B communication
}
