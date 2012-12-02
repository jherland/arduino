/*
 * PWM_LED_Control
 * A simple combination of the AnalogReadSerial and Fade Arduino examples.
 *
 * Reads an analog input on pin 0, prints the result to the serial monitor,
 * and PWM the LED on pin 9 using the analogWrite() function.
 *
 * Attach the center pin of a potentiometer to pin A0, and the outside pins to
 * +5V and ground, and connect an LED to pin 9.
 *
 * This example code is in the public domain.
 */

const int pot_pin = 0;
const int led_pin = 9;

void setup() {
	Serial.begin(9600);
	Serial.println("Ready.");
}

void loop() {
	int value = analogRead(pot_pin); // 0 <= N < 1024
	analogWrite(led_pin, value / 4); // 0 <= N < 255
	Serial.println(value);
	delay(100);
}
