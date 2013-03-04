#include <limits.h>

void setup(void)
{
  Serial.begin(9600);
  Serial.println(F("Ready"));
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
  Serial.print(F("CHAR_BIT: "));
  Serial.println(CHAR_BIT, DEC);
//  Serial.print(F("WORD_BIT: "));
//  Serial.println(WORD_BIT, DEC);
//  Serial.print(F("LONG_BIT: "));
//  Serial.println(LONG_BIT, DEC);
  Serial.println();
  Serial.print(F("CHAR_MAX: "));
  Serial.println(CHAR_MAX, DEC);
  Serial.print(F("CHAR_MIN: "));
  Serial.println(CHAR_MIN, DEC);
  Serial.print(F("SCHAR_MAX: "));
  Serial.println(SCHAR_MAX, DEC);
  Serial.print(F("SCHAR_MIN: "));
  Serial.println(SCHAR_MIN, DEC);
  Serial.print(F("UCHAR_MAX: "));
  Serial.println(UCHAR_MAX, DEC);
  Serial.println();
  Serial.print(F("SHRT_MAX: "));
  Serial.println(SHRT_MAX, DEC);
  Serial.print(F("SHRT_MIN: "));
  Serial.println(SHRT_MIN, DEC);
  Serial.print(F("USHRT_MAX: "));
  Serial.println(USHRT_MAX, DEC);
  Serial.println();
  Serial.print(F("INT_MAX: "));
  Serial.println(INT_MAX, DEC);
  Serial.print(F("INT_MIN: "));
  Serial.println(INT_MIN, DEC);
  Serial.print(F("UINT_MAX: "));
  Serial.println(UINT_MAX, DEC);
  Serial.println();
  Serial.print(F("LONG_MAX: "));
  Serial.println(LONG_MAX, DEC);
  Serial.print(F("LONG_MIN: "));
  Serial.println(LONG_MIN, DEC);
  Serial.print(F("ULONG_MAX: "));
  Serial.println(ULONG_MAX, DEC);
  Serial.println();
//  Serial.print(F("LLONG_MAX: "));
//  Serial.println(LLONG_MAX, DEC);
//  Serial.print(F("LLONG_MIN: "));
//  Serial.println(LLONG_MIN, DEC);
//  Serial.print(F("ULLONG_MAX: "));
//  Serial.println(ULLONG_MAX, DEC);
//  Serial.println();
//  Serial.print(F("SSIZE_MAX: "));
//  Serial.println(SSIZE_MAX, DEC);
//  Serial.print(F("SSIZE_MIN: "));
//  Serial.println(SSIZE_MIN, DEC);
//  Serial.print(F("SIZE_MAX: "));
//  Serial.println(SIZE_MAX, DEC);
}

void loop()
{
}

