/*
 * Encoder sketch for 433 MHz transmitter module sending Nexa commands.
 *
 * Read Nexa commands from the serial port, and transmit to the OOK 433
 * Plug. See RF433Transceiver.h for more info on the physical setup.
 *
 * Author: Johan Herland <johan@herland.net>
 * License: GNU GPL v2 or later
 */

#include <RF433Transceiver.h>
#include <NexaCommand.h>

// Adjust the following to match where the RF transmitter is connected.
RF433Transceiver rf_port(1);

void setup()
{
	Serial.begin(115200);
	Serial.println(F("nexa_encoder ready:"));
}

void loop()
{
	// Wait until a full command is available on serial input
	if (Serial.available() < NexaCommand::cmd_str_len)
		return;

	char cmd_buf[NexaCommand::cmd_str_len];
	size_t buf_read = Serial.readBytesUntil(
		'\n', cmd_buf, NexaCommand::cmd_str_len);

#if DEBUG
	Serial.print("Read ");
	Serial.print(buf_read);
	Serial.print(" bytes: ");
	Serial.write((const byte *) cmd_buf, buf_read);
	Serial.println();
#endif

	NexaCommand cmd;
	if (NexaCommand::from_cmd_str(cmd, cmd_buf, buf_read)) {
		for (size_t i = 0; i < 5; i++) {
			cmd.transmit(rf_port);
			Serial.print("TX -> ");
			cmd.print(Serial);
		}
	}
}
