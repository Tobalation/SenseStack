#include <Arduino.h>
#include <SoftwareSerial.h>
#include <Wire.h>
#include "protocol.h"

#define BLINK_TIME 500

byte SELF_ADDR = SENSOR_PM25;

SoftwareSerial mySerial(2,3); // RX, TX
unsigned int pm1 = 0;
unsigned int pm2_5 = 0;
unsigned int pm10 = 0;

void blink() {
  digitalWrite(LED_BUILTIN,HIGH);
  delay(BLINK_TIME);
  digitalWrite(LED_BUILTIN,LOW);
}

// function that executes whenever data is requested from master
void sendData() {
  char replyData[MAX_SENSOR_REPLY_LENGTH];
  String reply = String(pm1) + ' ' + String(pm2_5) + ' ' + String(pm10);
  reply.toCharArray(replyData, MAX_SENSOR_REPLY_LENGTH);
  Serial.println(replyData);
  Wire.write(replyData); // send string on request
  blink();
}

void setup() {
  Wire.begin(SELF_ADDR);
  Wire.onRequest(sendData);
  Serial.begin(9600);
  while (!Serial) ;
  mySerial.begin(9600);
  pinMode(LED_BUILTIN, OUTPUT);
}

void loop() {
  int index = 0;
  char value;
  char previousValue;
  
  while (mySerial.available()) {
    value = mySerial.read();
    if ((index == 0 && value != 0x42) || (index == 1 && value != 0x4d)){
    Serial.println("Cannot find the data header.");
    break;
    }

    if (index == 4 || index == 6 || index == 8 || index == 10 || index == 12 || index == 14) {
      previousValue = value;
    } else if (index == 5) {
      pm1 = 256 * previousValue + value;
      Serial.print("{ ");
      Serial.print("\"pm1\": ");
      Serial.print(pm1);
      Serial.print(" ug/m3");
      Serial.print(", ");
    } else if (index == 7) {
      pm2_5 = 256 * previousValue + value;
      Serial.print("\"pm2_5\": ");
      Serial.print(pm2_5);
      Serial.print(" ug/m3");
      Serial.print(", ");
    } else if (index == 9) {
      pm10 = 256 * previousValue + value;
      Serial.print("\"pm10\": ");
      Serial.print(pm10);
      Serial.print(" ug/m3");
    } else if (index > 15) {
      break;
    }
    index++;
  }
  
  while(mySerial.available()) mySerial.read();
  Serial.println(" }");
  delay(1000);
}