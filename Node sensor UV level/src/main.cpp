#include <Arduino.h>
#include <Wire.h>
#include "protocol.h"

int UVOUT = A0; //Output from the sensor
int REF_3V3 = A1; //3.3V power on the Arduino board
float uvIntensity = 0;

#define BLINK_TIME 500

byte SELF_ADDR = SENSOR_LIGHT_UV;

void blink() {
  digitalWrite(LED_BUILTIN,HIGH);
  delay(BLINK_TIME);
  digitalWrite(LED_BUILTIN,LOW);
}

// function that executes whenever data is requested from master
void sendData() {
  char replyData[MAX_SENSOR_REPLY_LENGTH];
  String reply = String(uvIntensity);
  reply.toCharArray(replyData, MAX_SENSOR_REPLY_LENGTH);
  Serial.println(replyData);
  Wire.write(replyData); // send string on request
  blink();
}

void setup(){
  Wire.begin(SELF_ADDR);
  Wire.onRequest(sendData);
  Serial.begin(9600);
  pinMode(UVOUT, INPUT);
  pinMode(REF_3V3, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
}

//Takes an average of readings on a given pin

//Returns the average

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
  delay(100);
}