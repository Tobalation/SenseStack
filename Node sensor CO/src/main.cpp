#include <Arduino.h>
#include <Wire.h>
#include "protocol.h"

#include "MQ7.h"

byte SELF_ADDR = SENSOR_CO;
byte transmissionCounter = 0; // counter for what string to send

MQ7 mq7(A0,5.0);
float coPPM = 0.0;

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
    reply = CH_IS_KEY + String("co_density") + CH_MORE;
    transmissionCounter++;
    break;
  case 1:
    reply = CH_IS_VALUE + String(coPPM) + " ppm" + CH_TERMINATE;
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
  delay(1000);
  coPPM = mq7.getPPM();
}