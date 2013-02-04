/*
 * Humidity/temperature sensing node
 *
 * This node measures humidity and temperature using one or more sensors,
 * and transmits the measurements via serial port and RFM12B radio.
 *
 * Hardware-wise, we're using a JeeNode with the following attachments:
 *  - AM2302 (DHT22-compatible) humd/temp sensor connected to P1 as such:
 *     - VCC (red wire) to P (+3.3V - +5V)
 *     - Data (yellow wire) to D (Arduino pin 4, ATmega D4)
 *     - GND (black wire) to G (GND)
 *     - (White wire is N.C.)
 *  - DHT11 humd/temp sensor connected to P2 as such:
 *     - VCC (red wire) to P (+3.3V - +5V)
 *     - GND (black wire) to G (GND)
 *     - S (blue wire) to D (Arduino pin 5, ATmega D5)
 *  - Parallax Sensirion SHT11 Sensor Module connected to P3 as such:
 *     - 1 (Data) to D (Arduino pin 6, ATmega D6)
 *     - 3 (Clock) to A (Arduino pin A2, ATmega C2)
 *     - 4 (Vss) to G (GND)
 *     - 8 (Vdd) to + (+3.3V, module handles +2.4V - +5.5V)
 *
 * Some of this code is based on the example testing sketch for various
 * DHT humidity/temperature sensors, written by ladyada, public domain.
 *
 * Author: Johan Herland <johan@herland.net>
 */

#include <DHT.h> // Needs DHT class
#include <Ports.h> // Needs Port and Sleepy
#include <PortsSHT11.h> // Needs SHT class

const unsigned long PERIOD = 5000; // msecs between readings
// Port P1(1);
// Port P2(2);
// Port P3(3);
// Port P4(4);

DHT dht22(4, DHT22);
DHT dht11(5, DHT11);
SHT11 sht11(3);

// The following is needed by the Sleepy::loseSomeTime()
ISR(WDT_vect) { Sleepy::watchdogEvent(); }

void setup()
{
	Serial.begin(115200);
	Serial.println(F("humd_temp_node: Humidity/temperature sensing node"));

	dht22.begin();
	dht11.begin();
	sht11.enableCRC();
}

void publish_sample(const char *name, long ts, float humd, float temp)
{
	Serial.print(name);
	Serial.print(F(": Time - "));
	Serial.print(ts);
	Serial.print(F(" msecs\tHumidity - "));
	Serial.print(humd);
	Serial.print(F(" %\tTemperature - "));
	Serial.print(temp);
	Serial.println(F(" *C"));
}

void loop()
{
	unsigned long now = millis();

	// Read DHT22 sensor - takes about 250 msecs!
	float h1 = dht22.readHumidity();
	float t1 = dht22.readTemperature();

	// Read DHT11 sensor - takes about 250 msecs!
	float h2 = dht11.readHumidity();
	float t2 = dht11.readTemperature();

	// Read SHT11 sensor
	uint8_t error = sht11.measure(SHT11::HUMI);
	error |= sht11.measure(SHT11::TEMP);
	float h3, t3;
	sht11.calculate(h3, t3);


	if (isnan(h1) || isnan(t1))
		Serial.println(F("Failed to read from DHT22"));
	else
		publish_sample("DHT22", now, h1, t1);

	if (isnan(h2) || isnan(t2))
		Serial.println(F("Failed to read from DHT11"));
	else
		publish_sample("DHT11", now, h2, t2);

	if (error)
		Serial.println(F("Failed to read from SHT11"));
	else
		publish_sample("SHT11", now, h3, t3);

	// Sleep until next reading is due
	Serial.print(millis() - now);
	Serial.println(F("msec elapsed"));
	Serial.flush();
	unsigned long sleep = now + PERIOD - millis();
	if (sleep > 0)
		Sleepy::loseSomeTime(sleep);
}
