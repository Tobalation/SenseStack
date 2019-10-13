#include <Arduino.h>
#include <Wire.h>
#include "protocol.h"

#define BLINK_TIME 500

byte SELF_ADDR = 127;

void blink() 
{
  digitalWrite(LED_BUILTIN,HIGH);
  delay(BLINK_TIME);
  digitalWrite(LED_BUILTIN,LOW);
}

// function that executes whenever data is requested from master
void sendData()
{
  blink();
  char replyData[MAX_SENSOR_REPLY_LENGTH];
  String reply = "basic sensor";
  reply.toCharArray(replyData, MAX_SENSOR_REPLY_LENGTH);
  Wire.write(replyData); // send string on request
}

// -------------- Arduino framework main code -------------- //

void setup()
{
  Wire.begin(SELF_ADDR);          // join i2c bus with defined address
  Wire.onRequest(sendData);       // register request event
  Serial.begin(9600);             // start serial for debug
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop()
{
  delay(50); // loop forever, waiting for commands
}