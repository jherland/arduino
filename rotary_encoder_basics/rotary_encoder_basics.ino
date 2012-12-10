//these pins can not be changed 2/3 are special pins
int pin1 = 2;
int pin2 = 3;

volatile byte ring_buffer[256] = { 0 };
volatile byte rb_write; // current write position in ring buffer
byte rb_read; // current read position in ringbuffer

byte pin_state = 0;
int encoder_value = 0;

void setup()
{
  Serial.begin(115200);

  pinMode(pin1, INPUT); 
  pinMode(pin2, INPUT);

  digitalWrite(pin1, HIGH); //turn pullup resistor on
  digitalWrite(pin2, HIGH); //turn pullup resistor on

  Serial.println("Ready");

  //call updateEncoder() when any high/low changed seen
  //on interrupt 0 (pin 2), or interrupt 1 (pin 3) 
  attachInterrupt(0, updateEncoder, CHANGE); 
  attachInterrupt(1, updateEncoder, CHANGE);
}

void updateEncoder()
{
  ring_buffer[rb_write++] = PIND;
}

void loop()
{
  if (rb_read == rb_write)
    return; // Nothing has been added to the ring buffer

  // Process the next value in the ring buffer
  byte value = (ring_buffer[rb_read] >> 2) & 0b11;
  // Did the value actually change since last reading?
  if (value != (pin_state & 0b11)) {
    // Append to history of pin states
    pin_state = (pin_state << 2) | value;
    // Are we in a "rest" state?
    if (value == 0b11) {
      // Figure out how we got here
      switch (pin_state & 0b111111) {
      case 0b000111:
        // CCW
        encoder_value--;
        Serial.print("<- ");
        Serial.println(encoder_value);
        break;
      case 0b001011:
        // CW
        encoder_value++;
        Serial.print("-> ");
        Serial.println(encoder_value);
        break;
      }
    }
  }

  rb_read++;
}

