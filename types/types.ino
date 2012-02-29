void setup(void)
{
  Serial.begin(9600);
  Serial.println("Ready");
  Serial.print("boolean has size ");
  Serial.println(sizeof(boolean), DEC);
  Serial.print("char has size ");
  Serial.println(sizeof(char), DEC);
  Serial.print("short has size ");
  Serial.println(sizeof(short), DEC);
  Serial.print("int has size ");
  Serial.println(sizeof(int), DEC);
  Serial.print("size_t has size ");
  Serial.println(sizeof(size_t), DEC);
  Serial.print("long has size ");
  Serial.println(sizeof(long), DEC);
  Serial.print("long long has size ");
  Serial.println(sizeof(long long), DEC);
}

void loop()
{
}

