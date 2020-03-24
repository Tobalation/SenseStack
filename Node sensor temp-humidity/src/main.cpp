#include <Arduino.h>
#include <Wire.h>
#include "protocol.h"

#include "DHT.h"

byte SELF_ADDR = SENSOR_TEMP_HUM;
byte transmissionCounter = 0; // counter for what string to send

DHT dht(PIND2,DHT22);
float humidity = 0.0, temperature = 0.0;

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
    reply = CH_IS_KEY + String("temperature") + CH_MORE;
    transmissionCounter++;
    break;
  case 1:
    reply = CH_IS_VALUE + String(temperature) + " c" + CH_MORE;
    transmissionCounter++;
    break;
  case 2:
    reply = CH_IS_KEY + String("humidity") + CH_MORE;
    transmissionCounter++;
    break;
  case 3:
    reply = CH_IS_VALUE + String(humidity) + " %" + CH_TERMINATE;
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
  dht.begin();
  Wire.begin(SELF_ADDR);          // join i2c bus with defined address
  Wire.onRequest(sendData);       // register request event
  Serial.begin(9600);             // start serial for debug
  Serial.println("Temperature/Humidity module started.");
}

void loop()
{
  delay(1000);
  humidity = dht.readHumidity();
  delay(100);
  temperature = dht.readTemperature();
}