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
NexaCommand cmd;

void setup()
{
	Serial.begin(115200);
	Serial.println(F("nexa_decoder ready:"));
}

void loop()
{
	pulse_parser(rf_port.rx_get_pulse());
	if (!rx_bits.r_empty()) {
#if DEBUG
		Serial.write((const byte *) rx_bits.r_buf(),
			     rx_bits.r_buf_len());
		Serial.write((const byte *) rx_bits.r_wrapped_buf(),
			     rx_bits.r_wrapped_buf_len());
#endif
		if (NexaCommand::from_bit_buffer(cmd, rx_bits)) {
#if DEBUG
			Serial.println();
#endif
			cmd.print(Serial);
		}
	}
}
