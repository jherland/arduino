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
 * The three PWM outputs are driven using analogWrite() from the main loop.
 *
 * The main loop implements a 3-channel LED controller, where the
 * pushbutton is used to cycle through the Red/Green/Blue channels, and
 * rotation adjusts the level/value of the current channel. The RGB LED
 * displays the color of the current channel, with intensity proportional
 * to the level/value of the current channel.
 *
 * Author: Johan Herland <johan@herland.net>
 * License: GNU GPL v2 or later
 */

#include <RF12.h>

// Utility macros
#define ARRAY_LENGTH(a) ((sizeof (a)) / (sizeof (a)[0]))
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define LIMIT(min, val, max) (min > val ? min : (max < val) ? max : val)

const byte VERSION = 2;

const byte rfm12b_band = RF12_868MHZ;
const byte rfm12b_group = 123;
const byte rfm12b_local_id = 13;
const byte rfm12b_remote_id = 0;

volatile byte ring_buffer[256] = { 0 };
volatile byte rb_write; // current write position in ring buffer
byte rb_read; // current read position in ringbuffer

byte rot_state = 0;
bool button_state = 0;

byte rot_values[3] = { 0 }; // R/G/B channel values
byte cur_channel = 0; // Index into above array

const byte pwm_pins[3] = {5, 6, 9};

// Format of rfm12b data packets used to send commands and status updates:
struct rc_data {
	uint8_t modify   : 1; // Modify (=1) or Read (=0) command.
	uint8_t relative : 1; // Next byte is relative (=1) or absolute (=0).
	uint8_t channel  : 6; // Channel number to be written/read
	union {
		uint8_t abs_value;
		int8_t rel_value;
	};
};

struct rc_packet {
	uint8_t hdr; // RF12 packet header
	struct rc_data d; // RF12 packet payload
};

/*
 * RFM12B network protocol between this rotary remote control (RRC) and
 * the remote nodes that "channels" and "levels":
 *
 * The objective of this network protocol is to allow an RRC to read and
 * modify the current level of a channel on a remote node. The RRC knows
 * the remote node ID and channel number beforehand, but must be able to
 * the request the current channel level, and also request a modification
 * of the channel level across the network. Furthermore, the channel level
 * may be manipulated by other means (either from other RRCs, or via some
 * other mechanism), so the RRC needs to receive updates of the current
 * channel level. The channel and its associated level is "owned" by the
 * remote node, but the RRC needs to know it in order to display useful
 * feedback to its user.
 *
 * More specifically, the RFM12B messages needed are:
 *
 * - Status request: From RRC to remote node to request current level of
 *   a given channel. Packet data: {0, 0, $channel, 0}
 * - Status update: Broadcast from remote node, to inform of current level
 *   of a given channel. Packet data: {0, 0, $channel, $abs_value}
 * - Change request: From RRC to remote node to request a change in the
 *   level of a given channel. Packet data: {1, 1, $channel, $rel_value}
 *                             OR           {1, 0, $channel, $abs_value}
 *
 * The network traffic proceeds as follows:
 *
 * - When switching to a (different) channel, the RRC typically sends a
 *   status request to the remote node, to establish the current level for
 *   that channel.
 *
 * - Upon receiving a status request, the remote node _broadcasts_ a
 *   status update containing the current level of that channel.
 *
 * - When rotation occurs, the RRC sends a change request to the remote
 *   node, which should adjust its channel accordingly.
 *
 * - Whenever the remote node adjusts a channel, the new level of that
 *   channel should be _broadcast_ as a status update.
 *
 * Fundamentally, the RRC is free to send status requests at any time, and
 * the remote node is free to broadcast status updates at any time.
 */

struct rc_packet out_buf[16]; // RFM12B packet output buffer
byte ob_write; // current write position in out_buf ringbuffer
byte ob_read; // current read position in out_buf ringbuffer

void setup(void)
{
	Serial.begin(115200);

	cli(); // Disable interrupts while setting up

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
	 * In addition to the above, we will use the Timer/Counter1
	 * overflow interrupt as a crude debouncing timer for the
	 * pushbutton: When a pushbutton event happens, we'll reset the
	 * Counter1 value to 0, and enable the overflow interrupt. If more
	 * pushbutton events happen, we'll reset the counter. When the
	 * overflow interrupt finally happens (after ~2.4ms, a side effect
	 * of how analogWrite() sets up the PWM-ing), we'll disable the
	 * interrupt, and do a "proper" reading of the pushbutton state.
	 */

	sei(); // Re-enable interrupts

	// Set up RFM12B communication
	rf12_initialize(rfm12b_local_id, rfm12b_band, rfm12b_group);

	status_request(next_packet(), cur_channel);

	Serial.print(F("Rotary Encoder version "));
	Serial.println(VERSION);
	Serial.print(F("Using RFM12B group "));
	Serial.print(rfm12b_group);
	Serial.print(F(" from node "));
	Serial.print(rfm12b_local_id);
	Serial.print(F(" to node "));
	Serial.print(rfm12b_remote_id);
	Serial.print(F(" @ "));
	Serial.print(rfm12b_band == RF12_868MHZ ? 868
		  : (rfm12b_band == RF12_433MHZ ? 433 : 915));
	Serial.println(F("MHz"));
	Serial.println(F("Ready"));
}

/*
 * PCINT1 interrupt vector
 *
 * Append the current values of the relevant input port to the ring buffer.
 */
ISR(PCINT1_vect)
{
	ring_buffer[rb_write++] = PINC & B00000111;
}

ISR(TIMER1_OVF_vect)
{
	TIMSK1 &= ~1; // Unset TOIE1 to disable Timer1 overflow interrupt
	ring_buffer[rb_write++] = (PINC & B00000111) | B00001000;
}

void start_debounce_timer(void)
{
	// Reset Counter1 value, and enable overflow interrupt
	TCNT1 = 0; // Reset Counter1
	TIMSK1 |= 1; // Set TOIE1 to enable Timer1 overflow interrupt
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
	bool debounced = ring_buffer[rb_read] & B1000;
	bool button_pin = ring_buffer[rb_read] & B0100;
	if (button_pin != button_state) {
		if (!debounced)
			start_debounce_timer();
		else {
			events |= button_pin ? BTN_DOWN : BTN_UP;
			button_state = button_pin;
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
	status_request(next_packet(), cur_channel);
}

void update_value(int increment)
{
	// Adjust current channel, but limit to 0 <= level <= 255
	int level = LIMIT(0x00, rot_values[cur_channel] + increment, 0xff);
	rot_values[cur_channel] = level;
	// Display adjusted level
	analogWrite(pwm_pins[cur_channel], 0xff - level);
	// Ask for remote end to update its status
	adjust_request(next_packet(), cur_channel, increment);
}

void print_state(char event)
{
	Serial.print(event);
	Serial.print(F(" "));
	Serial.print(cur_channel);
	Serial.print(F(":"));
	Serial.println(rot_values[cur_channel]);
}

void print_byte_buf(volatile byte * buf, size_t len)
{
	for (size_t i = 0; i < len; i++) {
		Serial.print(F(" "));
		if (buf[i] <= 0xf)
			Serial.print(F("0"));
		Serial.print(buf[i], HEX);
	}
	Serial.println();
}

bool recv_update(void)
{
	if (rf12_recvDone()) {
		Serial.println(F("Received RFM12B communication:"));
		if (rf12_crc)
			Serial.println(F("  CRC mismatch. Discarding!"));
		else {
			Serial.print(F("  rf12_hdr: 0x"));
			Serial.println(rf12_hdr, HEX);
			Serial.print(F("  rf12_len: "));
			Serial.println(rf12_len);
			Serial.print(F("  rf12_data(hex):"));
			print_byte_buf(rf12_data, rf12_len);
			return true;
		}
	}
	return false;
}

struct rc_packet * next_packet(void)
{
	byte i = ob_write;
	++ob_write %= ARRAY_LENGTH(out_buf);
	return out_buf + i;
}

void status_request(struct rc_packet * packet, byte channel)
{
	if (channel >= 64) {
		Serial.print(F("Illegal channel number: "));
		Serial.println(channel);
		return;
	}

	// Regular packet destined for remote node id
	packet->hdr = RF12_HDR_DST | (RF12_HDR_MASK & rfm12b_remote_id);
	packet->d.modify = 0;
	packet->d.relative = 0;
	packet->d.channel = channel;
	packet->d.abs_value = 0;
}

void adjust_request(struct rc_packet * packet, byte channel, int8_t adjust)
{
	if (channel >= 64) {
		Serial.print(F("Illegal channel number: "));
		Serial.println(channel);
		return;
	}

	// Regular packet destined for remote node id
	packet->hdr = RF12_HDR_DST | (RF12_HDR_MASK & rfm12b_remote_id);
	packet->d.modify = 1;
	packet->d.relative = 1;
	packet->d.channel = channel;
	packet->d.rel_value = adjust;
}

bool service_out_buf(void)
{
	if (ob_write != ob_read && rf12_canSend()) {
		// Send next scheduled packet
		byte i = ob_read;
		++ob_read %= ARRAY_LENGTH(out_buf);
		struct rc_packet * p = out_buf + i;
		rf12_sendStart(p->hdr, &(p->d), sizeof p->d);
		Serial.print(F("Sending packet: "));
		Serial.print(p->hdr, HEX);
		print_byte_buf((byte *) &(p->d), sizeof p->d);
		return true;
	}
	return false;
}

void loop(void)
{
	service_out_buf();
	recv_update();

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

	// TODO: Low power mode
	// TODO: Run on JeeNode Micro v2?
}
