/*
 * Encoder sketch for 433 MHz transmitter module sending Nexa commands.
 *
 * Objective:
 *
 * Read Nexa commands from the serial port, and transmit them to the
 * digital input from a WLS107B4B or similar 433 MHz transmitter.
 *
 * Connections:
 *
 *   -------------
 *  |   433 MHz   |
 *  | Transmitter |
 *   -------------
 *      | | | |
 *      1 2 3 4
 *
 * 1: VCC (5V)
 * 2: GND
 * 2: Digital input (connect to PD4 (Arduino pin #4))
 * 8: Optional Antenna (173mm wire)
 *
 * Author: Johan Herland <johan@herland.net>
 * License: GNU GPL v2 or later
 */

#include <limits.h>

#define DEBUG 0

#define ARRAY_LENGTH(a) ((sizeof (a)) / (sizeof (a)[0]))

// Adjust the following to match where the RF transmitter is connected.
#define RF_TX_SETUP() bitSet(DDRC, 4)
#define RF_TX_WRITE(b) bitWrite(PORTD, 4, (b))


void setup()
{
	RF_TX_SETUP();
	Serial.begin(115200);
	Serial.println(F("nexa_encoder ready:"));
}

class NexaCommand {
public: // types & constants
	enum Version {
		NEXA_INVAL, // Unknown/invalid version
		NEXA_12BIT, // Old 12-bit command format: DDDDDDDD011S
		NEXA_32BIT, // New 32-bit command format: D{24}10GSCCCC
		NEXA_END // End sentinel
	};

	// length of command string on format "V:DDDDDD:G:C:S"
	static const size_t cmd_str_len = 14;

public: // initializers
	/*
	 * Initialize NexaCommand instance from  incoming command string
	 * of the form "V:DDDDDD:G:C:S", where
	 *  - V = Nexa command version (hex digit)
	 *  - DDDDDD = 24-bit device id in hex
	 *  - G = group bit (0/1)
	 *  - C = channel in hex (0-F)
	 *  - S = state bit (0/1 == off/on)
	 */
	static bool from_cmd_str(NexaCommand & cmd, const char * buf,
	                         size_t len);

public: // queries
	// Print this Nexa command on the serial port.
	void print(Print & out) const;

	/*
	 * Transmit this Nexa command on the RF transmitter.
	 *
	 * Return true on success, false otherwise.
	 */
	bool transmit() const;

private: // helpers
	void transmit_12bit(size_t repeats) const;
	void transmit_32bit(size_t repeats) const;

public: // representation
	Version version;
	byte device[3]; // 24-bit device id (NEXA_12BIT only uses last 8)
	byte channel; // 4-bit channel id (always 0 for NEXA_12BIT)
	bool group; // group bit (always false for NEXA_12BIT)
	bool state; // ON - true, OFF - false
};

class HexUtils {
public: // utils
	// return 0 - 15 for given char '0' - 'F'/'f'; otherwise return -1
	static int parse_digit(char c);

	// convert 2 hex digits into 0 - 255; return -1 on failure
	static int parse_byte(char h, char l);

	/*
	 * Convert hex string into corresponding byte string.
	 *
	 * The "len" * 2 first hex digits in "src" are parsed and their
	 * byte values stored into the first "len" bytes of "dst".
	 *
	 * Return true on success, or false if any non-hex digits are
	 * found (in which case the contents of target is undefined).
	 */
	static bool hex2bytes(byte * dst, const char * src, size_t len);

	/*
	 * Convert byte string into corresponding hex string.
	 *
	 * The first "len" bytes of "src" and converted into hex, and
	 * written into "dst", which must have room for at least "len" * 2
	 * characters.
	 */
	static void bytes2hex(char * dst, const byte * src, size_t len);
};

int HexUtils::parse_digit(char c)
{
	switch(c) {
		case '0':
		case '1':
		case '2':
		case '3':
		case '4':
		case '5':
		case '6':
		case '7':
		case '8':
		case '9':
			return c - '0';
		case 'a':
		case 'b':
		case 'c':
		case 'd':
		case 'e':
		case 'f':
			return 10 + c - 'a';
		case 'A':
		case 'B':
		case 'C':
		case 'D':
		case 'E':
		case 'F':
			return 10 + c - 'A';
		default:
			return -1;
	}
}

int HexUtils::parse_byte(char h, char l)
{
	int ih = parse_digit(h);
	int il = parse_digit(l);
	if (ih == -1 || il == -1)
		return -1;
	return ih << 4 | il;
}

bool HexUtils::hex2bytes(byte * dst, const char * src, size_t len)
{
	for (size_t i = 0; i < len; ++i) {
		int b = parse_byte(src[i * 2], src[i * 2 + 1]);
		if (b == -1)
			return false;
		dst[i] = b;
	}
	return true;
}

void HexUtils::bytes2hex(char * dst, const byte * src, size_t len)
{
	static const char hex[] = "0123456789ABCDEF";
	for (size_t i = 0; i < len; ++i) {
		dst[i * 2] = hex[src[i] >> 4 & B1111];
		dst[i * 2 + 1] = hex[src[i] & B1111];
	}
}

bool NexaCommand::from_cmd_str(NexaCommand & cmd, const char * buf,
			       size_t len)
{
	if (len != cmd_str_len)
		return false;

	if (buf[1] != ':' || buf[8] != ':' || buf[10] != ':' ||
	    buf[12] != ':')
		return false;

	int v = HexUtils::parse_digit(buf[0]);
	bool d = HexUtils::hex2bytes(cmd.device, buf + 2, 3);
	int g = HexUtils::parse_digit(buf[9]);
	int c = HexUtils::parse_digit(buf[11]);
	int s = HexUtils::parse_digit(buf[13]);

	if ((v <= NEXA_INVAL && v >= NEXA_END) || !d ||
	    (g != 1 && g != 0) || c == -1 || (s != 1 && s != 0))
		return false;

	cmd.version = (Version) v;
	cmd.channel = c;
	cmd.group = g;
	cmd.state = s;
	return true;
}

void NexaCommand::print(Print & out) const
{
	const size_t device_bytes = 3;
	byte device_id[device_bytes * 2];
	HexUtils::bytes2hex((char *) device_id, device, device_bytes);
	out.print(version, HEX);
	out.print(':');
	out.write(device_id, ARRAY_LENGTH(device_id));
	out.print(':');
	out.print(group ? '1' : '0');
	out.print(':');
	out.print(channel, HEX);
	out.print(':');
	out.println(state ? '1' : '0');
}

bool NexaCommand::transmit() const
{
	switch(version) {
		case NEXA_12BIT:
			transmit_12bit(5);
			break;
		case NEXA_32BIT:
			transmit_32bit(5);
			break;
		default:
			Serial.print(F("Unknown Nexa command version "));
			Serial.println(version);
			return false;
	}

#if DEBUG
	Serial.print(F("Transmitted code: "));
	print(Serial);
	Serial.flush();
#endif

	return true;
}

void NexaCommand::transmit_12bit(size_t repeats) const
{
	/*
	 * SYNC: SHORT (0.35ms) HIGH, XXLONG (10.9ms) LOW
	 * '0' bit: SHORT HIGH, LONG (1.05ms) LOW, SHORT HIGH, LONG LOW
	 * '1' bit: SHORT HIGH, LONG LOW, LONG HIGH, SHORT LOW
	 */
	enum PulseLength { // in µs
		SHORT = 350,
		LONG = 3 * 350,
		XXLONG = 31 * 350,
	};

	int bits[12]; // DDDDDDDD011S
	// The 8 device bits are device[2] LSB first
	for (size_t i = 0; i < 8; ++i)
		bits[i] = device[2] >> i & 1;
	bits[8] = 0;
	bits[9] = 1;
	bits[10] = 1;
	bits[11] = state;

	for (size_t i = 0; i < repeats; ++i) {
		// SYNC
		RF_TX_WRITE(HIGH);
		delayMicroseconds(SHORT);
		RF_TX_WRITE(LOW);
		delayMicroseconds(XXLONG);

		// data bits
		for (size_t i = 0; i < ARRAY_LENGTH(bits); ++i) {
			if (bits[i]) { // '1'
				RF_TX_WRITE(HIGH);
				delayMicroseconds(SHORT);
				RF_TX_WRITE(LOW);
				delayMicroseconds(LONG);
				RF_TX_WRITE(HIGH);
				delayMicroseconds(LONG);
				RF_TX_WRITE(LOW);
				delayMicroseconds(SHORT);
			}
			else { // '0'
				RF_TX_WRITE(HIGH);
				delayMicroseconds(SHORT);
				RF_TX_WRITE(LOW);
				delayMicroseconds(LONG);
				RF_TX_WRITE(HIGH);
				delayMicroseconds(SHORT);
				RF_TX_WRITE(LOW);
				delayMicroseconds(LONG);
			}
		}
	}
	RF_TX_WRITE(HIGH);
	delayMicroseconds(SHORT);
	RF_TX_WRITE(LOW);
}

void NexaCommand::transmit_32bit(size_t repeats) const
{
	/*
	 * SYNC: XXLONG (10.15ms) LOW, SHORT (0.31ms) HIGH,
	 *       XLONG (2.64ms) LOW, SHORT HIGH
	 * '0' bit: XSHORT (0.22ms) LOW, SHORT HIGH,
	 *          LONG (1.24ms) LOW, SHORT HIGH
	 * '1' bit: LONG LOW, SHORT HIGH,
	 *          XSHORT LOW, SHORT HIGH
	 */
	enum PulseLength { // in µs
		XSHORT = 215,
		SHORT = 310,
		LONG = 1236,
		XLONG = 2643,
		XXLONG = 10150,
	};

	int bits[32]; // DDDDDDDDDDDDDDDDDDDDDDDD10GSCCCC
	// The 24 device bits are device[2..0] LSB first
	for (size_t i = 0; i < 24; ++i)
		bits[i] = device[2 - i / 8] >> (i % 8) & 1;
	bits[24] = 1;
	bits[25] = 0;
	bits[26] = group;
	bits[27] = state;
	bits[28] = channel & B1000;
	bits[29] = channel & B100;
	bits[30] = channel & B10;
	bits[31] = channel & B1;

	for (size_t i = 0; i < repeats; ++i) {
		// SYNC
		RF_TX_WRITE(LOW);
		delayMicroseconds(XXLONG);
		RF_TX_WRITE(HIGH);
		delayMicroseconds(SHORT);
		RF_TX_WRITE(LOW);
		delayMicroseconds(XLONG);
		RF_TX_WRITE(HIGH);
		delayMicroseconds(SHORT);

		// data bits
		for (size_t i = 0; i < ARRAY_LENGTH(bits); ++i) {
			if (bits[i]) { // '1'
				RF_TX_WRITE(LOW);
				delayMicroseconds(LONG);
				RF_TX_WRITE(HIGH);
				delayMicroseconds(SHORT);
				RF_TX_WRITE(LOW);
				delayMicroseconds(XSHORT);
				RF_TX_WRITE(HIGH);
				delayMicroseconds(SHORT);
			}
			else { // '0'
				RF_TX_WRITE(LOW);
				delayMicroseconds(XSHORT);
				RF_TX_WRITE(HIGH);
				delayMicroseconds(SHORT);
				RF_TX_WRITE(LOW);
				delayMicroseconds(LONG);
				RF_TX_WRITE(HIGH);
				delayMicroseconds(SHORT);
			}
		}
	}
	RF_TX_WRITE(LOW);
}

void loop()
{
	// Wait until a full command is availalbe on serial input
	if (Serial.available() < NexaCommand::cmd_str_len)
		return;

	char cmd_buf[NexaCommand::cmd_str_len];
	size_t buf_read = Serial.readBytesUntil(
		'\n', cmd_buf, NexaCommand::cmd_str_len);

#if DEBUG
	Serial.print("Read ");
	Serial.print(buf_read);
	Serial.print(" cmd_buf: ");
	Serial.write((const byte *) cmd_buf, buf_read);
	Serial.println();
#endif

	NexaCommand cmd;
	if (NexaCommand::from_cmd_str(cmd, cmd_buf, buf_read))
		cmd.transmit();
}
