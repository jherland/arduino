#include <Ports.h>
#include <RF12.h>

void setup(void)
{
	// Wait 5 seconds to allow measuring "regular" power use
	delay(5000);

	// Go into low-power sleep
	Sleepy::powerDown();
}

void loop(void)
{
}
