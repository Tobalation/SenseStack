#include <Arduino.h>
#include <Wire.h>
#include "protocol.h"

#define BLINK_TIME 500

byte SELF_ADDR = 0x10;

void asyncBlink(unsigned long ms = 0)
{
  static unsigned long stopTime = 0;

  if (ms){
    stopTime = millis() + ms;
    digitalWrite(LED_BUILTIN, HIGH);      
  }else{
    if (millis() > stopTime){             //Check whether is it time to turn off the LED.
      digitalWrite(LED_BUILTIN,LOW);
    }
  }
}

// function that executes whenever data is requested from master
void sendData()
{
  asyncBlink(BLINK_TIME);
  char replyData[MAX_SENSOR_REPLY_LENGTH];
  String reply = "$test^#BASIC_SENSOR1^";
  reply.toCharArray(replyData, MAX_SENSOR_REPLY_LENGTH);
  Wire.write(replyData); // send string on request
}

// -------------- Arduino framework main code -------------- //

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
  Wire.begin(SELF_ADDR);          // join i2c bus with defined address
  Wire.onRequest(sendData);       // register request event
  Serial.begin(9600);             // start serial for debug
}

void loop()
{
  delay(50); // loop forever, waiting for commands
  asyncBlink();   //Handle LED Blink asynchronously 
}