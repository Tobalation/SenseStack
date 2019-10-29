#include <Arduino.h>
#include <Wire.h>
#include "protocol.h"

#include "MQ7.h"

#define BLINK_TIME 500

byte SELF_ADDR = 126;

MQ7 mq7(A0,5.0);
float coPPM = 0.0;

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
  String reply = CH_DATA_NAME + String("CO") + CH_TERMINATOR + CH_DATA_BEGIN + String(coPPM) + CH_TERMINATOR;
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
  delay(1000);
  coPPM = mq7.getPPM();
  delay(50); // loop forever, waiting for commands
}