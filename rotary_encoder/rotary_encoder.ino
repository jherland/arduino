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

// TODO: Display current color at full intensity while button is pressed.
// TODO: Rotating while pressed should browse through channels.
// TODO: Communicate range from host to controller
// TODO: Dynamic discovery of channels
// TODO: Dynamic discovery of multiple hosts
// TODO: Packet radio collision robustness

// #define DEBUG 1

#if DEBUG
#define LOG Serial.print
#define LOGln Serial.println
#else
#define LOG(...)
#define LOGln(...)
#endif

#define RCN_CTRL_MAX_CHANNELS 7

#include <RF12.h> // Needed by RCN
#include <Ports.h> // Needs Sleepy::*
#include <rcn_controller.h> // Needs RCN_Controller

// Utility macros
#define ARRAY_LENGTH(a) ((sizeof (a)) / (sizeof (a)[0]))
#define MIN(x, y) ((x) < (y) ? (x) : (y))
#define MAX(x, y) ((x) > (y) ? (x) : (y))
#define LIMIT(min, val, max) (min > val ? min : (max < val) ? max : val)

const byte VERSION = 4;
const unsigned long max_idle_time = 10000; // msecs until we go to sleep
const unsigned long long_click_time = 3000; // min msecs for long-click

volatile byte ring_buffer[256] = { 0 };
volatile byte rb_write; // current write position in ring buffer
byte rb_read; // current read position in ringbuffer

byte rot_state = 0;
bool button_state = 0;
bool button_debounce = 0;

byte cur_channel = 0; // Index into above array

enum PWM_PINS { // Arduino PWM pins hooked up to RGB LED
	PWM_RED = 5,
	PWM_GREEN = 6,
	PWM_BLUE = 9
};

unsigned long last_activity = 0;
bool groggy = false; // Set to true, when we've just woken up from sleep

struct color { uint8_t red, green, blue; };
const struct color COLORS[RCN_CTRL_MAX_CHANNELS] = {
	{0xff, 0xff, 0xff}, // white
	{0xff, 0x00, 0x00}, // red
	{0xff, 0x7f, 0x00}, // orange
	{0xff, 0xff, 0x00}, // yellow
	{0x00, 0xff, 0x00}, // green
	{0x00, 0x00, 0xff}, // blue
	{0x7f, 0x00, 0xff}  // violet
};
enum COLOR { WHITE, RED, ORANGE, YELLOW, GREEN, BLUE, VIOLET };

void fade_pin(int pin, uint8_t value)
{
	if (value == 0)
		digitalWrite(pin, HIGH);
	else if (value == 0xff)
		digitalWrite(pin, LOW);
	else
		analogWrite(pin, 0xff - value);
}

void display_color(
	const struct color& c, uint8_t level, uint8_t range = 0xff)
{
	struct color result;
	result.red = ((uint16_t) level * c.red) / range;
	result.green = ((uint16_t) level * c.green) / range;
	result.blue = ((uint16_t) level * c.blue) / range;
	fade_pin(PWM_RED,   result.red);
	fade_pin(PWM_GREEN, result.green);
	fade_pin(PWM_BLUE,  result.blue);
}

void update_notify(
	uint8_t channel, // The channel id
	uint8_t range, // The registered range for this channel
	uint8_t data, // The auxiliary data for this channel
	uint8_t old_level, // The old/previous level
	uint8_t new_level) // The new/current level
{
#ifdef DEBUG
	LOG(F("update_notify("));
	LOG(channel);
	LOG(F(", "));
	LOG(range);
	LOG(F(", "));
	LOG(data);
	LOG(F(", "));
	LOG(old_level);
	LOG(F(", "));
	LOG(new_level);
	LOGln(F(")"));
#endif
	if (channel == cur_channel) // Display adjusted level
		display_color(COLORS[data], new_level, range);
}

RCN_Controller ctrl(RF12_868MHZ, 123, 16, update_notify);

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

	ctrl.init();

	// Initialize PWM pins.
	pinMode(PWM_RED,   OUTPUT);
	pinMode(PWM_GREEN, OUTPUT);
	pinMode(PWM_BLUE,  OUTPUT);
	display_color(COLORS[WHITE], 0); // black

	for (int i = 0; i < ARRAY_LENGTH(COLORS); i++)
		ctrl.add_channel(0xff, 0, i);

	// Request current channel level
	ctrl.sync(cur_channel);

#if DEBUG
	LOGln(F("Ready"));
#endif
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
	NO_EVENT   = 0,
	ROT_CW     = 1,  // Mutually exclusive with ROT_CCW.
	ROT_CCW    = 2,  // Mutually exclusive with ROT_CW.
	BTN_DOWN   = 4,  // Mutually exclusive with BTN_UP.
	BTN_UP     = 8,  // Mutually exclusive with BTN_DOWN.
	IDLE       = 16, // Set when max_idle_time has passed w/o activity
	LONG_CLICK = 32, // Set when button press > long_click_time
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
		if (groggy) { // disregard the button push that woke us up
			button_state = button_pin;
			groggy = button_pin; // disable groggy on release
		}
		else if (button_debounce && tick) { // debounce finished
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
	if (events || groggy)
		last_activity = millis();
	else if (tick) {
		unsigned long inactivity = millis() - last_activity;
		if (button_state && inactivity > long_click_time)
			events |= LONG_CLICK;
		else if (inactivity > max_idle_time)
			events = IDLE;
	}
	return events;
}

void next_channel()
{
	// Go to next channel
	++cur_channel %= ctrl.num_channels();
	// Display level of the new channel
	ctrl.adjust(cur_channel, 0);
	// Ask for a status update on the new channel
	ctrl.sync(cur_channel);
}

void print_state(char event)
{
#if DEBUG
	LOG(event);
	LOG(F(" "));
	LOG(cur_channel);
	LOG(F(":"));
	LOGln(ctrl.get(cur_channel));
#endif
}

/*
 * Return false immediately if we cannot go to sleep, or go to sleep, and
 * return true when it's time to wake up again.
 */
bool go_to_sleep()
{
	if (rb_read != rb_write) // There is input to be processed
		return false;
	if (!ctrl.go_to_sleep()) // RCN network is busy
		return false;

#if DEBUG
	LOGln(F("Going to sleep..."));
#endif

	// Change PCINT8..10 to trigger only on A2 (pushbutton).
	PCMSK1 = B00000100; // - PCINT14 .. PCINT8

	// Disable periodic timer interrupt
	TIMSK2 &= ~1; // Clear TOIE2 to disable TOV2 interrupt

	// Animate LEDs to signal power-down
	for (int i = 0xff; i > 0; i -= MAX(i / 10, 1)) {
		display_color(COLORS[WHITE], i);
		delay(5);
	}

	// Go black
	display_color(COLORS[WHITE], 0);

#if DEBUG
	Serial.flush();
#endif
	Sleepy::powerDown();

	// If we fell asleep because of long-click, we need to swallow the
	// following button release without waking up.
	if (rb_write == rb_read + 1
	 && (ring_buffer[rb_read] & B1111) == B0011) {
		rb_read++; // Skip button release
		Sleepy::powerDown(); // Keep sleeping
	}
	return true;
}

void wake_up()
{
#if DEBUG
	LOGln(F("Waking up..."));
#endif
	ctrl.wake_up();

	// Animate LEDs to signal wakeup
	for (int i = 0; i < 0xff; i += MIN(MAX(i / 10, 1), 0xff)) {
		display_color(COLORS[WHITE], i);
		delay(5);
	}

	// Re-enable PWM output
	ctrl.adjust(cur_channel, 0);

	// Tell process_inputs() that we have just woken up
	groggy = true;

	// Re-enable periodic timer interrupt
	TIMSK2 |= 1; // Set TOIE2 to enable TOV2 interrupt

	// Change PCINT8..10 to trigger only on A0..A2
	PCMSK1 = B00000111; // - PCINT14 .. PCINT8

	// Request current channel level
	ctrl.sync(cur_channel);
}

void loop(void)
{
	ctrl.run();

	int events = process_inputs();

	if (events & BTN_DOWN)
		print_state('v');
	else if (events & BTN_UP) {
		next_channel();
		print_state('^');
	}

	if (events & ROT_CW) {
		ctrl.adjust(cur_channel, MAX(1, ctrl.get(cur_channel) / 2));
		print_state('>');
	}
	else if (events & ROT_CCW) {
		ctrl.adjust(cur_channel, -MAX(1, ctrl.get(cur_channel) / 3));
		print_state('<');
	}

	if (events & (LONG_CLICK | IDLE) && go_to_sleep())
		wake_up();
}
