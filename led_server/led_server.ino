/*
 * Server for controlling LEDs based on incoming commands from RCC
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

const byte VERSION = 1;

const byte rfm12b_band = RF12_868MHZ;
const byte rfm12b_group = 123;
const byte rfm12b_local_id = 1;

byte rot_values[3] = { 0 }; // R/G/B channel values
byte cur_channel = 0; // Index into above array

const byte pwm_pins[3] = {5, 6, 9};

// Format of rfm12b data packets used to send commands and status updates:
struct rc_data {
	uint8_t write    : 1; // Write (=1) or Read (=0) command.
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
 * RFM12B data traffic to/from this server:
 *
 * - Status request: From a rotary remote control (RRC) to this node, to
 *   request current level of a given channel.
 *   Packet data: {0, 0, $channel, 0}
 * - Status update: Broadcast from this node, to inform of current level
 *   of a given channel. Packet data: {0, 0, $channel, $abs_value}
 * - Write update: From an RRC to this node, to adjust the current level
 *   of a given channel. Packet data: {1, 1, $channel, $rel_value}
 *                       OR           {1, 0, $channel, $abs_value}
 */

struct rc_packet out_buf[16]; // Current data packet being sent on RFM12B
byte ob_write; // Current write position in out_buf ringbuffer
byte ob_read; // Current read position in out_buf ringbuffer

void setup(void)
{
	Serial.begin(115200);

	// Set up RFM12B communication
	rf12_initialize(rfm12b_local_id, rfm12b_band, rfm12b_group);

	// Initialize R/G/B LEDs to all black
	for (int i = 0; i < ARRAY_LENGTH(pwm_pins); i++) {
		analogWrite(pwm_pins[i], 0);
		status_update(next_packet(), i, 0);
	}

	Serial.print(F("LED server version "));
	Serial.println(VERSION);
	Serial.print(F("Using RFM12B group "));
	Serial.print(rfm12b_group);
	Serial.print(F(" with node id "));
	Serial.print(rfm12b_local_id);
	Serial.print(F(" @ "));
	Serial.print(rfm12b_band == RF12_868MHZ ? 868
		  : (rfm12b_band == RF12_433MHZ ? 433 : 915));
	Serial.println(F("MHz"));
	Serial.println(F("Ready"));
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

			if (rf12_len == 2) {
				struct rc_data * d = (struct rc_data *) rf12_data;
				if (d->write && d->relative)
					handle_update(d->channel, d->rel_value);
			}
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

void status_update(struct rc_packet * packet, byte channel, byte value)
{
	if (channel >= ARRAY_LENGTH(pwm_pins)) {
		Serial.print(F("Illegal channel number: "));
		Serial.println(channel);
		return;
	}

	// Regular broadcast packet
	packet->hdr = RF12_HDR_MASK & rfm12b_local_id;
	packet->d.write = 0;
	packet->d.relative = 0;
	packet->d.channel = channel;
	packet->d.abs_value = value;
}

void handle_update(byte channel, int8_t adjust)
{
	if (channel >= ARRAY_LENGTH(pwm_pins)) {
		Serial.print(F("Illegal channel number: "));
		Serial.println(channel);
		return;
	}

	// Adjust current channel, but limit to 0 <= level <= 255
	int level = LIMIT(0x00, rot_values[channel] + adjust, 0xff);
	rot_values[channel] = level;
	Serial.print(F("Adjusted channel #"));
	Serial.print(channel);
	Serial.print(F(" with "));
	Serial.print(adjust);
	Serial.print(F(" to "));
	Serial.println(level);

	// Send status update as reply
	status_update(next_packet(), channel, level);
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

	// Refresh R/G/B LEDs
	for (int i = 0; i < ARRAY_LENGTH(pwm_pins); i++)
		analogWrite(pwm_pins[i], rot_values[i]);
}
