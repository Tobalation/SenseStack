#include <Arduino.h>

// AutoconnectElements for viewing 
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
    },
    {
        "title": "Live sensor viewer",
        "uri": "/sensorviewer",
        "menu": true,
        "element": [
            {
                "name": "viewcss",
                "type": "ACStyle",
                "value": "p { font-size: 32px; }"
            },
            {
                "name": "header_title",
                "type": "ACText",
                "style": "color:#06697c;border-bottom:5px solid black",
                "value": "<h2>Currently connected sensors<h2>"
            }
        ]
    }
]
)raw";

// Live sensor viewing HTML and JS
const char sensorViewerHTML[] PROGMEM = R"rawliteral(
<div id=sensorElement>
    <p>Getting sensor information...</p>
</div>
<script>
    loadData();                  //Load the data for first time
    setInterval(loadData, 1000); //Reload the data every X milliseconds.
    //Load the JSON data from API, and then replace the HTML element
    function loadData(){
        var xhttp = new XMLHttpRequest();
        xhttp.onreadystatechange = function () {
            if (this.readyState == 4 && this.status == 200) {
                var sensorElement = document.getElementById("sensorElement");
                sensorElement.textContent = "";     //Clear all elements in sensorElement div
                var sensorsData = JSON.parse(this.response).data;     //Parse the response to JSON object
                if (Object.keys(sensorsData).length == 0 ) {                          //object is empty
                    sensorElement.innerHTML = "<p> No sensors connected. </p>"
                    return;
                }
                for (sensorName of Object.keys(sensorsData)) {                        //iterate through each element and create paragraph for each sensor 
                    sensorElement.innerHTML += "<p><span class=\"sensor-label\">" +
                        sensorName + "</span> <span id=\"sensor-value\">" +
                        sensorsData[sensorName] + " </span> <sup class=\"units\">" + ' ' + " </sup>  </p>";
                }
            }
        };
        xhttp.open("GET", "/getJSON", true);      //request JSON data from URI/getJSON
        xhttp.send();
    }
</script>
)rawliteral";

// 404 page
const char notFoundPage[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
  <head>
    <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
    <title>
        Page not found
    </title>
  </head>
  <style>
    html { 
        font-family: Verdana; 
        display: inline-block; 
        margin: 0px auto; 
        text-align: center;
    }
  </style>
  <body>
  <h1>404</h1>
  <p>This page does not exist.</p>
  <p><a href='/_ac'>Return to main page</a></p>
  </body>
</html>
)rawliteral";