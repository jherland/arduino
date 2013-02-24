/*
 * Test RFM12B connectivity by broadcasting ping packets every second and
 * expecting ACKs.
 *
 * The number of packets sent and ACKed are reported on a connected LCD
 * display, and also on the serial port.
 *
 * Author: Johan Herland <johan@herland.net>
 * License: GNU GPL v2 or later
 */

#include <RF12.h>
#include <PortsLCD.h>

#define SKETCH_ID "rf12ping v1"

#define SERIAL_REPORT // Enable serial communication
#define LCD_PORT 1    // LCD display hooked up to JeeNode port P1
#define BATTERY_PIN 4 // Battery + hooked up to pin A4

const unsigned long ping_interval = 1000; // msecs

byte node_id, send_hdr, recv_hdr;

struct packet {
	unsigned long num_sent;
	unsigned long num_acked;
} p;

unsigned long last_send;

#ifdef LCD_PORT
PortI2C lcdp(LCD_PORT);
LiquidCrystalI2C lcd(lcdp);
#endif

#ifdef BATTERY_PIN
/*
 * Return battery level in mV
 *
 * We assume that AREF is at 3.30V, and GND is at 0V, and that scaling the
 * value read from the BATTERY_PIN will provide a fairly accurate reading
 * of the current battery voltage.
 */
float batteryLevel()
{
	// Average 3 readings to improve stability
	int v = analogRead(BATTERY_PIN) +
		analogRead(BATTERY_PIN) +
		analogRead(BATTERY_PIN);
	return v * 3.30 / (1024.0 * 3);
}
#endif

void printLine1(Print& out)
{
	out.print(SKETCH_ID);
#ifdef BATTERY_PIN
	out.print(' ');
	out.print(batteryLevel(), 1);
	out.print('V');
#endif
}

void printLine2(Print& out)
{
	out.print('>');
	out.print(p.num_sent);
	out.print('<');
	out.print(p.num_acked);
	out.print('x');
	out.print(p.num_sent - p.num_acked);
	out.print(' ');
	out.print(p.num_acked * 100.0 / p.num_sent, 0);
	out.print("%  ");
}

void setup()
{
#ifdef SERIAL_REPORT
	Serial.begin(115200);
	Serial.println(SKETCH_ID);
	node_id = rf12_config(1);
#else
	node_id = rf12_config(0);
#endif
#ifdef LCD_PORT
	lcd.begin(16, 2);
	lcd.print(SKETCH_ID);
#endif

	send_hdr = RF12_HDR_ACK | node_id;
	recv_hdr = RF12_HDR_DST | RF12_HDR_CTL | node_id;
}

void loop()
{
	if (rf12_recvDone() && rf12_crc == 0 && rf12_hdr == recv_hdr)
		p.num_acked++;

	unsigned long now = millis();
	if (now >= last_send + ping_interval && rf12_canSend()) {
		last_send = now;
#ifdef SERIAL_REPORT
		printLine2(Serial);
		Serial.println();
#endif
#ifdef LCD_PORT
		lcd.setCursor(0, 0);
		printLine1(lcd);
		lcd.setCursor(0, 1);
		printLine2(lcd);
#endif
		rf12_sendStart(send_hdr, &p, sizeof(p));
		p.num_sent++;
	}
}
