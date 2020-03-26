#include <Arduino.h>
#include <Wire.h>
#include <SPIFFS.h>

#include <AsyncDelay.h>

#include <WiFi.h>
#include <HTTPClient.h>
#include <WebServer.h>
#include <AutoConnect.h>
#include <AutoConnectCredential.h>
#include <ArduinoJson.h>

#include "protocol.h"
#include "customPages.h" 

#define LED_TICKER 33
#define BUTTON_PIN 32
#define DEFAULT_UPDATE_INTERVAL 60000
#define LIVE_SENSOR_INTERVAL 1000
#define SETTINGS_FILE "/settings.txt"
#define DATA_TRANSMISSION_TIMEOUT 20 // arbitrary number
#define REBOOT_BUTTON_HOLD_DURATION 3000
#define FACTORY_RESET_BUTTON_HOLD_DURATION 10000



byte modules[MAX_SENSORS];      // array with address listings of connected sensor modules
AsyncDelay delay_sensor_update; // delay timer for asynchronous update interval
AsyncDelay delay_sensor_view; // 1 second delay for real time sensor viewing
bool sensorViewMode = false;

String currentJSONReply = "{\"data\":[\"N/A\":\"No sensors connected.\"]}"; // string to hold JSON object to be sent to endpoint
String lastPOSTreply = "N/A";        // string to save last POST status reply

// config vars set to default values
String nodeName = "MainModule";
String nodeUUID = "1234567890";
String nodeLat = "N/A";
String nodeLong = "N/A";
String currentEndPoint = "https://yourgisdb.com/apiforposting/";
String nodeLEDSetting = "On";
unsigned long currentUpdateRate = DEFAULT_UPDATE_INTERVAL;

WebServer server;           // HTTP server to serve web UI
AutoConnect Portal(server); // AutoConnect handler object
AutoConnectConfig portalConfig("MainModuleAP", "12345678");

// NOTE: the data for the custom pages are in the customPages.h header file
AutoConnectElement viewerHTML("viewerhtml", sensorViewerHTML, AC_Tag_None); // script and HTML for live sensor view


// -------------- Helper functions -------------- //

// helper function to write settings to file in SPIFFS.
void saveSettings()
{
  File settingsFile = SPIFFS.open(SETTINGS_FILE, FILE_WRITE);
  if (!settingsFile)
  {
    Serial.println("Failed to open file stream. Save failed.");
    return;
  }

  // trim to remove any unncessary whitespace
  nodeUUID.trim();
  nodeName.trim();
  currentEndPoint.trim();
  nodeLat.trim();
  nodeLong.trim();

  settingsFile.println(nodeUUID);
  settingsFile.println(nodeName);
  settingsFile.println(currentEndPoint);
  settingsFile.println(currentUpdateRate);
  settingsFile.println(nodeLat);
  settingsFile.println(nodeLong);
  settingsFile.println(nodeLEDSetting);
  Serial.println("Wrote existing settings to save file.");
  settingsFile.close();
}

// Delete all Wi-Fi credentials that have been saved with AutoConnect Library.
void deleteAllCredentials(void) {
  AutoConnectCredential credential;
  station_config_t config;
  uint8_t ent = credential.entries();

  while (ent--) {
    credential.load((int8_t) 0 , &config);
    credential.del((const char*)&config.ssid[0]);
  }
}

// Reset configuration in config page to default parameters.
void clearSettings(){
  Serial.println("REMOVING ALL SETTINGS..");
  WiFi.disconnect(true,true);
  for (int i = 0; i < 3 ; i++){
    digitalWrite(LED_TICKER,HIGH);
    delay(200);
    digitalWrite(LED_TICKER,LOW);
    delay(200);
  }
  deleteAllCredentials();         
  SPIFFS.remove(SETTINGS_FILE);
}

// helper function to make LED blink asynchronously	
// ms : Time for LED to lighten up (millisecond)	
// ms = 0 means checking whether the LED should be turned of or not.	
void asyncBlink(unsigned long ms = 0)	
{	
  static unsigned long stopTime = 0;	
  if (ms)	
  {	
    stopTime = millis() + ms;	
    digitalWrite(LED_TICKER, HIGH); 
  }	
  else	
  {	
    // Check whether is it time to turn off the LED.	
    if (millis() > stopTime)	
    {	
      digitalWrite(LED_TICKER, LOW);	
    }	
  }	
}

void checkButton(){
  static unsigned long pushedDownTime = NULL;
  if (pushedDownTime == NULL && digitalRead(BUTTON_PIN) == LOW){  //Button pressed
    pushedDownTime = millis();
    
   
  }else if (pushedDownTime != NULL && digitalRead(BUTTON_PIN) == HIGH ){                     //Button released
    unsigned int pressingDuration = millis() - pushedDownTime;
    
    if (pressingDuration > FACTORY_RESET_BUTTON_HOLD_DURATION){
      clearSettings();
      ESP.restart();
    }
    else if (pressingDuration > REBOOT_BUTTON_HOLD_DURATION){
      Serial.println("Restarting..");
      delay(3000);
      ESP.restart();
    }

    pushedDownTime = NULL;      
  }

  // indicate the led to let user know when to release button
  if (pushedDownTime != NULL  && digitalRead(BUTTON_PIN) == LOW){
    unsigned int pressingDuration = millis() - pushedDownTime;
      if (pressingDuration > REBOOT_BUTTON_HOLD_DURATION && pressingDuration < FACTORY_RESET_BUTTON_HOLD_DURATION){
        asyncBlink(FACTORY_RESET_BUTTON_HOLD_DURATION - pressingDuration);
      }
  }
}


// helper function to load settings from save file in SPIFFS.
void loadSettings()
{
  File settingsFile = SPIFFS.open(SETTINGS_FILE, FILE_READ);
  if (!SPIFFS.exists(SETTINGS_FILE) || !settingsFile) // settings file does not exist, set everything to default.
  {
    Serial.println("Settings file does not exist or could not be opened. Creating new setings file.");
    File newSettingsFile = SPIFFS.open(SETTINGS_FILE, FILE_WRITE);
    newSettingsFile.println(nodeUUID);
    newSettingsFile.println(nodeName);
    newSettingsFile.println(currentEndPoint);
    newSettingsFile.println(currentUpdateRate);
    newSettingsFile.println(nodeLat);
    newSettingsFile.println(nodeLong);
    newSettingsFile.println(nodeLEDSetting);
    Serial.println("Wrote default settings to file.");
    newSettingsFile.close();
  }
  else // read existing settings
  {
    if (SPIFFS.exists(SETTINGS_FILE))
    {
      Serial.println("Reading existing settings from file.");
      nodeUUID = settingsFile.readStringUntil('\n');
      nodeName = settingsFile.readStringUntil('\n');
      currentEndPoint = settingsFile.readStringUntil('\n');
      currentUpdateRate = settingsFile.readStringUntil('\n').toInt();
      nodeLat = settingsFile.readStringUntil('\n');
      nodeLong = settingsFile.readStringUntil('\n');
      nodeLEDSetting = settingsFile.readStringUntil('\n');

      // trim to remove any unncessary whitespace
      nodeUUID.trim();
      nodeName.trim();
      currentEndPoint.trim();
      nodeLat.trim();
      nodeLong.trim();
      nodeLEDSetting.trim();

      Serial.println("Read UUID: " + nodeUUID);
      Serial.println("Read Name: " + nodeName);
      Serial.println("Read EndPoint: " + currentEndPoint);
      Serial.println("Read UpdateRate: " + String(currentUpdateRate));
      Serial.println("Read Position: " + nodeLat + "," + nodeLong);
      Serial.println("Read LED Setting: " + nodeLEDSetting);

    }
  }
  settingsFile.close();
}

// -------------- Web functions -------------- //

// handler to setup initial values for status page
String handle_Status(AutoConnectAux &aux, PageArgument &args)
{
  AutoConnectText &title = aux.getElement<AutoConnectText>("header_title");
  AutoConnectText &reply = aux.getElement<AutoConnectText>("currentReply");
  AutoConnectText &endpoint = aux.getElement<AutoConnectText>("currentEndpoint");
  AutoConnectText &lastpost = aux.getElement<AutoConnectText>("lastPOSTreply");
  AutoConnectText &interval = aux.getElement<AutoConnectText>("currentUpdateRate");
  AutoConnectText &uptime = aux.getElement<AutoConnectText>("currentUpTime");

  title.value = "<h2>" + nodeName + " status<h2>";
  reply.value = currentJSONReply;
  endpoint.value = currentEndPoint;
  lastpost.value = lastPOSTreply;
  interval.value = String(currentUpdateRate);
  uptime.value = String(millis() / 1000);

  return String();
}

// handler to setup initial values for configuration page
String handle_Config(AutoConnectAux &aux, PageArgument &args)
{
  AutoConnectInput &name = aux.getElement<AutoConnectInput>("nameInput");
  AutoConnectInput &uuid = aux.getElement<AutoConnectInput>("uuidInput");
  AutoConnectInput &latitude = aux.getElement<AutoConnectInput>("latInput");
  AutoConnectInput &longitude = aux.getElement<AutoConnectInput>("longInput");
  AutoConnectInput &endpoint = aux.getElement<AutoConnectInput>("urlInput");
  AutoConnectInput &interval = aux.getElement<AutoConnectInput>("intervalInput");
  AutoConnectRadio &ledSetting = aux.getElement<AutoConnectRadio>("ledSettingRadio");

  name.value = nodeName;
  uuid.value = nodeUUID;
  latitude.value = nodeLat;
  longitude.value = nodeLong;
  endpoint.value = currentEndPoint;
  interval.value = String(currentUpdateRate);
  if (nodeLEDSetting == "On"){  
    ledSetting.checked = 1;
  }else{
    ledSetting.checked = 2;
  }

  return String();
}

// Handle custom sensor viewer page
String handle_sensorViewer(AutoConnectAux &aux, PageArgument &args)
{
  sensorViewMode = false;
  return String();
}

// used for updating live sensor view page
void handle_getSensorJSON()
{
  sensorViewMode = true;
  server.send(200, "application/json", currentJSONReply);
}

// handle 404
void handle_NotFound()
{
  server.send(404, "text/html", notFoundPage);
}

// save the new settings from config page
void handle_SaveSettings()
{
  // get args from server and save them to setting variables
  String newurl = server.arg("urlInput");
  currentEndPoint = newurl;

  unsigned long newinterval = server.arg("intervalInput").toInt();
  if (currentUpdateRate != newinterval)
  {
    currentUpdateRate = newinterval;

    // reset update interval
    if (delay_sensor_update.isExpired())
    {
      delay_sensor_update.restart();
    }
    delay_sensor_update.start(currentUpdateRate, AsyncDelay::MILLIS);
  }

  String newName = server.arg("nameInput");
  nodeName = newName;

  String newUUID = server.arg("uuidInput");
  nodeUUID = newUUID;

  String newLat = server.arg("latInput");
  nodeLat = newLat;

  String newLong = server.arg("longInput");
  nodeLong = newLong;

  String newNodeLEDSetting = server.arg("ledSettingRadio");
  nodeLEDSetting = newNodeLEDSetting;

  // save settings to file
  saveSettings();

  Serial.println("Saved new end point URL as " + currentEndPoint);
  Serial.println("Saved new update rate to be " + String(currentUpdateRate) + " ms");
  Serial.println("Saved node name as " + nodeName);
  Serial.println("Saved UUID as " + nodeUUID);
  Serial.println("Saved location as " + nodeLat + " " + nodeLong);
  Serial.println("Saved LED setting as " + nodeLEDSetting);


  // redirect back to main page after saving
  server.sendHeader("Location", "/status", true);
  server.send(302, "text/plain", "");
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

  if (httpResponseCode > 0)
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
  for (byte i = 0; i < MAX_SENSORS; i++) // fill modules array with zeroes
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

    if (nDevices > MAX_SENSORS) // maximum number of sensors allowed reached
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

  bool endTransmission = false;

  char replyData[MAX_SENSOR_REPLY_LENGTH] = {0};
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
    endTransmission = false;
    replyCount++;
    // read until end of transmission
    while (Wire.available() && !endTransmission)
    {
      // read each individual character
      char c = Wire.read();
      switch(c) {
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
          lastSpecifier = c;
          endTransmission = true;
          break;

        case CH_TERMINATE:
          // same as CH_more
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
          lastSpecifier = c;
          endTransmission = true;
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
  jsonDoc["lat"] = nodeLat.toDouble();
  jsonDoc["long"] = nodeLong.toDouble();
  JsonObject dataObj = jsonDoc.createNestedObject("data");

  // obtain information from sensors
  Serial.println("Gathering sensor data.");
  for (int i = 0; i < MAX_SENSORS; i++)
  {
    if (modules[i] != 0)
    {
      getSensorModuleReading(modules[i], dataObj);
      sensorCount++;
    }
  }
  Serial.print("Read from ");
  Serial.print(sensorCount);
  Serial.println(" sensors");

  // serialize JSON reply string
  serializeJson(jsonDoc, currentJSONReply);
  // print out JSON output (for debug purposes)
  Serial.println("Serialized data string:");
  Serial.println(currentJSONReply);
  Serial.println("Saved JSON to string.\n");
}

// -------------- Arduino framework main code -------------- //

void setup()
{
  // underclock from 240 MHz to 80 MHz for power saving
  setCpuFrequencyMhz(80);

  // initialize serial, I2C and SPIFFS
  Serial.begin(9600);
  Wire.begin();

  Serial.println("Running at " + String(getCpuFrequencyMhz()) + " MHz");

  // setup SenseStack IO pins
  pinMode(BUTTON_PIN, INPUT_PULLUP);
  pinMode(LED_TICKER, OUTPUT);

  // turn on LED on to indicate booting process
  digitalWrite(LED_TICKER, HIGH);

  if (!SPIFFS.begin(true))
  {
    Serial.println("An Error has occurred while mounting SPIFFS. Rebooting.");
    ESP.restart();
  }
  Serial.println("SPIFFS mounted.");

  // load settings on boot
  loadSettings();

  // attach handlers for HTTPserver
  server.on("/save_settings", handle_SaveSettings);
  server.on("/getJSON", handle_getSensorJSON);

  // setup AutoConnect with a configuration
  portalConfig.title = "Main Module v1.0";
  portalConfig.apid = "MainModule-" + String((uint32_t)(ESP.getEfuseMac() >> 32), HEX);
  portalConfig.apip = IPAddress(192, 168, 1, 1);
  portalConfig.gateway = IPAddress(192, 168, 1, 1);
  portalConfig.bootUri = AC_ONBOOTURI_ROOT;
  portalConfig.ticker = true;
  portalConfig.tickerPort = LED_TICKER;
  portalConfig.tickerOn = HIGH;
  Portal.config(portalConfig);

  // load custom page JSON and build pages into memory
  if(!Portal.load(customPageJSON))
  {
    Serial.println("HTML page generation failed, rebooting.");
    ESP.restart();
  }
  // inject live view HTML into sensor viewer page
  AutoConnectAux* viewerPage = Portal.aux("/sensorviewer");
  viewerPage->add(viewerHTML);

  // set URLs and handlers
  Portal.home("/status");
  Portal.on("/status", handle_Status, AC_EXIT_AHEAD);
  Portal.on("/moduleconfig", handle_Config, AC_EXIT_AHEAD);
  Portal.on("/sensorviewer", handle_sensorViewer, AC_EXIT_LATER);
  Portal.onNotFound(handle_NotFound);

  // initialize networking via AutoConnect
  if (Portal.begin())
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
  delay_sensor_update.start(currentUpdateRate, AsyncDelay::MILLIS);
  delay_sensor_view.start(LIVE_SENSOR_INTERVAL, AsyncDelay::MILLIS);

  // Turn off LED to indicate finished of booting process
  digitalWrite(LED_TICKER, LOW);
}

void loop()
{
  // handle web UI
  server.handleClient();
  Portal.handleRequest();

  // handle button press
  checkButton();

  // handle LED state
  asyncBlink();

  // if we are viewing the live sensor view page
  if(delay_sensor_view.isExpired() && sensorViewMode == true)
  {
    scanDevices();
    fetchData();
    delay_sensor_view.restart();
    return;
  }
  // this will be set to true while viewing the live sensor view page
  sensorViewMode = false;

  // data update loop
  if (delay_sensor_update.isExpired())
  {
    scanDevices();
    fetchData();
    if ((currentJSONReply != NULL || currentJSONReply != "") && (WiFi.status() != WL_IDLE_STATUS) && (WiFi.status() != WL_DISCONNECTED))
    {
      if (WiFi.getMode() == WIFI_MODE_STA){
        sendDataToEndpoint();

        if (nodeLEDSetting == "On"){
          asyncBlink(200);
        }
      }
    }
    delay_sensor_update.restart();
  }
}
