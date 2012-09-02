#define PRINT_BITREG(reg, name, page, bits) { \
	unsigned char value = reg; \
	Serial.println(F("* " #reg " - " name " (p" #page "):")); \
	Serial.println(F("  [ " bits " ]")); \
	Serial.print(F("  0b")); \
	Serial.println(value, BIN); \
	Serial.println(); \
}

#define PRINT_16BREG(reg, name, page) { \
	unsigned short value = reg; \
	Serial.println(F("* " #reg "(H/L) - " name " (p" #page "):")); \
	Serial.print(F("  " #reg "[15:8]: ")); \
	Serial.println(value >> 8, BIN); \
	Serial.print(F("  " #reg "[7:0]:  ")); \
	Serial.println(value & 0xff, BIN); \
	Serial.print(F("  16-bit value: 0x")); \
	Serial.print(value, HEX); \
	Serial.print(F(" / ")); \
	Serial.print(value, DEC); \
	Serial.print(F(" / 0")); \
 	Serial.print(value, OCT); \
	Serial.print(F(" / 0b")); \
	Serial.println(value, BIN); \
	Serial.println(); \
}

#define PUTS(literal_string) \
	Serial.println(F(literal_string))

void print_type_info(void)
{
	PUTS("Type information:");
	PUTS("=================");
	Serial.print(F("boolean has size "));
	Serial.println(sizeof(boolean), DEC);
	Serial.print(F("char has size "));
	Serial.println(sizeof(char), DEC);
	Serial.print(F("short has size "));
	Serial.println(sizeof(short), DEC);
	Serial.print(F("int has size "));
	Serial.println(sizeof(int), DEC);
	Serial.print(F("size_t has size "));
	Serial.println(sizeof(size_t), DEC);
	Serial.print(F("long has size "));
	Serial.println(sizeof(long), DEC);
	Serial.print(F("long long has size "));
	Serial.println(sizeof(long long), DEC);
	Serial.println();
	Serial.println();
}

void print_register_info(void)
{
	PUTS("Register contents:");
	PUTS("==================");
	PUTS("  (Page numbers (p###) refer to ATmega328P datasheet");
	PUTS("   available from http://www.atmel.com/Images/doc8271.pdf)");
	Serial.println();

	PUTS("Misc.:");
	PUTS("------");
	Serial.println();
	PRINT_BITREG(PRR, "Power reduction register", 44,
		"PRTWI PRTIM2 PRTIM0 - PRTIM1 PRSPI PRUSART0 PRADC");

	PUTS("Timer/Counter 1:");
	PUTS("----------------");
	Serial.println();
	PRINT_BITREG(TCCR1A, "Timer/Counter1 Control Register A", 132,
		"COM1A1 COM1A0 COM1B1 COM1B0 - - WGM11 WGM10");
	PRINT_BITREG(TCCR1B, "Timer/Counter1 Control Register B", 134,
		"ICNC1 ICES1 - WGM13 WGM12 CS12 CS11 CS10");
	PRINT_BITREG(TCCR1C, "Timer/Counter1 Control Register C", 135,
		"FOC1A FOC1B - - - - - -");
	PRINT_16BREG(TCNT1, "Timer/Counter1", 135);
	PRINT_16BREG(OCR1A, "Output Compare Register 1 A", 136);
	PRINT_16BREG(OCR1B, "Output Compare Register 1 B", 136);
	PRINT_16BREG(ICR1, "Input Capture Register 1", 136);
	PRINT_BITREG(TIMSK1, "Timer/Counter1 Interrupt Mask Register", 136,
		"- - ICIE1 - - OCIE1B OCIE1A TOIE1");
	PRINT_BITREG(TIFR1, "Timer/Counter1 Interrupt Flag Register", 137,
		"- - ICF1 - - OCF1B OCF1A TOV1");

	Serial.println();
}

void setup(void)
{
	Serial.begin(115200);
	print_type_info();
}

unsigned long t0; // Store µsecs since last interrupt

// Timer1 overflow interrupt handler; triggers when TOV1 is set.
ISR(TIMER1_OVF_vect)
{
	unsigned long t = micros();
	Serial.print(F("TIMER1_OVF: "));
	Serial.println(t - t0);
	t0 = t;
}

// Timer1 Compare Match A interrupt handler; triggers when OCF1A is set.
ISR(TIMER1_COMPA_vect)
{
	unsigned long t = micros();
	Serial.print(F("TIMER1_COMPA: "));
	Serial.println(t - t0);
	t0 = t;
}

// Timer1 Capture Event interrupt handler; triggers when ICF1 is set.
ISR(TIMER1_CAPT_vect)
{
	unsigned long t = micros();
	Serial.print(F("TIMER1_CAPT: "));
	Serial.println(t - t0);
	t0 = t;
}

void loop(void)
{
	print_register_info();

	PUTS("--------------------------- Test #1 ---------------------------");
	PUTS("Testing Timer1 in normal mode with no prescaling and interrupt");
	PUTS("on overflow. For the next 100 msecs, the TIMER1_OVF ISR will");
	PUTS("print usecs between interrupts. With no prescaling, the 16MHz");
	PUTS("system clock should overflow the 16-bit timer every 4096 usecs.");
	PUTS("We expect ~24 of those in the following 100 msecs:");
        Serial.flush();

	cli(); // Disable interrupts while setting up the test
	// WGM13:0 = 0b0000 - Normal mode
	TCCR1A &= ~(_BV(WGM10) | _BV(WGM11));
	TCCR1B &= ~(_BV(WGM12) | _BV(WGM13));
	// CS12:0 = 0b001 - No prescaling
	TCCR1B &= ~(_BV(CS12) | _BV(CS11));
	TCCR1B |= _BV(CS10);
	// TOIE1 = 1 - Enable Timer1 overflow interrupt (when TOV1 is set)
	TIMSK1 |= _BV(TOIE1);
	// TOV1 = 1 - Clear Timer1 overflow flag
	TIFR1 |= _BV(TOV1);
	// TCNT1 = 0 - Reset Timer1
	TCNT1 = 0;
	t0 = micros(); // Set start time
	sei(); // Re-enable interrupts

	delay(100); // Let it run for 100 msecs (should yield ~24 interrupts)

	// TOIE1 = 0 - Disable Timer1 overflow interrupt
	TIMSK1 &= ~_BV(TOIE1);
	Serial.println();

	delay(1000);


	PUTS("--------------------------- Test #2 ---------------------------");
	PUTS("Testing Timer1 in CTC mode with no prescaling and interrupt on");
	PUTS("OCR1A match (OCR1A = 31999). For the next 50 msecs, the");
	PUTS("TIMER1_COMPA ISR will print usecs between interrupts. With no");
	PUTS("prescaling, and 16MHz system clock, OCR1A should match TCNT1");
	PUTS("(and thus trigger the TIMER1_COMPA interrupt) every 2000 usecs.");
	PUTS("We expect ~25 of those in the following 50 msecs:");
        Serial.flush();

	cli(); // Disable interrupts while setting up the test
	// WGM13:0 = 0b0100 - CTC mode, TOP = OCR1A
	TCCR1A &= ~(_BV(WGM10) | _BV(WGM11));
	TCCR1B &= ~_BV(WGM13);
	TCCR1B |= _BV(WGM12);
	// OCR1A = 31999
	OCR1A = 31999;
	// CS12:0 = 0b001 - No prescaling
	TCCR1B &= ~(_BV(CS12) | _BV(CS11));
	TCCR1B |= _BV(CS10);
	// OCIE1A = 1 - Enable Timer1 Output Compare A Match interrupt (when OCF1A is set)
	TIMSK1 |= _BV(OCIE1A);
	// OCF1A = 1 - Clear Timer1 Output Compare A Match flag
	TIFR1 |= _BV(OCF1A);
	// TCNT1 = 1 - Reset Timer1
	TCNT1 = 0;
	t0 = micros(); // Set start time
	sei(); // Re-enable interrupts

	delay(50); // Let it run for 50 msecs (should yield ~25 interrupts)

	// OCIE1A = 0 - Disable Timer1 Output Compare A Match interrupt
	TIMSK1 &= ~_BV(OCIE1A);
	Serial.println();

	delay(1000);


	PUTS("--------------------------- Test #3 ---------------------------");
	PUTS("Testing Timer1 in CTC mode with 1024x prescaling and interrupt");
	PUTS("on ICR1 match (ICR1A = 15624). For the next 5 secs, the");
	PUTS("TIMER1_COMPA ISR will print usecs between interrupts. With");
	PUTS("1024x prescaling, and 16MHz system clock, ICR1 should match");
	PUTS("TCNT1 (and thus trigger the TIMER1_CAPT interrupt) every");
	PUTS("second. We expect ~5 of those in the following 5 secs:");
        Serial.flush();

	cli(); // Disable interrupts while setting up the test
	// WGM13:0 = 0b1100 - CTC mode, TOP = ICR1
	TCCR1A &= ~(_BV(WGM10) | _BV(WGM11));
	TCCR1B |= (_BV(WGM13) | _BV(WGM12));
	// ICR1 = 15624
	ICR1 = 15624;
	// CS12:0 = 0b101 - 1024x prescaling
	TCCR1B &= ~_BV(CS11);
	TCCR1B |= (_BV(CS12) | _BV(CS10));
	// ICIE1 = 1 - Enable Timer1 Input Capture interrupt (when ICF1 is set)
	TIMSK1 |= _BV(ICIE1);
	// ICF1 = 1 - Clear Timer1 Input Capture flag
	TIFR1 |= _BV(ICF1);
	// TCNT1 = 1 - Reset Timer1
	TCNT1 = 0;
	t0 = micros(); // Set start time
	sei(); // Re-enable interrupts

	delay(5000); // Let it run for 5 secs (should yield ~5 interrupts)

	// ICIE1 = 0 - Disable Timer1 Input Capture interrupt
	TIMSK1 &= ~_BV(ICIE1);
	Serial.println();

	delay(60000);
}
