/*
 *  Switch and RGB LED test program
 *
 * Wiring
 * ------
 * Connect a switch and an RGB LED to the Arduino as follows:
 * 1. Connect one side of the switch to ground.
 * 2. Connect the other side of the switch to Arduino digital pin 2
 *    through a 100Ω resistor, and to +5V through a 10kΩ pull-up resistor.
 * 3. Connect the longest pin (pin 2) of RGB LED to ground (0V).
 * 4. Connect the other pins of the RGB LED as follows:
 *     - LED pin 1 (red) through 1kΩ to Arduino digital pin 11 (PWM)
 *     - LED pin 3 (green) through 3.3kΩ to Arduino digital pin 10 (PWM)
 *     - LED pin 4 (blue) through 1kΩ to Arduino digital pin 9 (PWM)
 *
 * Behavior
 * --------
 * 0. Initially, the LED will be dark.
 * 1. Press the switch once to start repeatedly fading the red component
 *    up and down.
 * 2. Press the switch again to lock the red component, and the green
 *    component will start to fade up and down.
 * 3. Press the switch to lock the green component, and the blue component
 *    will start to fade up and down.
 * 4. Press the switch to lock the blue component. The LED will now
 *    display the three locked components.
 * 5. Press again to return to step #1.
 *
 * Johan Herland <johan@herland.net>
 */

const int switchPin = 2;
const int numLeds = 3;
const int ledPin[numLeds] = {11, 10, 9};

const int numstates = numLeds + 1;

int val, pval;
unsigned long bounceTimeout;
int state;
byte lumi[numLeds];
boolean up;


void setup()
{
	pinMode(switchPin, INPUT);
	for (int i = 0; i < numLeds; i++) {
		pinMode(ledPin[i], OUTPUT);
		lumi[i] = 0;
	}
	bounceTimeout = 0;
	state = numstates - 1;
	up = true;
}


void loop()
{
	val = digitalRead(switchPin);
	if (val != pval && millis() > bounceTimeout) {
		if (val == LOW)
			state = (state + 1) % numstates;
		pval = val;
		bounceTimeout = millis() + 100; // set new bounce timeout
	}

	if (up && state < numLeds) {
		lumi[state] += 5;
		if (lumi[state] == 255)
			up = false;
	}
	else if (state < numLeds) {
		lumi[state] -= 5;
		if (lumi[state] == 0)
			up = true;
	}

	for (int i = 0; i < numLeds; i++)
		analogWrite(ledPin[i], lumi[i]);

	delay(20);
}
