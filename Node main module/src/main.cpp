#include <Arduino.h>
#include <Wire.h>
#include <AsyncDelay.h>

#include <WiFi.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>

#include "protocol.h"

#define LED_BUILTIN 2
#define DEFAULT_UPDATE_INTERVAL 10000
#define BLINK_TIME 500

byte modules[MAX_SENSORS];  // array with address listings of connected sensor modules
AsyncDelay delay_sensor_update; // delay timer for asynchronous update interval

String currentJSONReply = "";  // string to hold JSON object to be sent to endpoint

const char* ssid = "TestModule";
const char* password = "12345678";
IPAddress local_ip(192,168,1,1);
IPAddress gateway(192,168,1,1);
IPAddress subnet(255,255,255,0);
AsyncWebServer server(80);

// -------------- Web functions -------------- //

// write status page HTML, TODO: put HTML in PROGMEM
String SendStatus()
{
  // refer to testPage.html in src
  String html = "<!DOCTYPE html><html>\n";
  html +="<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  html +="<title>Main Module Status</title>\n";
  html +="<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}\n";
  html +="body {margin-top: 50px;}h1 {color: #444444;margin: 50px auto 30px;}h2 {color: #707070;margin: 50px auto 30px;}h3 {color: #444444;margin-bottom: 50px;}\n";
  html +="</style>\n";
  html +="</head>\n";

  html +="<body>\n";
  html +="<h1>Main Module Status Page</h1>\n";
  html +="<h2>Current data string</h2>\n";
  html +="<h3>" + currentJSONReply + "</h3>\n";
  html +="<h2>Current up time</h2>\n";
  html +="<h3>" + String(millis()) + "</h3>\n";
  html +="</body>\n";

  html +="</html>\n";
  return html;
}

// handle 404
void handle_NotFound(AsyncWebServerRequest *request)
{
  request->send(404);
}

// -------------- Helper functions -------------- //

// helper function to make LED blink asynchronously 
// ms : Time for LED to lighten up (millisecond)
// ms = 0 means checking whether the LED should be turned of or not.  
void asyncBlink(unsigned long ms = 0)
{
  static unsigned long stopTime = 0;

  if (ms)
  {
    stopTime = millis() + ms;
    digitalWrite(LED_BUILTIN, LOW); // LOW = LED lights up on ESP 32 Lite board.
  }
  else
  {
    // Check whether is it time to turn off the LED.
    if (millis() > stopTime)
    {            
      digitalWrite(LED_BUILTIN,HIGH); 
    }
  }
}

// helper function to scan connected modules on I2C bus
void scanDevices()
{
  for(byte i = 0; i < MAX_SENSORS; i++) // fill modules array with zeroes
  {
    modules[i] = 0;
  }

  byte error, address;
  int nDevices;
  Serial.println("Scanning for connected modules...");
  nDevices = 0;
  for (address = 1; address < TOP_ADDRESS; address++)
  {
    Wire.beginTransmission(address);
    error = Wire.endTransmission();
    if (error == 0) // success
    {
      Serial.print("Module found at address 0x");
      if (address < 16)
      {
        Serial.print("0");
      }
      Serial.println(address, HEX);
      modules[nDevices] = address;
      nDevices++;      
    }
    else if (error == 4) // unknown error
    {
      Serial.print("Unknow error at address 0x");
      if (address < 16)
      {
        Serial.print("0");
      }
      Serial.println(address, HEX);
    }

    if(nDevices > MAX_SENSORS) // maximum number of sensors allowed reached
    {
      Serial.print("Maximum of ");
      Serial.print(MAX_SENSORS);
      Serial.println(" sensors are connected. Terminating scan.");
      break;
    }
  }
  if (nDevices == 0)
  {
    Serial.println("No modules are connected.");
  }
  else
  {
    Serial.println("Scan complete.");
  }
}

// helper function to request data from a sensor module and add it to the JSON packet
void getSensorModuleReading(byte sensorAddr, JsonObject dataObj)
{
  // print out who we are communicating with
  Serial.print("Sending request to 0x");
  if (sensorAddr < 16)
  {
    Serial.print("0");
  }
  Serial.println(sensorAddr, HEX);

  // begin transmission
  Wire.requestFrom(sensorAddr, MAX_SENSOR_REPLY_LENGTH);

  char replyData[MAX_SENSOR_REPLY_LENGTH]= {0};
  char lastSpecifier = 0;
  String dataName = "";
  String dataValue = "";
  uint8_t replyIter = 0;

  while (Wire.available())
  {
    char c = Wire.read();
    switch(c) {
      case CH_DATA_NAME:
        lastSpecifier = c;
        replyIter = 0;
        Serial.print("Reading data name, ");
      break;
      case CH_DATA_BEGIN:
        lastSpecifier = c;
        replyIter = 0;
        Serial.print("Reading data value, ");
      break;
      case CH_TERMINATOR:
        // terminate reply string
        replyData[replyIter] = 0;
        // print out reading to see what we got
        Serial.print("Parsed reading: ");
        Serial.println(replyData);
        // put the parsed reading in the right string
        if(lastSpecifier == CH_DATA_NAME)
        {
          dataName = String(replyData);
        }
        else if(lastSpecifier == CH_DATA_BEGIN)
        {
          dataValue = String(replyData);
        }
        // add data to JSON reply
        dataObj[dataName] = dataValue;
        // clear the data buffer
        memset(replyData,0,sizeof(replyData));
        replyIter = 0;
      break;

      default:
        // append the character into the reply data array and increment replyIter
        replyData[replyIter++] = c;
      break;
    }
  }
}

// helper function to request data from all connected modules and create a JSON object
void fetchData()
{
  byte sensorCount = 0;

  // create JSON document
  StaticJsonDocument<MAX_JSON_REPLY> jsonDoc;
  currentJSONReply = "";
  jsonDoc["UUID"] = "TEST_UUID";
  jsonDoc["Name"] = "TEST_NODE";
  JsonObject dataObj = jsonDoc.createNestedObject("SensorData");

  // obtain information from sensors
  Serial.println("Gathering sensor data.");
  for(int i = 0; i < MAX_SENSORS; i++)
  {
    if(modules[i] != 0)
    {
      asyncBlink(BLINK_TIME);
      getSensorModuleReading(modules[i],dataObj);
      sensorCount++;
    }
    asyncBlink(BLINK_TIME);
  }
  Serial.print("Read from ");
  Serial.print(sensorCount);
  Serial.println(" sensors");

  // serialize JSON reply string
  serializeJson(jsonDoc,currentJSONReply);
  // print out JSON output (for debug purposes)
  Serial.println("Serialized data string:");
  Serial.println(currentJSONReply);
  Serial.println("Saved JSON to string.\n");
}

// -------------- Arduino framework main code -------------- //

void setup()
{
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(9600);
  Wire.begin(); 

  delay_sensor_update.start(DEFAULT_UPDATE_INTERVAL,AsyncDelay::MILLIS);
  scanDevices();

  // setup AP
  WiFi.enableAP(true);
  delay(100);
  WiFi.softAPConfig(local_ip, gateway, subnet);
  WiFi.softAP(ssid, password);
  
  Serial.println("AP started.");
  delay(500);
  
  // attach handlers
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", SendStatus());
  });
  server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", "This has not been implemented yet.");
  });
  server.onNotFound(handle_NotFound);
  // start async server
  server.begin();
  Serial.println("Server started.");
  Serial.print("IP");
  Serial.println(WiFi.softAPIP());
}

void loop()
{
  // data update loop
  if(delay_sensor_update.isExpired())
  {
    scanDevices();
    fetchData();
    delay_sensor_update.restart();
  }
  asyncBlink(BLINK_TIME);
}
