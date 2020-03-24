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

// Live sensor viewing page
const char customPageSensorViewer[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML>
<html>

<head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css"
        integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
    <style>
        html {
            font-family: Arial;
            display: inline-block;
            margin: 0px auto;
            text-align: center;
        }

        h2 {
            font-size: 3.0rem;
        }

        p {
            font-size: 3.0rem;
        }

        .units {
            font-size: 1.2rem;
        }

        .sensor-label {
            font-size: 1.5rem;
            vertical-align: middle;
            padding-bottom: 15px;
        }
    </style>
</head>

<body>
    <h2>Live connected sensor viewer</h2>
    <div id=sensorElement>
        <p>Getting sensor information...</p>
    </div>
</body>

<footer>
    <p style="padding-top:15px;text-align:center">
        <a href="/_ac"><img
                src="data:image/png;base64,iVBORw0KGgoAAAANSUhEUgAAABgAAAAYCAYAAADgdz34AAAC2klEQVRIS61VvWsUQRSfmU2pon9BUIkQUaKFaCBKgooSb2d3NSSFKbQR/KrEIiIKBiGF2CgRxEpjQNHs7mwOUcghwUQ7g58IsbGxEBWsb2f8zR177s3t3S2cA8ftzPu993vzvoaSnMu2vRKlaqgKp74Q/tE8qjQPyHGcrUrRjwlWShmDbFMURd/a6TcQwNiYUmpFCPElUebcuQ2vz6aNATMVReHEPwzfSSntDcNwNo2rI+DcvQzhpAbA40VKyV0p1Q9snzBG1qYVcYufXV1sREraDcxpyHdXgkfpRBj6Uwm2RsC5dxxmZ9pdOY9cKTISRcHTCmGiUCh4fYyplTwG2mAUbtMTBMHXOgK9QfyXEZr+TkgQ1oUwDA40hEgfIAfj+HuQRaBzAs9eKyUZ5Htx+T3ZODKG8DzOJMANhmGomJVMXPll+hx9UUAlzZrJJ4QNCDG3VEfguu7mcpmcB/gkBOtShhQhchAlu5jlLUgc9ENgyP5gf9+y6LTv+58p5zySkgwzLNOIGc8sEoT1Lc53NMlbCQQuvMxeCME1NNPVVkmH/i3IzzXDtCSA0qQQwZWOCJDY50jsQRjJmkslEOxvTcDRO6zPxOh5xZglKkYLhWM9jMVnkIsTyMT6NBj7IbOCEjm6HxNVVTo2WXqEWJZ1T8rytB6GxizyDkPhWVpBqfiXUtbo/HywYJSpA9kMamNNPZ71R9Hcm+TMHHZNGw3EuraXEUldbfvw25UdOjqOt+JhMwJd7+jSTpZaEiIcaCDwPK83jtWnTkwnunFMtxeL/ge9r4XItt1RNNaj/0GAcV2bR3U5sG3nEh6M61US+Qrfd9Bs31GGulI2GOS/8dgcQZV1w+ApjIxB7TDwF9GcNzJzoA+rD0/8HvPnXQJCt2qFCwbBTfRI7UyXumWVt+HJ9NO4XI++bdsb0YyrqXmlh+AWOLHaLqS5CLQR5EggR3YlcVS9gKeH2hnX8r8Kmi1CAsl36QAAAABJRU5ErkJggg==" border="0" title="AutoConnect menu" alt="AutoConnect menu" />
            </a>
    </p>

</footer>


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
</html>
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