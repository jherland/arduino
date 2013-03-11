/*
 * Decoder sketch for 433 MHz receiver module receiving Nexa commands.
 *
 * Objective:
 *
 * Read the digital output from a RWS-371 or similar 433 MHz receiver, and
 * output the Nexa-style commands to the serial port.
 *
 * Connections:
 *
 *   -----------------------
 *  |  433 MHz RF Receiver  |
 *   -----------------------
 *    | | | |       | | | |
 *    1 2 3 4       5 6 7 8
 *
 * 1: GND
 * 2: Digital output (connect to PB0 (Arduino pin #8))
 * 3. Linear output (maybe: pull-down resistor to remove noise from pin 2)
 * 4: VCC (5V)
 * 5: VCC
 * 6: GND
 * 7: GND
 * 8: Optional Antenna (10-15 cm wire, or 35 cm wire)
 *
 * Author: Johan Herland <johan@herland.net>
 * License: GNU GPL v2 or later
 */

#include <limits.h>

// Adjust the following to match where the RF receiver is connected.
#define RF_SETUP() bitClear(DDRB, 0)
#define RF_READ()  bitRead(PINB, 0)

const unsigned int BUF_SIZE = 1536;
char buf[BUF_SIZE];
size_t i = 0;

enum {
	UNKNOWN, SX1, SX2, SX3,
	DA0, DA1, DA2, DA3,
	DB0, DB1, DB2, DB3,
} cur_state = UNKNOWN, new_state;

byte cur_bit = 0;

void setup()
{
	/*
	 * TODO: Connect RF input to ICP1/PB0 (Arduino pin #8), and set up
	 * an interrupt to be triggered and a timestamp recorded on every
	 * pin change.
	 */
	RF_SETUP();
	Serial.begin(115200);
	Serial.println(F("nexa_decoder ready:"));
}

/*
 * Read the next pulse from the RF receiver and return it.
 *
 * This will block until RF_READ() changes. At that point it will return
 * an int whose absolute value is the pulse length in µs, and the sign
 * is positive for a HIGH pulse and negative for a LOW pulse.
 *
 * This function must be called more often than the shortest pulse to be
 * detected.
 *
 * This function assumes that the longest pulse of interest is shorter
 * than INT_MAX µs. If the measured pulse length is longer, the returned
 * value will be pinned at INT_MAX or INT_MIN (for a HIGH and LOW pulse,
 * respectively).
 */
int next_pulse()
{
	static unsigned long start = 0;
	static int state = false;

	while (state == RF_READ())
		; // spin until state changes
	unsigned long now = micros();
	bool ret_state = state;
	state = RF_READ();

	int ret;
	if (ret_state)
		ret = (now - start > INT_MAX) ? INT_MAX : now - start;
	else
		ret = (start - now < INT_MIN) ? INT_MIN : start - now;
	start = now;
	return ret;
}

/*
 * Return number of bits needed to represent the given number.
 *
 * This is equivalent to floor(log2(v)) + 1.
 */
unsigned int numbits(unsigned int v)
{
	unsigned int ret = 0;
	while (v) {
		v >>= 1;
		ret++;
	}
	return ret;
}

/*
 * Classify the given pulse length into the following categories:
 *
 * 0: Invalid pulse length (i.e. not within any of the below categories)
 * 1:    0µs <= p <   512µs
 * 2:  512µs <= p <  2048µs
 * 3: 2048µs <= p <  4096µs
 * 4: 4096µs <= p <  8192µs
 * 5: 8192µs <= p < 16384µs
 *
 * The above categories (1..5) are positive (1..5) for HIGH pulses, and
 * negative (-1..-5) for LOW pulses.
 */
int quantize_pulse(int p)
{
	int sign = (p > 0) ? 1 : -1;
	p *= sign; // abs value
	p >>= 9; // divide by 512
	switch (numbits(p)) {
		case 0: // 0µs <= p < 512µs
			return sign * 1;
		case 1: // 512µs <= p < 1024µs
		case 2: // 1024µs <= p < 2048µs
			return sign * 2;
		case 3: // 2048µs <= p < 4096µs
			return sign * 3;
		case 4: // 4096µs <= p < 8192µs
			return sign * 4;
		case 5: // 8192µs <= p < 16384µs
			return sign * 5;
		default:
			return 0;
	}
}

void loop()
{
	new_state = UNKNOWN;
	int p = quantize_pulse(next_pulse()); // current pulse
	switch (p) {
		case -5: // LOW: 8192µs <= pulse < 16384µs
			new_state = SX1;
			break;
		case -3: // LOW: 2048µs <= pulse < 4096µs
			if (cur_state == SX2) // cmd format A
				new_state = SX3;
			break;
		case -2: // LOW: 512µs <= pulse < 2048µs
			if (cur_state == DA0) {
				cur_bit = '1';
				new_state = DA1;
			}
			else if (cur_state == DA2 && cur_bit == '0')
				new_state = DA3;
			else if (cur_state == SX2 || cur_state == DB1) {
				if (cur_state == SX2) { // cmd format B
					buf[i++] = '\n';
					buf[i++] = 'B';
				}
				new_state = DB2;
			}
			else if (cur_state == DB3 && cur_bit == '0') {
				buf[i++] = cur_bit;
				cur_bit = 0;
				new_state = DB0;
			}
			break;
		case -1: // LOW: 0µs <= pulse < 512µs
			if (cur_state == DA0) {
				cur_bit = '0';
				new_state = DA1;
			}
			else if (cur_state == DA2 && cur_bit == '1')
				new_state = DA3;
			else if (cur_state == DB3 && cur_bit == '1') {
				buf[i++] = cur_bit;
				cur_bit = 0;
				new_state = DB0;
			}
			break;
		case 2: // HIGH: 512µs <= pulse < 2048µs
			if (cur_state == DB2) {
				cur_bit = '1';
				new_state = DB3;
			}
			break;
		case 1: // HIGH: 0µs <= pulse < 512µs
			switch (cur_state) {
				case SX1:
					new_state = SX2;
					break;
				case SX3:
					buf[i++] = '\n';
					buf[i++] = 'A';
					new_state = DA0;
					break;
				case DA1:
					new_state = DA2;
					break;
				case DA3:
					buf[i++] = cur_bit;
					cur_bit = 0;
					new_state = DA0;
					break;
				case DB0:
					new_state = DB1;
					break;
				case DB2:
					cur_bit = '0';
					new_state = DB3;
					break;
			}
			break;
	}
	if (i >= BUF_SIZE - 5) { // Prevent overflowing buf
		buf[i++] = 'X';
		new_state = UNKNOWN;
	}
	if (cur_state != UNKNOWN && new_state == UNKNOWN) { // => UNKNOWN
		// Invalid data: Print buffer.
		// ...but only if it has > 8 valid bits
		if (i > 8 + 2) {
			buf[i++] = '\0';
			Serial.print(buf);
			Serial.print('>');
			Serial.print(cur_state);
			Serial.print('|');
			Serial.print(p);
			Serial.print('<');
			Serial.flush();
		}
		i = 0; // Restart buffer
	}
	cur_state = new_state;
}
