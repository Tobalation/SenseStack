#include <Arduino.h>
#include <Wire.h>
#include "protocol.h"

byte SELF_ADDR = 126; // test value (1 below top addr)
byte transmissionCounter = 0; // counter for what string to send

// -------------- Utility Functions -------------- //
// Good idea to define any utlity funcitions for dealing with sensors here.

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
    reply = CH_IS_KEY + String("test_key") + CH_MORE;
    // ALWAYS set to the next part of transmission
    transmissionCounter++;
    break;
  
  case 1:
    reply = CH_IS_VALUE + String("test_value") + CH_TERMINATE;
    // ALWAYS set counter to 0 when done sending everything
    transmissionCounter = 0;
    break;
  }
  reply.toCharArray(replyData, MAX_SENSOR_REPLY_LENGTH);
  Serial.println("Sent " + reply);
  Wire.write(replyData); // send string on request
}

// -------------- Arduino framework main code -------------- //

void setup()
{
  Wire.begin(SELF_ADDR);          // join i2c bus with defined address
  Wire.onRequest(sendData);       // register request event
  Serial.begin(9600);             // start serial for debug
}

void loop()
{
  delay(100); // loop forever, waiting for commands
}