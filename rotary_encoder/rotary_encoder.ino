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
 *  1: Red LED cathode
 *  2: Green LED cathode
 *  3: Pushbutton switch
 *  4: Blue LED cathode
 *  5: Common anode for LEDs, and pushbutton switch
 *
 * Arduino hookup:
 *  - Encoder pin C to GND.
 *  - Encoder pins A and B to PC0..1 (Arduino pins A0 and A1)
 *    (rotation code inputs; flip them to swap CW vs. CCW rotation).
 *  - Encoder pin 5 to Vcc (5V or 3.3V).
 *  - Encoder pin 3 to PC2 (Arduino pin A2) with a 10K pull-down resistor
 *    (pushbutton input).
 *  - Encoder pins 1, 2 and 4 through current limiting resistors and on to
 *    D5, D6 and B1 (Arduino pins 5, 6 and 9), respectively (PWM outputs
 *    for RGB LED hooked up to D5/OC0B, D6/OC0A and B1/OC1A, respectively).
 *
 * Diagram:
 *
 *   Encoder         Arduino
 *     ---            -----
 *    |  A|----------|A0   |
 *    |  C|------*---|GND  |
 *    |  B|------+---|A1   |
 *    |   |      |   |     |
 *    |   |      R4  |     |
 *    |   |      |   |     |
 *    |  1|--R3--+---|5    |
 *    |  2|--R2--+---|6    |
 *    |  3|------*---|A2   |
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
 * trigger a pin change interrupt (PCINT1). The corresponding ISR merely
 * forwards the current state of the input pins to the main loop by using
 * a simple ring buffer. This keeps the ISR very short and fast, and
 * ensures that we miss as few interrupts as possible.
 *
 * The main loop, and associated functions read the input events from the
 * ring buffer, and use them to drive a Controller node in a Remote
 * Controller Network. See rcn_common.h in the RCN library for more
 * information on RCN. As an RCN Controller, we control 3 different RCN
 * Channels, and assign them to each of the R, G and B components of the
 * RGB LED on the rotary encoder. The R/G/B components are drive by three
 * PWM outputs, which are controlled by analogWrite().
 *
 * The pusbutton input is used to cycle through the 3 channels, and the
 * current Channel is displayed in the corresponding (R, G or B) color on
 * the RGB LED. The intensity of the color is proportional to the Level of
 * the currently selected Channel. Rotating the knob adjusts the Level for
 * the currently selected channel.
 *
 * Author: Johan Herland <johan@herland.net>
 * License: GNU GPL v2 or later
 */

// #define DEBUG 1

#if DEBUG
#define LOG Serial.print
#define LOGln Serial.println
#else
#define LOG(...)
#define LOGln(...)
#endif

#include <RF12.h> // Needed by rcn_common.h
#include <rcn_common.h> // Needs RCN_Node

// Utility macros
#define ARRAY_LENGTH(a) ((sizeof (a)) / (sizeof (a)[0]))
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define LIMIT(min, val, max) (min > val ? min : (max < val) ? max : val)

const byte VERSION = 3;
RCN_Node node(RF12_868MHZ, 123, 15);
RCN_Node::RecvPacket recvd; // RCN packet receive buffer
const byte rcn_remote_host = 1; // RFM12B node ID of remote RCN node
const unsigned long max_idle_time = 5000; // msecs until we go to sleep

volatile byte ring_buffer[256] = { 0 };
volatile byte rb_write; // current write position in ring buffer
byte rb_read; // current read position in ringbuffer

byte rot_state = 0;
bool button_state = 0;
bool button_debounce = 0;

byte rot_values[3] = { 0 }; // R/G/B channel values
byte cur_channel = 0; // Index into above array

const byte pwm_pins[3] = {5, 6, 9};

unsigned long last_activity = 0;

void setup(void)
{
#if DEBUG
	Serial.begin(115200);
#endif

	cli(); // Disable interrupts while setting up

	// Disable unused A/D converter to save power.
	ADCSRA &= ~ bit(ADEN);

	// Set up input pins (Arduino pins A0..A2 == PORTC pins 0..2).
	// Set PC0..2 as inputs:
	DDRC &= ~B00000111;
	// Enable PC0..1 internal pull-up resistors
	PORTC |= B00000011;

	// Set up PCINT8..10 interrupt to trigger on changing pins A0..A2.
	PCICR = B00000010; // - - - - - PCIE2 PCIE1 PCIE0
	PCMSK1 = B00000111; // - PCINT14 .. PCINT8

	// Set up pins 5/6/9 (D5/D6/B1) as output (for PWM)
	DDRD |= B01100000;
	DDRB |= B00000010;
	// Initialize RGB LED to all black
	for (int i = 0; i < ARRAY_LENGTH(pwm_pins); i++)
		analogWrite(pwm_pins[i], 0xff);

	/*
	 * Finally, we will use the Timer/Counter2 overflow interrupt
	 * (TOV2) as a general purpose timer. It will be used for
	 * debouncing and idle detection, so high resolution is not
	 * particularily important. We therefore use the highest prescaler
	 * available for this times, which yields a TOV2 freq. of ~61Hz.
	 */
	TCCR2B |= B00000111; // FOC2A FOC2B - - WGM22 CS22 CS21 CS20
	TCNT2 = 0; // Reset Counter2
	TIMSK2 |= 1; // Set TOIE2 to enable TOV2 interrupt

	sei(); // Re-enable interrupts

	node.init();

	node.send_status_request(rcn_remote_host, cur_channel);

	LOGln(F("Ready"));
}

/*
 * PCINT1 interrupt vector
 *
 * Append the current values of the input pins to the ring buffer.
 */
ISR(PCINT1_vect)
{
	ring_buffer[rb_write++] = PINC & B00000111;
}

/*
 * TOV2 interrupt vector
 *
 * Append the current values of the input pins plus a "timer" bit to the
 * ring buffer.
 */
ISR(TIMER2_OVF_vect)
{
	ring_buffer[rb_write++] = (PINC & B00000111) | B00001000;
}

enum input_events {
	NO_EVENT = 0,
	ROT_CW   = 1, // Mutually exclusive with ROT_CCW.
	ROT_CCW  = 2, // Mutually exclusive with ROT_CW.
	BTN_DOWN = 4, // Mutually exclusive with BTN_UP.
	BTN_UP   = 8, // Mutually exclusive with BTN_DOWN.
	IDLE    = 16, // Set when max_idle_time has passed w/o activity
};

/*
 * Check the ring buffer and return a bitwise-OR combination of the above
 * enum values.
 */
int process_inputs(void)
{
	int events = NO_EVENT;
	if (rb_read == rb_write) // Nothing added to the ring buffer
		return NO_EVENT;

	// Process the next input event in the ring buffer

	// Did the periodic timer trigger this event?
	bool tick = ring_buffer[rb_read] & B1000;

	// Did the pushbutton change since last reading?
	bool button_pin = ring_buffer[rb_read] & B0100;
	if (button_pin != button_state) {
		if (button_debounce && tick) { // debounce period expired
			events |= button_pin ? BTN_DOWN : BTN_UP;
			button_state = button_pin;
			button_debounce = false;
		}
		else { // start debounce period
			TCNT2 = 0; // Reset timer to delay next tick
			button_debounce = true;
		}
	}

	// Did the rotary encoder value change since last reading?
	byte rot_pins = ring_buffer[rb_read] & B11;
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

	// Check for idleness
	if (events)
		last_activity = millis();
	else if (tick && (millis() - last_activity > max_idle_time))
		events = IDLE;

	return events;
}

void next_channel()
{
	// Stop displaying current channel
	analogWrite(pwm_pins[cur_channel], 0xff);
	// Go to next channel
	++cur_channel %= ARRAY_LENGTH(rot_values);
	// Display level of the new channel
	analogWrite(pwm_pins[cur_channel], 0xff - rot_values[cur_channel]);
	// Ask for a status update on the current channel
	node.send_status_request(rcn_remote_host, cur_channel);
}

void update_value(int8_t incr)
{
	if (incr) {
		// Adjust current channel, but limit to 0 <= l <= 255
		int l = LIMIT(0x00, rot_values[cur_channel] + incr, 0xff);
		rot_values[cur_channel] = l;
		// Ask for remote end to update its status
		node.send_update_request_rel(
			rcn_remote_host, cur_channel, incr);
	}
	// Display adjusted level
	analogWrite(pwm_pins[cur_channel], 0xff - rot_values[cur_channel]);
}

void handle_status_update(const RCN_Node::RecvPacket& p)
{
	if (p.channel() >= ARRAY_LENGTH(pwm_pins)) {
		LOG(F("Illegal channel number: "));
		LOGln(p.channel());
		return;
	}

	if (p.relative()) {
		LOGln(F("Status update should not have relative level!"));
		return;
	}

	// Set channel according to status update
	LOG(F("Received status update for channel #"));
	LOG(p.channel());
	LOG(F(": "));
	LOG(rot_values[p.channel()]);
	LOG(F(" -> "));
	LOGln(p.abs_level());
	rot_values[p.channel()] = p.abs_level();

	// Trigger update of corresponding LED:
	if (p.channel() == cur_channel)
		update_value(0);
}

void print_state(char event)
{
	LOG(event);
	LOG(F(" "));
	LOG(cur_channel);
	LOG(F(":"));
	LOGln(rot_values[cur_channel]);
}

void loop(void)
{
	if (node.send_and_recv(recvd))
		handle_status_update(recvd);

	int events = process_inputs();

	if (events & BTN_DOWN)
		print_state('v');
	else if (events & BTN_UP) {
		next_channel();
		print_state('^');
	}

	if (events & ROT_CW) {
		update_value(MAX(1, rot_values[cur_channel] / 2));
		print_state('>');
	}
	else if (events & ROT_CCW) {
		update_value(-MAX(1, rot_values[cur_channel] / 3));
		print_state('<');
	}

	if (events & IDLE) {
		LOGln(F("IDLE"));
	}

	// TODO: Low power mode
	// TODO: Run on JeeNode Micro v2?
}
