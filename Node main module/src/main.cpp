#include <Arduino.h>
#include <Wire.h>
#include "protocol.h"

#define LED_BUILTIN 2
#define UPDATE_INTERVAL 6000
#define BLINK_TIME 500

byte testCount = 0;

// char moduleData[MAX_SENSORS][MAX_SENSOR_REPLY_LENGTH];  // module reply string buffer
byte modules[MAX_SENSORS];  // array with address listings of connected sensor modules

void blink()
{
  digitalWrite(LED_BUILTIN, HIGH);
  delay(BLINK_TIME);
  digitalWrite(LED_BUILTIN,LOW);
}

// helper function to scan connected modules on I2C bus
void scanDevices()
{
  for(byte i = 0; i < MAX_SENSORS; i++) // fill modules array with zeroes
  {
    modules[i] = 0;
  }

  byte error, address;
  int nDevices;
  Serial.println("Scanning for connected modules...");
  nDevices = 0;
  for (address = 1; address < TOP_ADDRESS; address++)
  {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) // success
    {
      Serial.print("Module found at address 0x");
      if (address < 16)
      {
        Serial.print("0");
      }
      Serial.println(address, HEX);
      modules[nDevices] = address;
      nDevices++;      
    }
    else if (error == 4) // unknown error
    {
      Serial.print("Unknow error at address 0x");
      if (address < 16)
      {
        Serial.print("0");
      }
      Serial.println(address, HEX);
    }

    if(nDevices > MAX_SENSORS) // maximum number of sensors allowed reached
    {
      Serial.print("Maximum of ");
      Serial.print(MAX_SENSORS);
      Serial.println(" sensors are connected. Terminating scan.");
      break;
    }
  }
  if (nDevices == 0)
  {
    Serial.println("No modules are connected.\n");
  }
  else
  {
    Serial.println("Scan complete.\n");
  }
  delay(5000);
}

// helper function to request data from a sensor module
void getSensorModuleReading(byte sensorAddr)
{
  Serial.print("Sending request to 0x");
  if (sensorAddr < 16)
  {
    Serial.print("0");
  }
  Serial.println(sensorAddr, HEX);

  Wire.requestFrom(sensorAddr, MAX_SENSOR_REPLY_LENGTH);
  while (Wire.available())
  {
    char c = Wire.read();
    Serial.print(c);
  }
  Serial.println();
}

// -------------- Arduino framework main code -------------- //

void setup()
{
  delay(2000); // allow any slow modules to startup
  blink();
  blink();
  blink();
  Serial.begin(9600);
  Wire.begin();
  pinMode(LED_BUILTIN, OUTPUT);
  scanDevices();
}

void loop()
{
  Serial.println("Gathering sensor data.");
  for(int i = 0; i < MAX_SENSORS; i++)
  {
    if(modules[i] != 0) //
    {
      blink();
      getSensorModuleReading(modules[i]);
      testCount++;
    }
  }
  Serial.print("Read from ");
  Serial.print(testCount);
  Serial.println(" sensors");
  testCount = 0;
  delay(UPDATE_INTERVAL);
}
  