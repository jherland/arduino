/*
 * Test RFM12B connectivity by serving ACKs to incoming ping packets, and
 * providing status information on the serial port.
 *
 * Author: Johan Herland <johan@herland.net>
 * License: GNU GPL v2 or later
 */

#include <RF12.h>

#define SKETCH_ID "rf12pong v1"

struct packet {
	unsigned long num_sent;
	unsigned long num_acked;
} p;

void setup()
{
	Serial.begin(115200);
	Serial.println(SKETCH_ID);
	rf12_config(1);
}

void loop()
{
	if (rf12_recvDone() && rf12_crc == 0 &&
	    rf12_len == sizeof(p) && RF12_WANTS_ACK) {
		p = *((struct packet *)rf12_data);
		byte node_id = rf12_hdr & RF12_HDR_MASK;
		rf12_sendStart(RF12_ACK_REPLY, 0, 0);

		Serial.print(millis() / 1000.0, 3);
		Serial.print(", from node ");
		Serial.print(node_id);
		Serial.print(": ");
		Serial.print(p.num_sent);
		Serial.print(" sent, ");
		Serial.print(p.num_acked);
		Serial.print(" acked, ");
		Serial.print(p.num_sent - p.num_acked);
		Serial.print(" lost, ");
		Serial.print(p.num_acked * 100.0 / p.num_sent, 2);
		Serial.println("% success.");
	}
}
