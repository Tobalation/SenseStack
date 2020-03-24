#include <Arduino.h>
#include <Wire.h>
#include "protocol.h"

#define BLINK_TIME 500

byte SELF_ADDR = SENSOR_LIGHT_UV;
byte transmissionCounter = 0; // counter for what string to send

int UVOUT = A0; //Output from the sensor
int REF_3V3 = A1; //3.3V power on the Arduino board
float uvIntensity = 0;

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

//Takes an average of readings on a given pin, returns the average
int averageAnalogRead(int pinToRead) {
  byte numberOfReadings = 8;
  unsigned int runningValue = 0; 

  for(int x = 0 ; x < numberOfReadings ; x++)
    runningValue += analogRead(pinToRead);
    runningValue /= numberOfReadings;
    return(runningValue);  
}

float mapfloat(float x, float in_min, float in_max, float out_min, float out_max){
  return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// -------------- Protocol Function -------------- //
// This function will be called when a request on I2C has been made
// to this module. The main module will continue requesting until
// the character CH_TERMINATE ('!') is recieved

void sendData() {
  char replyData[MAX_SENSOR_REPLY_LENGTH];
  String reply = "";

  switch(transmissionCounter) {
  case 0:
    reply = CH_IS_KEY + String("uv_intensity") + CH_MORE;
    transmissionCounter++;
    break;
  case 1:
    reply = CH_IS_VALUE + String(uvIntensity) + " mw/cm^2" + CH_TERMINATE;
    transmissionCounter = 0;
    break;
  }
  reply.toCharArray(replyData, MAX_SENSOR_REPLY_LENGTH);
  Serial.println("Sent " + reply);
  Wire.write(replyData); // send string on request
  asyncBlink(BLINK_TIME);
}

// -------------- Arduino framework main code -------------- //

void setup(){
  Wire.begin(SELF_ADDR);
  Wire.onRequest(sendData);
  Serial.begin(9600);
  pinMode(UVOUT, INPUT);
  pinMode(REF_3V3, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop(){
  int uvLevel = averageAnalogRead(UVOUT);
  int refLevel = averageAnalogRead(REF_3V3);

  //Use the 3.3V power pin as a reference to get a very accurate output value from sensor

  float outputVoltage = 3.3 / refLevel * uvLevel;
  uvIntensity = mapfloat(outputVoltage, 0.99, 2.8, 0.0, 15.0); //Convert the voltage to a UV intensity level
  Serial.print("output: ");
  Serial.print(refLevel);
  Serial.print("ML8511 output: ");
  Serial.print(uvLevel);
  Serial.print(" / ML8511 voltage: ");
  Serial.print(outputVoltage);
  Serial.print(" / UV Intensity (mW/cm^2): ");
  Serial.print(uvIntensity);
  Serial.println();
  delay(1000);
  asyncBlink(BLINK_TIME);
}