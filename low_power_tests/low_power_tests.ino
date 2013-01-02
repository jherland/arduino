#include <Ports.h>
#include <RF12.h>

void setup(void)
{
	// Wait 5 seconds to allow measuring "regular" power use
	delay(5000);

	// Put RFM12B radio to sleep, to minimize its power draw
	rf12_initialize(1, RF12_868MHZ);
	rf12_sleep(RF12_SLEEP);

	// Go into low-power sleep
	Sleepy::powerDown();
}

void loop(void)
{
}
