#include <Arduino.h>
#include <Wire.h>
#include <AsyncDelay.h>

#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <AutoConnect.h>
#include <ArduinoJson.h>

#include "protocol.h"

#define LED_BUILTIN 2
#define DEFAULT_UPDATE_INTERVAL 20000
#define BLINK_TIME 500

byte modules[MAX_SENSORS];  // array with address listings of connected sensor modules
AsyncDelay delay_sensor_update; // delay timer for asynchronous update interval
unsigned long currentUpdateRate = DEFAULT_UPDATE_INTERVAL;
String currentJSONReply = "";  // string to hold JSON object to be sent to endpoint
// TODO: save string to SPIFFS, EEPROM would conflict with credentials storage
String currentEndPoint = "https://yourgisdb.com/apiforposting/"; // endpoint URL string
String lastPOSTreply = "N/A"; // string to save last POST status reply

WebServer server; // HTTP server to serve web UI
AutoConnect Portal(server); // AutoConnect handler object
AutoConnectConfig portalConfig("MainModuleAP","12345678");

// TODO: make pages JSON strings in PROGMEM
// see https://hieromon.github.io/AutoConnect/achandling.html#transfer-of-input-values-across-pages for example
// Endpoint config menu
AutoConnectText header("header","<h2>Data endpoint configuration<h2>","padding:10px");
AutoConnectText caption("caption","Enter the URL of the destination that you wish to send data to.");
AutoConnectInput urlInput("urlInput","","URL","","Endpoint URL");
AutoConnectText header2("header2","<h2>Update interval configuration<h2>","padding:10px");
AutoConnectText caption2("caption2","Enter the time for each update interval. Updating this value will restart the timer.");
AutoConnectInput intervalInput("intervalInput",String(DEFAULT_UPDATE_INTERVAL).c_str(),"Update Interval (ms)","","ms");
AutoConnectSubmit save("save","Save","/ep_save");
AutoConnectAux endpointConfigPage("/epconfig","Module configuration",true,{header,caption,urlInput,header2,caption2,intervalInput,save});

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

// -------------- Web functions -------------- //

// TODO: put pages in PROGMEM or SPIFFS for better space efficiency
// status page HTML
String StatusPage()
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
  html +="<h1>Current Module Status</h1>\n";
  html +="<h2>Latest data string</h2>\n";
  html +="<h3>" + currentJSONReply + "</h3>\n";
  html +="<h2>Current endpoint URL</h2>\n";
  html +="<h3>" + currentEndPoint + "</h3>\n";
  html +="<h2>Last end point reply</h2>\n";
  html +="<h3>" + lastPOSTreply + "</h3>\n";
  html +="<h2>Current update interval (milliseconds)</h2>\n";
  html +="<h3>" + String(currentUpdateRate) + "</h3>\n";
  html +="<h2>Current up time (seconds)</h2>\n";
  html +="<h3>" + String(millis() / 1000) + "</h3>\n";
  html +="<p><a href='/_ac'>Main page</a></p>\n";
  html +="</body>\n";
  html +="</html>\n";
  return html;
}

// 404 page HTML
String RedirectPage()
{
  String html = "<!DOCTYPE html><html>\n";
  html +="<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  html +="<title>Page not found</title></head>\n";
  html +="<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}</style>\n";
  html +="<body>\n";
  html +="<h1>404</h1>\n";
  html +="<p>This page does not exist.</p>\n";
  html +="<p><a href='/_ac'>Return to main page</a></p>\n";
  html +="</body>\n";
  html +="</html>\n";
  return html;
}

// show status
void handle_Status()
{
  server.send(200, "text/html", StatusPage());
}

// handle 404
void handle_NotFound()
{
  server.send(404, "text/html", RedirectPage());
}

// save the endpoint string
void handle_SaveEndpoint()
{
  // get args from server and save them to control variables
  String newurl = server.arg("urlInput");
  currentEndPoint = newurl;
  unsigned long newinterval = server.arg("intervalInput").toInt();
  currentUpdateRate = newinterval;
  Serial.println("Saved new end point URL as " + currentEndPoint);
  Serial.println("Saved new update rate to be " + String(currentUpdateRate) + " ms");

  // reset update intveral
  if(delay_sensor_update.isExpired())
  {
    delay_sensor_update.restart();
  }
  delay_sensor_update.start(currentUpdateRate,AsyncDelay::MILLIS);

  // redirect back to main page after saving
  server.sendHeader("Location", "/status",true);
  server.send(302, "text/plain",""); 
}

// POST latest JSON string to current URL endpoint
void sendDataToEndpoint()
{
  Serial.println("Sending data to " + currentEndPoint);  
 
  HTTPClient http;   
  http.begin(currentEndPoint);
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.POST(currentJSONReply);
  String response = http.getString();
  lastPOSTreply = "Code: ";
  lastPOSTreply += httpResponseCode;
  lastPOSTreply += " ";
  lastPOSTreply += http.errorToString(httpResponseCode);
  lastPOSTreply += " Reply: ";
  lastPOSTreply += response;

  if(httpResponseCode > 0)
  {
    Serial.println("Response from server:");
    Serial.println(http.errorToString(httpResponseCode));
    Serial.println(response);
  }
  else
  {
    Serial.print("Error on sending POST: ");
    Serial.println(http.errorToString(httpResponseCode));
  }
  http.end();
}


// -------------- Sensor Module functions -------------- //

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

  // begin i2c transmission
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
  // init serial, I2C and status LED
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(9600);
  Wire.begin(); 

  // attach handlers
  server.on("/status", handle_Status);
  server.on("/", handle_Status);
  server.on("/ep_save", handle_SaveEndpoint);

  // setup the web UI
  portalConfig.title = "Main Module v1.0";
  portalConfig.apid = "MainModule-" + String((uint32_t)(ESP.getEfuseMac() >> 32), HEX);
  portalConfig.apip = IPAddress(192,168,1,1);
  portalConfig.gateway = IPAddress(192,168,1,1);
  portalConfig.bootUri = AC_ONBOOTURI_ROOT;
  Portal.config(portalConfig);
  Portal.home("/status");
  Portal.onNotFound(handle_NotFound);
  Portal.join({endpointConfigPage});
  
  // initialize the web UI
  if(Portal.begin())
  {
    Serial.println("\nNetworking started.");
    Serial.print("IP: ");
    Serial.println(WiFi.localIP());
    Serial.println();
  }
  else
  {
    Serial.println("Connection failed, rebooting.");
    ESP.restart();
  }

  // perform initial device scan
  Serial.println("Performing initial device scan.");
  scanDevices();
  delay_sensor_update.start(currentUpdateRate,AsyncDelay::MILLIS);
}

void loop()
{
  // handle web UI
  server.handleClient();
  Portal.handleRequest();

  // data update loop
  if(delay_sensor_update.isExpired())
  {
    scanDevices();
    fetchData();
    if(currentJSONReply != NULL || currentJSONReply != "")
    {
      sendDataToEndpoint();
    }
    delay_sensor_update.restart();
  }
  asyncBlink(BLINK_TIME);
}
