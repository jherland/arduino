/*
 * Server for controlling 3 LEDs through RCN
 *
 * Author: Johan Herland <johan@herland.net>
 * License: GNU GPL v2 or later
 */

// #define DEBUG 1

#include <RF12.h> // Needed by rcn_common.h
#include <rcn_common.h>

// Utility macros
#define ARRAY_LENGTH(a) ((sizeof (a)) / (sizeof (a)[0]))
#define LIMIT(min, val, max) (min > val ? min : (max < val) ? max : val)

const byte VERSION = 2;
const struct rfm12b_config config = { 1, RF12_868MHZ, 123 };
const byte pwm_pins[3] = {5, 6, 9};

byte rot_values[3] = { 0 }; // R/G/B channel values

void setup(void)
{
	Serial.begin(115200);
	Serial.print(F("LED server version "));
	Serial.println(VERSION);

	rcn_init(&config, Serial);

	// Initialize R/G/B LEDs to all black
	for (int i = 0; i < ARRAY_LENGTH(pwm_pins); i++) {
		analogWrite(pwm_pins[i], 0);
		rcn_send_status_update(i, 0);
	}

	Serial.println(F("Ready"));
}

void handle_update_request(const struct rcn_payload *recvd)
{
	if (recvd->channel >= ARRAY_LENGTH(pwm_pins)) {
		Serial.print(F("Illegal channel number: "));
		Serial.println(recvd->channel);
		return;
	}

	if (recvd->relative && recvd->rel_level) {
		// Adjust current channel, but limit to 0 <= level <= 255
		int l = LIMIT(0x00, rot_values[recvd->channel] + recvd->rel_level, 0xff);
#ifdef DEBUG
		Serial.print(F("Adjusting channel #"));
		Serial.print(recvd->channel);
		Serial.print(F(": "));
		Serial.print(rot_values[recvd->channel]);
		Serial.print(F(" + "));
		Serial.print(recvd->rel_level);
		Serial.print(F(" =>"));
		Serial.println(l);
#endif
		rot_values[recvd->channel] = l;
	}
	else if (recvd->relative) { // && recvd->rel_level == 0
#ifdef DEBUG
		Serial.print(F("Status request for channel #"));
		Serial.println(recvd->channel);
#endif
	}
	else {
		int l = LIMIT(0x00, recvd->abs_level, 0xff);
#ifdef DEBUG
		Serial.print(F("Setting channel #"));
		Serial.print(recvd->channel);
		Serial.print(F(": "));
		Serial.print(rot_values[recvd->channel]);
		Serial.print(F(" -> "));
		Serial.print(recvd->abs_level);
		Serial.print(F(" =>"));
		Serial.println(l);
#endif
		rot_values[recvd->channel] = l;
	}

	// Send status update as reply
	rcn_send_status_update(recvd->channel, rot_values[recvd->channel]);
}

void loop(void)
{
	const struct rcn_payload *recvd = rcn_send_and_recv();
	if (recvd)
		handle_update_request(recvd);

	// Refresh R/G/B LEDs
	for (int i = 0; i < ARRAY_LENGTH(pwm_pins); i++)
		analogWrite(pwm_pins[i], rot_values[i]);
}
