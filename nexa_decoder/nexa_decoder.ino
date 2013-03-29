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

#include <Macros.h>
#include <RF433Transceiver.h>
#include <RingBuffer.h>
#include <PulseParser.h>
#include <NexaCommand.h>

// Adjust the following to match where the RF receiver is connected.
RF433Transceiver rf_port(1);
RingBuffer<char> rx_bits(1000);
PulseParser pulse_parser(rx_bits);

void setup()
{
	Serial.begin(115200);
	Serial.println(F("nexa_decoder ready:"));
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
 * Initialize the given old-style NexaCommand from the 12 bits at buf.
 *
 * The command bits are of the form: DDDDDDDD011S
 */
void parse_12bit_cmd(NexaCommand & cmd, const char buf[12])
{
	cmd.version = NexaCommand::NEXA_12BIT;
	cmd.device[0] = 0;
	cmd.device[1] = 0;
	for (size_t i = 0; i < 8; ++i)
		add_bit(cmd.device + 2, 1, i, buf[i] == '1');
	cmd.channel = 0;
	cmd.group = 0;
	cmd.state = buf[11] == '1';
}

/*
 * Initialize the given new-style NexaCommandfrom the 32 bits at buf.
 *
 * The command bits are of the form: DDDDDDDDDDDDDDDDDDDDDDDD10GSCCCC
 */
void parse_32bit_cmd(NexaCommand & cmd, const char buf[32])
{
	size_t i;
	cmd.version = NexaCommand::NEXA_32BIT;
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
 * Parse data from ring buffer, and generate + print NexaCommands.
 */
void decode_bits(RingBuffer<char> & rx_bits)
{
	static NexaCommand::Version version = NexaCommand::NEXA_INVAL;
	static char buf[32]; // Long enough for the longest command
	static size_t buf_pos = 0;
	static size_t expect = 0;
	while (!rx_bits.r_empty()) {
		char b = rx_bits.r_pop();
		if (b == 'A' || b == 'B') {
			buf_pos = 0;
			if (b == 'A') {
				version = NexaCommand::NEXA_32BIT;
				expect = 32;
			}
			else {
				version = NexaCommand::NEXA_12BIT;
				expect = 12;
			}
		}
		else if ((b == '0' || b == '1') && buf_pos < expect)
			buf[buf_pos++] = b;

		if (expect && buf_pos == expect) { // all bits present
			NexaCommand cmd;
			if (version == NexaCommand::NEXA_12BIT)
				parse_12bit_cmd(cmd, buf);
			else if (version == NexaCommand::NEXA_32BIT)
				parse_32bit_cmd(cmd, buf);
#if DEBUG
			Serial.println();
#endif
			cmd.print(Serial);
			Serial.flush();

			expect = 0;
			buf_pos = 0;
			version = NexaCommand::NEXA_INVAL;
		}
	}
}

void loop()
{
	pulse_parser(rf_port.rx_get_pulse());
	if (!rx_bits.r_empty()) {
		size_t len = rx_bits.r_buf_len();
		do {
#if DEBUG
			Serial.write((const byte *) rx_bits.r_buf(), len);
#endif
			decode_bits(rx_bits);
		} while (len = rx_bits.r_buf_len());
	}
}
