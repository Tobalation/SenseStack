#include <Arduino.h>
#include <Wire.h>
#include "protocol.h"

#include "DHT.h"

#define BLINK_TIME 500

byte SELF_ADDR = SENSOR_TEMP_HUM;

DHT dht(PIND2,DHT22);
float humidity = 0.0, temperature = 0.0;

void asyncBlink(unsigned long ms = 0)
{
  static unsigned long stopTime = 0;

  if (ms)
  {
    stopTime = millis() + ms;
    digitalWrite(LED_BUILTIN, HIGH);      
  }
  else
  {
    //Check whether is it time to turn off the LED.
    if (millis() > stopTime)
    { 
      digitalWrite(LED_BUILTIN,LOW);
    }
  }
}

// function that executes whenever data is requested from master
void sendData()
{
  char replyData[MAX_SENSOR_REPLY_LENGTH];
  String reply = CH_DATA_NAME + String("temp") + CH_TERMINATOR + CH_DATA_BEGIN + String(temperature) + CH_TERMINATOR 
  + CH_DATA_NAME + String("humid") + CH_TERMINATOR + CH_DATA_BEGIN + String(humidity) + CH_TERMINATOR;
  reply.toCharArray(replyData, MAX_SENSOR_REPLY_LENGTH);
  Serial.println(replyData);
  Wire.write(replyData); // send string on request
  asyncBlink(BLINK_TIME);
}

// -------------- Arduino framework main code -------------- //

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
  dht.begin();
  Wire.begin(SELF_ADDR);          // join i2c bus with defined address
  Wire.onRequest(sendData);       // register request event
  Serial.begin(9600);             // start serial for debug
}

void loop()
{
  delay(1000);
  humidity = dht.readHumidity();
  delay(100);
  temperature = dht.readTemperature();
  asyncBlink();     //Handle asyn blink (Check LED Status periodically)
}