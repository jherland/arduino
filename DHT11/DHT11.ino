#define DHT11_PIN 0      // define anlog  port 0

byte read_dht11_dat()
{
  byte i = 0;
  byte result=0;
  for(i=0; i< 8; i++)
  {
    while(!(PINC & _BV(DHT11_PIN)))
    {
    };  // wait  forever until anlog input port 0 is '1'   (NOTICE: PINC reads all the analog input ports 
    //and  _BV(X) is the macro operation which pull up positon 'X'to '1' and the rest positions to '0'. it is equivalent to 1<<X.) 
    delayMicroseconds(30);
    if(PINC & _BV(DHT11_PIN))  //if analog input port 0 is still '1' after 30 us
      result |=(1<<(7-i));     //this position is 1
    while((PINC & _BV(DHT11_PIN)));  // wait '1' finish
  }
  return result;
}


void setup()
{
  DDRC |= _BV(DHT11_PIN);   //let analog port 0 be output port 
  PORTC |= _BV(DHT11_PIN);  //let the initial value of this port be '1'
  Serial.begin(9600);
  Serial.println("Ready");
}

void loop()     
{
  byte dht11_dat[5];    
  byte dht11_in;
  byte i;// start condition

  PORTC &= ~_BV(DHT11_PIN);    // 1. pull-down i/o pin for 18ms
  delay(18);
  PORTC |= _BV(DHT11_PIN);     // 2. pull-up i/o pin for 40ms
  delayMicroseconds(1);
  DDRC &= ~_BV(DHT11_PIN);     //let analog port 0 be input port 
  delayMicroseconds(40);      

  dht11_in = PINC & _BV(DHT11_PIN);  // read only the input port 0
  if(dht11_in)
  {
    Serial.println("dht11 start condition 1 not met"); // wait for DHT response signal: LOW
    delay(1000);
    return;
  }
  delayMicroseconds(80);
  dht11_in = PINC & _BV(DHT11_PIN); //  
  if(!dht11_in)
  {
    Serial.println("dht11 start condition 2 not met");  //wair for second response signal:HIGH
    return;
  }

  delayMicroseconds(80);// now ready for data reception
  for (i=0; i<5; i++)
  {  
    dht11_dat[i] = read_dht11_dat();
  }  //recieved 40 bits data. Details are described in datasheet

    DDRC |= _BV(DHT11_PIN);      //let analog port 0 be output port after all the data have been received
  PORTC |= _BV(DHT11_PIN);     //let the  value of this port be '1' after all the data have been received
  byte dht11_check_sum = dht11_dat[0]+dht11_dat[1]+dht11_dat[2]+dht11_dat[3];// check check_sum
  if(dht11_dat[4]!= dht11_check_sum)
  {
    Serial.println("DHT11 checksum error");
  }
  Serial.print("Current humdity = ");
  Serial.print(dht11_dat[0], DEC);
  Serial.print(".");
  Serial.print(dht11_dat[1], DEC);
  Serial.print("%  ");
  Serial.print("temperature = ");
  Serial.print(dht11_dat[2], DEC);
  Serial.print(".");
  Serial.print(dht11_dat[3], DEC);
  Serial.println("C  ");
  delay(2000);  //fresh time
}

