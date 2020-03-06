#include <Arduino.h>
#include <Wire.h>
#include "protocol.h"

#define BLINK_TIME 500

byte SELF_ADDR = 126; // test value (1 below top addr)
byte transmissionCounter = 0; // counter for what string to send

// -------------- Utility Functions -------------- //
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

// -------------- Protocol Function -------------- //
// This function will be called when a request on I2C has been made
// to this module. The main module will continue requesting until
// the character CH_TERMINATE ('!') is recieved

void sendData()
{
  char replyData[MAX_SENSOR_REPLY_LENGTH];
  String reply = "";

  switch(transmissionCounter) {
  case 0:
    reply = "$testKey^";
    // ALWAYS set to the next part of transmission
    transmissionCounter++;
    break;
  
  case 1:
    reply = "#testValue!";
    // ALWAYS set counter to 0 when done sending everything
    transmissionCounter = 0;
    break;
  }
  reply.toCharArray(replyData, MAX_SENSOR_REPLY_LENGTH);
  Serial.println("Sent " + reply);
  Wire.write(replyData); // send string on request
  asyncBlink(BLINK_TIME);
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
  asyncBlink(BLINK_TIME);   //Handle LED Blink asynchronously 
}