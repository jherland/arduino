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
 * 2: Digital output (connect to PC0 (Arduino pin A0))
 * 3. Linear output (maybe: pull-down resistor to remove noise from pin 2)
 * 4: VCC (5V)
 * 5: VCC
 * 6: GND
 * 7: GND
 * 8: Optional Antenna (10-15 cm wire, or 35 cm wire)
 *
 * Or:
 *
 *   -------------------
 *  |  433 MHz RF Recv  |
 *   -------------------
 *              | | | |
 *              1 2 3 4
 *
 * 1: VCC (5V)
 * 2 & 3: Digital output (connect to PC0 (Arduino pin A0))
 * 4: GND
 *
 * Author: Johan Herland <johan@herland.net>
 * License: GNU GPL v2 or later
 */

#include <limits.h>

#define DEBUG 0

#define ARRAY_LENGTH(a) ((sizeof (a)) / (sizeof (a)[0]))

// Adjust the following to match where the RF receiver is connected.
#define RF_SETUP() bitClear(DDRC, 0)
#define RF_READ()  bitRead(PINC, 0)

const size_t BUF_SIZE = 1280;
char buf[BUF_SIZE];
size_t buf_pos = 0;

enum {
	UNKNOWN, SX1, SX2, SX3,
	DA0, DA1, DA2, DA3,
	DB0, DB1, DB2, DB3,
} cur_state = UNKNOWN, new_state;

byte cur_bit = 0;

enum nexa_cmd_version {
	NEXA_INVAL = 0, // Unknown/invalid version
	NEXA_12BIT = 1, // Old 12-bit command format: DDDDDDDD011S
	NEXA_32BIT = 2  // New 32-bit command format: D{24}10GSCCCC
};

struct nexa_cmd {
	enum nexa_cmd_version version; // Command version/format, One of enum nexa_cmd_version
	byte device[3]; // 24-bit (A) or 8-bit (B) device identifier
	byte channel; // 4-bit intra-device channel identifier (= 0 for B)
	bool group; // true iff the group bit is set (false for B)
	bool state; // ON - true, OFF - false
};

void setup()
{
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
		ret = now - start > INT_MAX ? INT_MAX : now - start;
	else
		ret = start - now < INT_MIN ? INT_MIN : start - now;
	start = now;
	return ret;
}

/*
 * Return number of bits needed to represent the given number.
 *
 * This is equivalent to floor(log2(v)) + 1.
 */
unsigned int num_bits(unsigned int v)
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
	switch (num_bits(p)) {
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

/*
 * Return a 6-letter string containing the given 3 bytes in hex notation.
 */
const char *three_bytes_in_hex(const byte s[3])
{
	static char buf[7]; // 6 hex digits + NUL
	const char hex[] = "0123456789ABCDEF";
	buf[0] = hex[s[0] >> 4 & B1111];
	buf[1] = hex[s[0] >> 0 & B1111];
	buf[2] = hex[s[1] >> 4 & B1111];
	buf[3] = hex[s[1] >> 0 & B1111];
	buf[4] = hex[s[2] >> 4 & B1111];
	buf[5] = hex[s[2] >> 0 & B1111];
	buf[6] = '\0';
	return buf;
}

/*
 * Transmit the given code on the serial port.
 */
void print_cmd(const struct nexa_cmd & cmd)
{
	Serial.print(cmd.version, HEX);
	Serial.print(':');
	Serial.print(three_bytes_in_hex(cmd.device));
	Serial.print(':');
	Serial.print(cmd.group ? '1' : '0');
	Serial.print(':');
	Serial.print(cmd.channel, HEX);
	Serial.print(':');
	Serial.println(cmd.state ? '1' : '0');
	Serial.flush();
}

/*
 * Add the given bit into the given byte array at the given bit index.
 *
 * The bit index is LSB, so index 0 corresponds to the LSB of
 * dst[dst_len - 1], index 7 corresponds to the MSB of dst[dst_len - 1],
 * index 8 corresponds to the LSB of dst[dst_len - 2], and so on.
 */
void add_bit(byte * dst, size_t dst_len, size_t bit_idx, int bit_val)
{
	// assert(bit_idx < 8 * dst_len);
	size_t byte_idx = dst_len - (1 + bit_idx / 8);
	byte bit_mask = 1 << (bit_idx % 8);
	if (bit_val)
		dst[byte_idx] |= bit_mask;
	else
		dst[byte_idx] &= ~bit_mask;
}

/*
 * Initialize the given old-style 12-bit nexa_cmd from the 12 bits at buf.
 *
 * The command bits are of the form: DDDDDDDD011S
 */
void parse_12bit_cmd(struct nexa_cmd & cmd, const char buf[12])
{
	cmd.version = NEXA_12BIT;
	cmd.device[0] = 0;
	cmd.device[1] = 0;
	for (size_t i = 0; i < 8; ++i)
		add_bit(cmd.device + 2, 1, i, buf[i] == '1');
	cmd.channel = 0;
	cmd.group = 0;
	cmd.state = buf[11] == '1';
}

/*
 * Initialize the given new-style 32-bit nexa_cmd from the 32 bits at buf.
 *
 * The command bits are of the form: DDDDDDDDDDDDDDDDDDDDDDDD10GSCCCC
 */
void parse_32bit_cmd(struct nexa_cmd & cmd, const char buf[32])
{
	size_t i;
	cmd.version = NEXA_32BIT;
	for (i = 0; i < 24; ++i)
		add_bit(cmd.device, ARRAY_LENGTH(cmd.device), i,
			buf[i] == '1');
	cmd.channel = (buf[28] == '1' ? B1000 : 0) |
	              (buf[29] == '1' ? B100 : 0) |
	              (buf[30] == '1' ? B10 : 0) |
	              (buf[31] == '1' ? B1 : 0);
	cmd.group = buf[26] == '1';
	cmd.state = buf[27] == '1';
}

/*
 * Parse data in buf[0..buf_pos] into nexa_cmds sent over the serial port.
 */
void decode_buf()
{
	int state = -1; // Initial state - before SYNC
	enum nexa_cmd_version version = NEXA_INVAL;
	for (size_t i = 0; i < buf_pos; i++) {
		char b = buf[i];
		if (state > 0) { // Expecting another data bit
			if (b == '0' || b == '1')
				--state;
			else
				state = -1; // Revert to initial state
		}

		if (state == 0) { // Finished reading data bits
			struct nexa_cmd cmd;
			if (version == NEXA_12BIT)
				parse_12bit_cmd(cmd, buf + i + 1 - 12);
			else if (version == NEXA_32BIT)
				parse_32bit_cmd(cmd, buf + i + 1 - 32);
			print_cmd(cmd);

			state = -1; // Look for next SYNC
		}
		else if (state == -1) { // Looking for SYNC
			if (b == 'A') { // SYNC for format A
				version = NEXA_32BIT;
				state = 32; // Expect 32 data bits
			}
			else if (b == 'B') { // SYNC for format B
				version = NEXA_12BIT;
				state = 12; // Expect 12 data bits
			}
		}
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
				if (cur_state == SX2) // cmd format B
					buf[buf_pos++] = 'B';
				new_state = DB2;
			}
			else if (cur_state == DB3 && cur_bit == '0') {
				buf[buf_pos++] = cur_bit;
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
				buf[buf_pos++] = cur_bit;
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
					buf[buf_pos++] = 'A';
					new_state = DA0;
					break;
				case DA1:
					new_state = DA2;
					break;
				case DA3:
					buf[buf_pos++] = cur_bit;
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
	if (buf_pos >= BUF_SIZE) // Prevent overflowing buf
		new_state = UNKNOWN;

	if (cur_state != UNKNOWN && new_state == UNKNOWN) { // => UNKNOWN
#if DEBUG
		buf[buf_pos] = '\0';
		Serial.println(buf);
		Serial.flush();
#endif // DEBUG
		// Reached end of valid data: Decode and print buffer.
		// ...but only if it is longer than the shortest command
		// (Format B: SYNC + 12 data bits)
		if (buf_pos > 1 + 12)
			decode_buf();
		buf_pos = 0; // Restart buffer
	}
	cur_state = new_state;
}
