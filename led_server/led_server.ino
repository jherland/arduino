/*
 * Server for controlling PWM and non-PWN pins connected through RCN
 *
 * Author: Johan Herland <johan@herland.net>
 * License: GNU GPL v2 or later
 */

// #define DEBUG 1

#if DEBUG
#define LOG Serial.print
#define LOGln Serial.println
#else
#define LOG(...)
#define LOGln(...)
#endif

#define RCN_HOST_MAX_CHANNELS 7

#include <RF12.h> // Needed by RCN
#include <rcn_host.h>

#define ARRAY_LENGTH(a) ((sizeof (a)) / (sizeof (a)[0]))

const byte VERSION = 4;
const byte   PINS[RCN_HOST_MAX_CHANNELS] = { 3,  4,  5,   6,  7, 8,  9 };
const byte RANGES[RCN_HOST_MAX_CHANNELS] = {255, 1, 255, 255, 1, 1, 255};

// The following function is called whenever a channel is being updated.
uint8_t update_filter(
	uint8_t channel, // The channel id
	uint8_t range, // The registered range for this channel
	uint8_t data, // The auxiliary data for this channel
	uint8_t old_level, // The old/current level
	uint8_t new_level) // The proposed new level
{
	// Adjust pin number 'data' from 'old_level' to 'new_level'
	if (range == 1) // on/off pin
		digitalWrite(data, new_level);
	else // PWM pin
		analogWrite(data, new_level);
	return new_level;
}

RCN_Host host(RF12_868MHZ, 123, 1, update_filter);

void setup(void)
{
#if DEBUG
	Serial.begin(115200);
	Serial.print(F("LED server version "));
	Serial.println(VERSION);
#endif
	host.init();

	// Initialize one channel for each pin.
	for (int i = 0; i < ARRAY_LENGTH(PINS); i++) {
		pinMode(PINS[i], OUTPUT);
		host.addChannel(RANGES[i], 0, PINS[i]);
	}

#if DEBUG
	Serial.println(F("Ready"));
#endif
}

void loop(void)
{
	host.run();
}
