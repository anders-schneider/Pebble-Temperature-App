#include <Wire.h> 
 
#define BAUD (9600)    /* Serial baud define */
#define _7SEG (0x38)   /* I2C address for 7-Segment */
#define THERM (0x49)   /* I2C address for digital thermometer */
#define EEP (0x50)     /* I2C address for EEPROM */
#define RED (3)        /* Red color pin of RGB LED */
#define GREEN (5)      /* Green color pin of RGB LED */
#define BLUE (6)       /* Blue color pin of RGB LED */

#define COLD (23)      /* Cold temperature, drive blue LED (23c) */
#define HOT (26)       /* Hot temperature, drive red LED (27c) */

const byte NumberLookup[16] =   {0x3F,0x06,0x5B,0x4F,0x66,
                                 0x6D,0x7D,0x07,0x7F,0x6F, 
                                 0x77,0x7C,0x39,0x5E,0x79,0x71};

/* Global state variables */
bool standby = false;
bool isCelsius = true;
bool danceMode = false;
bool isState1 = true;
int color = 1;

/* Function prototypes */
void Cal_temp (int&, byte&, byte&, bool&);
void Dis_7SEG (int, byte, byte, bool);
void Send7SEG (byte, byte);
void SerialMonitorPrint (byte, int, bool);
void UpdateRGB (byte);

/***************************************************************************
 Function Name: setup

 Purpose: 
   Initialize hardwares.
****************************************************************************/

void setup() 
{ 
  Serial.begin(BAUD);
  Wire.begin();        /* Join I2C bus */
  pinMode(RED, OUTPUT);    
  pinMode(GREEN, OUTPUT);  
  pinMode(BLUE, OUTPUT);   
  delay(500);          /* Allow system to stabilize */
} 

/***************************************************************************
 Function Name: loop

 Purpose: 
   Run-time forever loop.
****************************************************************************/
 
void loop() 
{ 
  int Decimal;
  byte Temperature_H, Temperature_L, counter, counter2;
  bool IsPositive;
  
  /* Configure 7-Segment to 12mA segment output current, Dynamic mode, 
     and Digits 1, 2, 3 AND 4 are NOT blanked */
     
  Wire.beginTransmission(_7SEG);   
  byte val = 0; 
  Wire.write(val);
  val = B01000111;
  Wire.write(val);
  Wire.endTransmission();
  
  /* Setup configuration register 12-bit */
     
  Wire.beginTransmission(THERM);  
  val = 1;  
  Wire.write(val);
  val = B01100000;
  Wire.write(val);
  Wire.endTransmission();
  
  /* Setup Digital THERMometer pointer register to 0 */
     
  Wire.beginTransmission(THERM); 
  val = 0;  
  Wire.write(val);
  Wire.endTransmission();
  
  /* Test 7-Segment */
  for (counter=0; counter<8; counter++)
  {
    Wire.beginTransmission(_7SEG);
    Wire.write(1);
    for (counter2=0; counter2<4; counter2++)
    {
      Wire.write(1<<counter);
    }
    Wire.endTransmission();
    delay (250);
  }
  
  while (1)
  {
    /* Handle incoming commands from server */
    if(Serial.available() > 0) {
      int incomingbyte = Serial.read();
      if(incomingbyte == 2){
        // Enter standby mode
        standby = true;
        danceMode = false;
      }
      else if(incomingbyte == 3){
        // Exit standby mode
        standby = false;
      }
      else if(incomingbyte == 4){
        // Change units to Celsius
        isCelsius = true;
      }
      else if(incomingbyte == 5){
        // Change units to Fahrenheit
        isCelsius = false;
      }
      else if(incomingbyte == 6){
        // Enable dance mode
        danceMode = true;
      }
      else if(incomingbyte == 7){
        // Disable dance mode
        danceMode = false;
      }
    }

    /* Standby mode
     * - Clear 7-segment display
     * - Send junk data to server once per second
     */
    if (standby) {
        for (counter = 1; counter <= 4; counter++) {
          Send7SEG(counter,0x40);
        }
        Serial.print("10000.0\n");
        delay (1000);
    }

    /* Dance mode
     * - Switch dance state
     * - Cycle through LED colors
     * - Every other iteration send junk data (once per second)
     */
    else if (danceMode) {
        danceSteps();
        discoLights();
        delay(500);
        if(isState1){
          Serial.print("10000.0\n");
        }
        isState1 = !isState1;
        color = (color + 1) % 8;
        if(color == 0){
          color = 1;
        }
    }
    
    /* Normal mode
     * - Take temperature reading
     * - Send temperature to serial monitor
     * - Turn on matching LED (based on temp)
     * - Display temperature on 7-segment display
     */
    else {
      Wire.requestFrom(THERM, 2);
      Temperature_H = Wire.read();
      Temperature_L = Wire.read();
      
      /* Calculate temperature */
      Cal_temp (Decimal, Temperature_H, Temperature_L, IsPositive);
      
      /* Display temperature on the serial monitor. 
         Comment out this line if you don't use serial monitor.*/
      SerialMonitorPrint (Temperature_H, Decimal, IsPositive);
      
      /* Update RGB LED.*/
      UpdateRGB (Temperature_H);
      
      /* Display temperature on the 7-Segment */
      Dis_7SEG (Decimal, Temperature_H, Temperature_L, IsPositive);
      delay (1000);        /* Take temperature read every 1 second */
    }
  }
} 

/***************************************************************************
 Function Name: Cal_temp

 Purpose: 
   Calculate temperature from raw data.
****************************************************************************/
void Cal_temp (int& Decimal, byte& High, byte& Low, bool& sign)
{
  if ((High&B10000000)==0x80)    /* Check for negative temperature. */
    sign = 0;
  else
    sign = 1;
    
  High = High & B01111111;      /* Remove sign bit */
  Low = Low & B11110000;        /* Remove last 4 bits */
  Low = Low >> 4; 
  Decimal = Low;
  Decimal = Decimal * 625;      /* Each bit = 0.0625 degree C */
  
  if (sign == 0)                /* if temperature is negative */
  {
    High = High ^ B01111111;    /* Complement all of the bits, except the MSB */
    Decimal = Decimal ^ 0xFF;   /* Complement all of the bits */
  }  
}

/***************************************************************************
 Function Name: Dis_7SEG

 Purpose: 
   Display number on the 7-segment display.
****************************************************************************/
void Dis_7SEG (int Decimal, byte High, byte Low, bool sign)
{
  byte Digit = 4;                 /* Number of 7-Segment digit */
  byte Number;     /* Temporary variable hold the number to display */
  bool aboveHundred = false;

  // If displaying in Fahrenheit, convert from C to F
  if (!isCelsius) {
    double decTemp = Decimal;
    while (decTemp >= 1) {
      decTemp /= 10;
    }
    
    double cTemp = High + decTemp;
    if (!sign) {
      cTemp = cTemp * -1;
    }
    
    double fTemp = (cTemp * 9/5) + 32;
    sign = fTemp >= 0;
    High = ((byte) fTemp) ;
    decTemp = fTemp - High;
    Decimal = decTemp * 10000;  
  }
  
  if (sign == 0)                  /* When the temperature is negative */
  {
    Send7SEG(Digit,0x40);         /* Display "-" sign */
    Digit--;                      /* Decrement number of digit */
  }
  
  if (High > 99)                  /* When the temperature is three digits long */
  {
    aboveHundred = true;
    Number = High / 100;          /* Get the hundredth digit */
    Send7SEG (Digit,NumberLookup[Number]);     /* Display on the 7-Segment */
    High = High % 100;            /* Remove the hundredth digit from the TempHi */
    Digit--;                      /* Subtract 1 digit */    
  }
  
  if (High > 9 || aboveHundred)
  {
    Number = High / 10;           /* Get the tenth digit */
    Send7SEG (Digit,NumberLookup[Number]);     /* Display on the 7-Segment */
    High = High % 10;            /* Remove the tenth digit from the TempHi */
    Digit--;                      /* Subtract 1 digit */
  }
  
  Number = High;                  /* Display the last digit */
  Number = NumberLookup [Number]; 
  if (Digit > 1)                  /* Display "." if it is not the last digit on 7-SEG */
  {
    Number = Number | B10000000;
  }
  Send7SEG (Digit,Number);  
  Digit--;                        /* Subtract 1 digit */
  
  if (Digit > 0)                  /* Display decimal point if there is more space on 7-SEG */
  {
    Number = Decimal / 1000;
    Send7SEG (Digit,NumberLookup[Number]);
    Digit--;
  }

  if (Digit > 0)                 /* Display "C" or "F" if there is more space on 7-SEG */
  {
    if (isCelsius) {
      // "C"
      Send7SEG (Digit,0x39);
    }
    else {
      // "F"
      Send7SEG (Digit,0x71);
    }
    Digit--;
  }
  
  if (Digit > 0)                 /* Clear the rest of the digit */
  {
    Send7SEG (Digit,0x00);
  }
}

/***************************************************************************
 Function Name: Send7SEG

 Purpose: 
   Send I2C commands to drive 7-segment display.
****************************************************************************/

void Send7SEG (byte Digit, byte Number)
{
  Wire.beginTransmission(_7SEG);
  Wire.write(Digit);
  Wire.write(Number);
  Wire.endTransmission();
}

/***************************************************************************
 Function Name: UpdateRGB

 Purpose: 
   Update RGB LED according to define HOT and COLD temperature. 
****************************************************************************/

void UpdateRGB (byte Temperature_H)
{
  digitalWrite(RED, LOW);
  digitalWrite(GREEN, LOW);
  digitalWrite(BLUE, LOW);        /* Turn off all LEDs. */
  
  if (Temperature_H <= COLD)
  {
    digitalWrite(BLUE, HIGH);
  }
  else if (Temperature_H >= HOT)
  {
    digitalWrite(RED, HIGH);
  }
  else 
  {
    digitalWrite(GREEN, HIGH);
  }
}

/***************************************************************************
 Function Name: danceSteps

 Purpose: 
   Update 7-Segment display with the next dancing man state. 
****************************************************************************/

void danceSteps()
{
  if (isState1) {
    Send7SEG (4,0x63);
    Send7SEG (3,0x5B);
    Send7SEG (2,0x40);
    Send7SEG (1,0x39);
  }
  else {
    Send7SEG (4,0x5C);
    Send7SEG (3,0x6D);
    Send7SEG (2,0x40);
    Send7SEG (1,0x39);
  }
}

/***************************************************************************
 Function Name: discoLights

 Purpose:
   Update the RGB LED with the next color in the disco sequence 
****************************************************************************/

void discoLights(){
  // Turn off all LEDs
  digitalWrite(RED, LOW);
  digitalWrite(GREEN, LOW);
  digitalWrite(BLUE, LOW); 
  
  // Determine which LEDs should be turned on
  if (color & 0b100) {
     digitalWrite(RED, HIGH);
  }
  if (color & 0b010) {
     digitalWrite(BLUE, HIGH);
  }
  if (color & 0b001) {
     digitalWrite(GREEN, HIGH);
  }
}

/***************************************************************************
 Function Name: SerialMonitorPrint

 Purpose: 
   Print current read temperature to the serial monitor.
****************************************************************************/

void SerialMonitorPrint (byte Temperature_H, int Decimal, bool IsPositive)
{
    if (!IsPositive)
    {
      Serial.print("-");
    }
    Serial.print(Temperature_H, DEC);
    Serial.print(".");
    Serial.print(Decimal, DEC);
    Serial.print("\n");
}
