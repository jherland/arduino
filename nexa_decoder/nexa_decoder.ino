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

#define ARRAY_LENGTH(a) ((sizeof (a)) / (sizeof (a)[0]))

enum PulseType {
	PULSE_A,  // ~10 150µs LOW pulse that starts the sync
	PULSE_B,  // ~2 643µs LOW pulse that continues the sync
	PULSE_X,  // ~1 236µs LOW pulse that is a half of a bit
	PULSE_Y,  // ~215µs LOW pulse that is the other half of a bit
	PULSE_H,  // ~310µs HIGH pulse
	PULSE_NO  // Not one of the others. An invalid pulse.
};

const unsigned int TIMINGS_LEN = 512;
unsigned int timings[TIMINGS_LEN];

void setup()
{
	RF_SETUP();
	Serial.begin(115200);
	Serial.println(F("rf_decoder ready:"));
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
 * Convert value from next_pulse() to one of the expected pulse types.
 */
/* enum PulseType */ int quantize_pulse(int p)
{
	// comments: min/max from observations
	if (p > 0 && p <= 1000) // 248 / 432
		return PULSE_H;
	else if (p < 0 && p >= -500) // -44 / -284
		return PULSE_Y;
	else if (p <= -500 && p >= -1500) // -1132 / -1300
		return PULSE_X;
	else if (p <= -2000 && p >= -3000) // -2592 / -2692
		return PULSE_B;
	else if (p <= -9000 && p >= -11000) // -10080 / -10208
		return PULSE_A;
	else
		return PULSE_NO;
}

/*
 * Look for the expected RF sync pattern, and return when detected.
 */
void detect_sync()
{
	// Sync pattern: A-H-B-H
	static const int sync[] = { PULSE_A, PULSE_H, PULSE_B, PULSE_H };
	unsigned int i = 0;
	while (i < ARRAY_LENGTH(sync))
		i = quantize_pulse(next_pulse()) == sync[i] ? i + 1 : 0;
}

void loop()
{
	const unsigned int BUF_SIZE = 1024;
	char buf[BUF_SIZE];
	unsigned int i = 0;

	detect_sync();
	buf[i++] = '>';
	while (i < BUF_SIZE - 1) {
		int p = next_pulse();
		switch (quantize_pulse(p)) {
			case PULSE_A:
				buf[i++] = 'A';
				break;
			case PULSE_B:
				buf[i++] = 'B';
				break;
			case PULSE_X:
				buf[i++] = 'X';
				break;
			case PULSE_Y:
				buf[i++] = 'Y';
				break;
			case PULSE_H:
				buf[i++] = 'H';
				break;
			default:
				buf[i++] = '\0';
				Serial.println(buf);
				Serial.print(F("and then "));
				Serial.println(p);
				return;
		}
	}
	buf[i++] = '\0';
	Serial.println(buf);
	Serial.flush();
}
