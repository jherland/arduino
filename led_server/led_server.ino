/*
 * Server for controlling 3 LEDs through RCN
 *
 * Author: Johan Herland <johan@herland.net>
 * License: GNU GPL v2 or later
 */

// #define DEBUG 1
#define LOG Serial.print

#include <RF12.h> // Needed by rcn_common.h
#include <rcn_common.h>

// Utility macros
#define ARRAY_LENGTH(a) ((sizeof (a)) / (sizeof (a)[0]))
#define LIMIT(min, val, max) (min > val ? min : (max < val) ? max : val)

const byte VERSION = 3;
RCN_Node node(RF12_868MHZ, 123, 1);
RCN_Node::RecvPacket recvd;
const byte pwm_pins[3] = {5, 6, 9};

byte rot_values[3] = { 0 }; // R/G/B channel values

void setup(void)
{
	Serial.begin(115200);
	Serial.print(F("LED server version "));
	Serial.println(VERSION);

	node.init();

	// Initialize R/G/B LEDs to all black
	for (int i = 0; i < ARRAY_LENGTH(pwm_pins); i++) {
		analogWrite(pwm_pins[i], 0);
		node.send_status_update(i, 0);
	}

	Serial.println(F("Ready"));
}

void handle_update_request(const RCN_Node::RecvPacket& p)
{
	if (p.channel() >= ARRAY_LENGTH(pwm_pins)) {
		Serial.print(F("Illegal channel number: "));
		Serial.println(p.channel());
		return;
	}

	if (p.relative() && p.rel_level() == 0) {
#ifdef DEBUG
		Serial.print(F("Status request for channel #"));
		Serial.println(p.channel());
#endif
	}
	else if (p.relative()) { // relative adjustment
		// Adjust current channel, but limit to 0 <= level <= 255
		int l = rot_values[p.channel()] + p.rel_level();
		l = LIMIT(0x00, l, 0xff);
#ifdef DEBUG
		Serial.print(F("Adjusting channel #"));
		Serial.print(p.channel());
		Serial.print(F(": "));
		Serial.print(rot_values[p.channel()]);
		Serial.print(F(" + "));
		Serial.print(p.rel_level());
		Serial.print(F(" => "));
		Serial.println(l);
#endif
		rot_values[p.channel()] = l;
	}
	else { // absolute assignment
		int l = LIMIT(0x00, p.abs_level(), 0xff);
#ifdef DEBUG
		Serial.print(F("Setting channel #"));
		Serial.print(p.channel());
		Serial.print(F(": "));
		Serial.print(rot_values[p.channel()]);
		Serial.print(F(" -> "));
		Serial.print(p.abs_level());
		Serial.print(F(" => "));
		Serial.println(l);
#endif
		rot_values[p.channel()] = l;
	}

	// Refresh corresponding LED
	analogWrite(pwm_pins[p.channel()], rot_values[p.channel()]);
	// Send status update as reply
	node.send_status_update(p.channel(), rot_values[p.channel()]);
}

void loop(void)
{
	if (node.send_and_recv(recvd))
		handle_update_request(recvd);
}
