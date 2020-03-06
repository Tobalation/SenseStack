#include <Arduino.h>
#include <Wire.h>
#include <SPIFFS.h>

#include <AsyncDelay.h>

#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <AutoConnect.h>
#include <ArduinoJson.h>

#include "protocol.h"

#define LED_BUILTIN 2
#define DEFAULT_UPDATE_INTERVAL 60000
#define BLINK_TIME 500
#define SETTINGS_FILE "/settings.txt"
#define DATA_TRANSMISSION_TIMEOUT 20 // arbitrary number

byte modules[MAX_SENSORS];  // array with address listings of connected sensor modules
AsyncDelay delay_sensor_update; // delay timer for asynchronous update interval

String currentJSONReply = "No data";  // string to hold JSON object to be sent to endpoint
String lastPOSTreply = "N/A"; // string to save last POST status reply

// config vars set to default values
String nodeName = "MainModule";
String nodeUUID = "1234567890";
String nodeLat = "N/A";
String nodeLong = "N/A";
String currentEndPoint = "https://yourgisdb.com/apiforposting/";
unsigned long currentUpdateRate = DEFAULT_UPDATE_INTERVAL;

WebServer server; // HTTP server to serve web UI
AutoConnect Portal(server); // AutoConnect handler object
AutoConnectConfig portalConfig("MainModuleAP","12345678");

// configuration and status pages
const static char customPageJSON[] PROGMEM = R"raw(
[
    {
        "title": "Module configuration",
        "uri": "/moduleconfig",
        "menu": true,
        "element": [
            {
                "name": "header_node",
                "type": "ACText",
                "value": "<h2>Node configuration<h2>"
            },
            {
                "name": "caption_node",
                "type": "ACText",
                "value": "Name and UUID identifier for this node."
            },
            {
                "name": "nameInput",
                "type": "ACInput",
                "label": "Node name"
            },
            {
                "name": "uuidInput",
                "type": "ACInput",
                "label": "UUID"
            },
            {
                "name": "caption_GPS",
                "type": "ACText",
                "value": "GPS coordinates of this node."
            },
            {
                "name": "latInput",
                "type": "ACInput",
                "label": "Latitude"
            },
            {
                "name": "longInput",
                "type": "ACInput",
                "label": "Longitude"
            },
            {
                "name": "currentLocBtn",
                "type": "ACButton",
                "value": "Use current location",
                "action": "function getPos() {navigator.geolocation.getCurrentPosition(set);}function set(pos){document.getElementById('latInput').value = pos.coords.latitude; document.getElementById('longInput').value = pos.coords.longitude;} getPos();"
            },
            {
                "name": "header_url",
                "type": "ACText",
                "value": "<h2>Data endpoint configuration<h2>"
            },
            {
                "name": "caption_url",
                "type": "ACText",
                "value": "Enter the URL of the destination that you wish to send data to."
            },
            {
                "name": "urlInput",
                "type": "ACInput",
                "label": "URL"
            },
            {
                "name": "header_interval",
                "type": "ACText",
                "value": "<h2>Update interval configuration<h2>"
            },
            {
                "name": "caption_interval",
                "type": "ACText",
                "value": "Enter the time for each update interval. Updating this value will restart the timer."
            },
            {
                "name": "intervalInput",
                "type": "ACInput",
                "label": "Update Interval (ms)"
            },
            {
                "name": "save",
                "type": "ACSubmit",
                "value": "Save settings",
                "uri": "/save_settings"
            }
        ]
    },
    {
        "title": "Node status",
        "uri": "/status",
        "menu": false,
        "element": [
            {
                "name": "header_title",
                "type": "ACText",
                "style": "color:#06697c;border-bottom:5px solid black",
                "value": "<h2>Node name status<h2>"
            },
            {
                "name": "header_lastreply",
                "type": "ACText",
                "style": "color:#06697c;",
                "value": "<h2>Latest data string<h2>"
            },
            {
                "name": "currentReply",
                "type": "ACText",
                "value": "No data"
            },
            {
                "name": "header_endpoint",
                "type": "ACText",
                "style": "color:#06697c;",
                "value": "<h2>Current data endpoint URL<h2>"
            },
            {
                "name": "currentEndpoint",
                "type": "ACText",
                "value": "No data"
            },
            {
                "name": "header_epreply",
                "type": "ACText",
                "style": "color:#06697c;",
                "value": "<h2>Last reply from endpoint<h2>"
            },
            {
                "name": "lastPOSTreply",
                "type": "ACText",
                "value": "N/A"
            },
            {
                "name": "header_interval",
                "type": "ACText",
                "style": "color:#06697c;",
                "value": "<h2>Current update interval (milliseconds)<h2>"
            },
            {
                "name": "currentUpdateRate",
                "type": "ACText",
                "value": "0"
            },
            {
                "name": "header_uptime",
                "type": "ACText",
                "style": "color:#06697c;",
                "value": "<h2>Current up time (seconds)<h2>"
            },
            {
                "name": "currentUpTime",
                "type": "ACText",
                "value": "0"
            }
        ]
    }
]
)raw";

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

// helper function to write settings to file in SPIFFS.
void saveSettings()
{
  File settingsFile = SPIFFS.open(SETTINGS_FILE, FILE_WRITE);
  if(!settingsFile)
  {
    Serial.println("Failed to open file stream. Save failed.");
    return;
  }
  settingsFile.println(nodeUUID);
  settingsFile.println(nodeName);
  settingsFile.println(currentEndPoint);
  settingsFile.println(currentUpdateRate);
  settingsFile.println(nodeLat);
  settingsFile.println(nodeLong);
  Serial.println("Wrote existing settings to save file.");
  settingsFile.close();
}

// helper function to load settings from save file in SPIFFS.
void loadSettings()
{
  File settingsFile = SPIFFS.open(SETTINGS_FILE, FILE_READ);
  if(!SPIFFS.exists(SETTINGS_FILE) || !settingsFile) // settings file does not exist, set everything to default.
  {
    Serial.println("Settings file does not exist or could not be opened. Creating new setings file.");
    File newSettingsFile = SPIFFS.open(SETTINGS_FILE,FILE_WRITE);
    newSettingsFile.println(nodeUUID);
    newSettingsFile.println(nodeName);
    newSettingsFile.println(currentEndPoint);
    newSettingsFile.println(currentUpdateRate);
    newSettingsFile.println(nodeLat);
    newSettingsFile.println(nodeLong);
    Serial.println("Wrote default settings to file.");
    newSettingsFile.close();
  }
  else // read existing settings
  {
    if(SPIFFS.exists(SETTINGS_FILE))
    {
      Serial.println("Reading existing settings from file.");
      nodeUUID = settingsFile.readStringUntil('\n');
      nodeName = settingsFile.readStringUntil('\n');
      currentEndPoint = settingsFile.readStringUntil('\n');
      currentUpdateRate = settingsFile.readStringUntil('\n').toInt();
      nodeLat = settingsFile.readStringUntil('\n');
      nodeLong = settingsFile.readStringUntil('\n');
      Serial.println("Read UUID: " + nodeUUID);
      Serial.println("Read Name: " + nodeName);
      Serial.println("Read EndPoint: " + currentEndPoint);
      Serial.println("Read UpdateRate: " + String(currentUpdateRate));
      Serial.println("Read Position: " + nodeLat + "," + nodeLong);
    }
  }
  settingsFile.close();
}

// -------------- Web functions -------------- //

// handler to setup initial values for status page
String handle_Status(AutoConnectAux& aux, PageArgument& args)
{
  AutoConnectText& title = aux.getElement<AutoConnectText>("header_title");
  AutoConnectText& reply = aux.getElement<AutoConnectText>("currentReply");
  AutoConnectText& endpoint = aux.getElement<AutoConnectText>("currentEndpoint");
  AutoConnectText& lastpost = aux.getElement<AutoConnectText>("lastPOSTreply");
  AutoConnectText& interval = aux.getElement<AutoConnectText>("currentUpdateRate");
  AutoConnectText& uptime = aux.getElement<AutoConnectText>("currentUpTime");

  title.value = "<h2>" + nodeName + " status<h2>";
  reply.value = currentJSONReply;
  endpoint.value = currentEndPoint;
  lastpost.value = lastPOSTreply;
  interval.value = String(currentUpdateRate);
  uptime.value = String(millis() / 1000);

  return String();
}

// handler to setup initial values for configuration page
String handle_Config(AutoConnectAux& aux, PageArgument& args)
{
  AutoConnectInput& name = aux.getElement<AutoConnectInput>("nameInput");
  AutoConnectInput& uuid = aux.getElement<AutoConnectInput>("uuidInput");
  AutoConnectInput& latitude = aux.getElement<AutoConnectInput>("latInput");
  AutoConnectInput& longitude = aux.getElement<AutoConnectInput>("longInput");
  AutoConnectInput& endpoint = aux.getElement<AutoConnectInput>("urlInput");
  AutoConnectInput& interval = aux.getElement<AutoConnectInput>("intervalInput");

  name.value = nodeName;
  uuid.value = nodeUUID;
  latitude.value = nodeLat;
  longitude.value = nodeLong;
  endpoint.value = currentEndPoint;
  interval.value = String(currentUpdateRate);
  
  return String();
}

// 404 page HTML
String RedirectPage()
{
  String html = "<!DOCTYPE html><html>\n";
  html +="<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0, user-scalable=no\">\n";
  html +="<title>Page not found</title></head>\n";
  html +="<style>html { font-family: Verdana; display: inline-block; margin: 0px auto; text-align: center;}</style>\n";
  html +="<body>\n";
  html +="<h1>404</h1>\n";
  html +="<p>This page does not exist.</p>\n";
  html +="<p><a href='/_ac'>Return to main page</a></p>\n";
  html +="</body>\n";
  html +="</html>\n";
  return html;
}

// handle 404
void handle_NotFound()
{
  server.send(404, "text/html", RedirectPage());
}

// save the new settings from config page
void handle_SaveSettings()
{
  // get args from server and save them to setting variables
  String newurl = server.arg("urlInput");
  currentEndPoint = newurl;

  unsigned long newinterval = server.arg("intervalInput").toInt();
  if(currentUpdateRate != newinterval)
  {
    currentUpdateRate = newinterval;

    // reset update interval
    if(delay_sensor_update.isExpired())
    {
      delay_sensor_update.restart();
    }
    delay_sensor_update.start(currentUpdateRate,AsyncDelay::MILLIS);
  }

  String newName = server.arg("nameInput");
  nodeName = newName;

  String newUUID = server.arg("uuidInput");
  nodeUUID = newUUID;

  String newLat = server.arg("latInput");
  nodeLat = newLat;

  String newLong = server.arg("longInput");
  nodeLong = newLong;

  // save settings to file
  saveSettings();

  Serial.println("Saved new end point URL as " + currentEndPoint);
  Serial.println("Saved new update rate to be " + String(currentUpdateRate) + " ms");
  Serial.println("Saved node name as " + nodeName);
  Serial.println("Saved UUID as " + nodeUUID);
  Serial.println("Saved location as " + nodeLat + " " + nodeLong);

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
      Serial.print("Unknown error at address 0x");
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

  char replyData[MAX_SENSOR_REPLY_LENGTH]= {0};
  char lastSpecifier = 0;
  String dataKey = "";
  String dataValue = "";
  uint8_t replyCharIter = 0;
  uint8_t replyCount = 0;

  // requst all data sensor module has to offer (with timeout)
  while(lastSpecifier != CH_TERMINATE)
  { 
    // check timeout
    if(replyCount >= DATA_TRANSMISSION_TIMEOUT)
    {
      Serial.println("Too many transmissions from module. Terminating!");
      break;
    }
    // start i2c transmission to module
    Wire.requestFrom(sensorAddr, MAX_SENSOR_REPLY_LENGTH);
    replyCount++;
    // read until end of transmission
    while (Wire.available())
    {
      // read each individual character
      char c = Wire.read();
      switch(c) {
        case CH_TERMINATE:
          lastSpecifier = c;
        break;

        case CH_IS_KEY:
          lastSpecifier = c;
          replyCharIter = 0;
          Serial.print("Reading key, ");
        break;

        case CH_IS_VALUE:
          lastSpecifier = c;
          replyCharIter = 0;
          Serial.print("Reading value, ");
        break;

        case CH_MORE:
          // terminate reply string
          replyData[replyCharIter] = 0;
          // print out reading to see what we got
          Serial.print("Parsed reading: ");
          Serial.println(replyData);
          // put the parsed reading in the right string and add data to JSON
          if(lastSpecifier == CH_IS_KEY)
          {
            dataKey = String(replyData);
          }
          else if(lastSpecifier == CH_IS_VALUE)
          {
            dataValue = String(replyData);
            // add the pair to the data object
            dataObj[dataKey] = dataValue;
          }
          else
          {
            Serial.println("Unknown reading, discarding.");
          }
          // clear the data buffer
          memset(replyData,0,sizeof(replyData));
          replyCharIter = 0;
        break;

        default:
          // append the character into the reply data array and increment replyCharIter
          replyData[replyCharIter++] = c;
        break;
      }
    }
  }
  Serial.println("Request complete. Total of " + String(replyCount) + " transmissions.");
}

// helper function to request data from all connected modules and create a JSON object
void fetchData()
{
  byte sensorCount = 0;

  // create JSON document
  StaticJsonDocument<MAX_JSON_REPLY> jsonDoc;
  currentJSONReply = "";
  nodeUUID.trim();
  nodeName.trim();
  nodeLat.trim();
  nodeLong.trim();
  jsonDoc["uuid"] = nodeUUID;
  jsonDoc["name"] = nodeName;
  jsonDoc["lat"] = nodeLat;
  jsonDoc["long"] = nodeLong;
  JsonObject dataObj = jsonDoc.createNestedObject("data");

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
  // init serial, I2C, SPIFFS and status LED
  pinMode(LED_BUILTIN, OUTPUT);
  Serial.begin(9600);
  Wire.begin();

  if(!SPIFFS.begin(true)){
      Serial.println("An Error has occurred while mounting SPIFFS. Rebooting.");
      ESP.restart();
  }
  Serial.println("SPIFFS mounted.");
  // load settings on boot
  loadSettings();

  // attach handlers
  server.on("/save_settings", handle_SaveSettings);

  // setup AutoConnect
  portalConfig.title = "Main Module v1.0";
  portalConfig.apid = "MainModule-" + String((uint32_t)(ESP.getEfuseMac() >> 32), HEX);
  portalConfig.apip = IPAddress(192,168,1,1);
  portalConfig.gateway = IPAddress(192,168,1,1);
  portalConfig.bootUri = AC_ONBOOTURI_ROOT;
  Portal.config(portalConfig);
  Portal.load(customPageJSON);
  Portal.home("/status");
  Portal.on("/status",handle_Status,AC_EXIT_AHEAD);
  Portal.on("/moduleconfig",handle_Config,AC_EXIT_AHEAD);
  Portal.onNotFound(handle_NotFound);

  // initialize networking via AutoConnect
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
    if((currentJSONReply != NULL || currentJSONReply != "") && (WiFi.status() != WL_IDLE_STATUS) && (WiFi.status() != WL_DISCONNECTED))
    {
      if(WiFi.getMode() == WIFI_MODE_STA)
        sendDataToEndpoint();
    }
    delay_sensor_update.restart();
  }
  asyncBlink(BLINK_TIME);
}
