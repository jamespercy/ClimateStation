/*********
  based on project details at https://randomnerdtutorials.com/esp8266-dht11dht22-temperature-and-humidity-web-server-with-arduino-ide/
*********/

// Import required libraries
#include <Adafruit_Sensor.h>
#include <Arduino.h>
#include <DHT.h>
#include <EEPROM.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>he
#include <ESPAsyncWebServer.h>
#include <Hash.h>
#include <WiFiClientSecure.h>
#include <string.h> 

// Replace with your network credentials
const char* ssid = "THE SOCKS ATTACK";
const char* password = "sockattack";
const char* apiSecret = "1e98f6ebb39e0720c4e95af7b436502f53d85f2becf8b5d96e1e62083890fbfab5a4fa05d8bbaa212b8d6cdc3aa10953d7a2d68958d1238ca57f54ff";

const char* host = "us-central1-climate-station.cloudfunctions.net";
const char* deviceVersion = "ClimateStation-2016-06-06";
const int httpsPort = 443;

//Fan controll definitions

#define FanIA 5 //fan conrol input A blue
#define FanIB 4 //fan conrol input B yellow

#define Fan_PWM FanIA //fan PWM speed
#define Fan_DIR FanIB //fan direction
// the actual values for "fast" and "slow" depend on the motor
#define PWM_SLOW 50 // arbitrary slow speed PWM duty cycle
#define PWM_FAST 200 // arbitrary fast speed PWM duty cycle
#define PWM_FULL 255 // 255 is the new over 9000

const unsigned int FLASH_STOREAGE_BYTES = 512;

//Environment control variables
const unsigned int DEFAULT_MAX_HUMIDITY = 99; //time to exchange some air
const unsigned int DEFAULT_MIN_HUMIDITY = 90; //getting a bit dry, 
const unsigned int DEFAULT_MAX_TEMP = 25; //a good heat
const unsigned int DEFAULT_MIN_TEMP = 20; //getting chilly

const unsigned int TEMP_MAX_LOCATION = 0;
const unsigned int TEMP_MIN_LOCATION = 1;
const unsigned int HEATING_ACTIVE_LOCATION = 4;

const unsigned int HUM_MAX_LOCATION = 2;
const unsigned int HUM_MIN_LOCATION = 3;
const unsigned int FAN_ACTIVE_LOCATION = 5;

const char* HEATING_ON_COLOUR = "#ffb300";
const char* HEATING_OFF_COLOUR = "#000000";
const char* HEATING_INACTIVE_COLOUR = "#5b8080";

const char* FAN_ON_COLOUR = "#d0c9f5";
const char* FAN_OFF_COLOUR = "#000000";
const char* FAN_INACTIVE_COLOUR = "#5b8080";

int unsigned currentFanSpeed = 0;
bool fanIsActive = true;

bool heaterIsHeating = false;
bool heatingIsActive = true;

// relay
#define RELAY_PIN 13  // Digital pin connected to the DHT sensor

//DHT sensors
#define S1_DHTPIN 14  // Digital pin connected to the DHT sensor
#define S2_DHTPIN 12  // Digital pin connected to the DHT sensor
#define DHTTYPE    DHT22     // DHT 22 (AM2302)

DHT dht1(S1_DHTPIN, DHTTYPE);
DHT dht2(S2_DHTPIN, DHTTYPE);

//functions
void setupWebServer();
void getMeasurements();
void setupFan();
void setupHeating();
void stopFan();
void fanSpeed(int speed);
String fanColour();
void heaterOn(bool shouldTurnOn);
String heatColour();
void registerStartup();
void postRequest(String url, char* payload);
void reportMeasurement(String sensorId, String measurementType, float measurement);
void(* resetFunc) (void) = 0;

int getMinTemp();
int getMaxTemp();
bool isHeatActive();
void setMinTemp(int minTemp);
void setMaxTemp(int maxTemp);
void setHeatActive(bool heaterOn);

int getMinHum();
int getMaxHum();
bool isFanActive();
void setMinHum(int minHum);
void setMaxHum(int maxHum);
void setFanActive(bool fanOn);

// current temperature & humidity, updated in loop()
float t1 = 0.0;
float t2 = 0.0;
float h1 = 0.0;
float h2 = 0.0;

// Create AsyncWebServer object on port 80
AsyncWebServer server(80);

//secure http client
  WiFiClientSecure client;

// Generally, you should use "unsigned long" for variables that hold time
// The value will quickly become too large for an int to store
unsigned long previousMillis = 0;    // will store last time DHT was updated

// Updates DHT readings every 10 seconds
const long interval = 10000;  

unsigned long previousReport = 0;    // will store last time measurements were reported
const long reportInterval = 60000; 

const long msInADay = 1000 * 60 * 60 * 24;  
unsigned long msSinceLastReset = 0;

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE HTML><html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <link href="favicon.ico" rel="icon" type="image/x-icon" />
  <link rel="stylesheet" href="https://use.fontawesome.com/releases/v5.7.2/css/all.css" integrity="sha384-fnmOCqbTlWIlj8LyTjo7mOUStjsKC4pOpQbqyi7RrhN7udi9RwhKkMHpvLbHG9Sr" crossorigin="anonymous">
  <style>
   html {
     font-family: Arial;
     display: inline-block;
     margin: 0px auto;
     text-align: center;
    }
    h2 { font-size: 3.0rem; }
    p { font-size: 2.8rem; }
    i { font-size: 2.8rem; }
    .units { font-size: 1.2rem; }
    .labels{
      font-size: 1.0rem;
      vertical-align:middle;
      padding-bottom: 15px;
    }
    small { font-size: 0.6rem; }
    .grid-container {
        display: grid;
        grid-template-columns: auto 60px 60px 60px 60px auto;
      }
      .temp-range {
        grid-column: 2;
        grid-row: 1 / span 2;
      }
     .heat {
        cursor: pointer;
        grid-column: 3;
        grid-row: 1 / span 1;
      }
     .fan {
        cursor: pointer;
        grid-column: 4;
        grid-row: 1 / span 1;
      }
      .hum-range {
        grid-column: 5;
        grid-row: 1 / span 2;
      }
      
      .internal {
        border-radius: 25px;
        border: 2px solid #988df7;
      }
      .external {
        border-radius: 25px;
        border: 2px dashed #32a852;
      }
      .settings {
        border-radius: 25px;
        border: 2px solid #000000;
      }
  </style>
</head>
<body>
  <div class="internal">
  <h3>Internal (S1)</h3>
  <p>
    <div class="center-me">
      <div class="grid-container" >
        <div class="temp-range">
          <div><span id="minTempRange">%MIN_TEMP%</span>&deg;c</div>
          <div><span id="maxTempRange">%MAX_TEMP%</span>&deg;c</div>
        </div>
        <i id="heating" class="fas fa-fire-alt heat" style="color:%HEATING_COLOUR%;"></i>
        <i id="fan" class="fas fa-wind fan" style="color:%FAN_COLOUR%;"></i>
        <div class="hum-range">
          <div><span id="minHumRange">%MIN_HUM%</span>&#37;</div>
          <div><span id="maxHumRange">%MAX_HUM%</span>&#37;</div>
        </div>
      </div>
    </div>
  </p>
  <p>
      <span>
    <i class="fas fa-thermometer-half" style="color:#059e8a;"></i> 
    <span id="temperature-1"> %TEMPERATURE1%</span>
    <sup class="units">&deg;C</sup>
    </span>
    <span>
      <i class="fas fa-tint" style="color:#00add6;"></i> 
      <span id="humidity-1"> %HUMIDITY1%</span>
      <sup class="units">&#37;</sup>
    </span>
  </p>
  </div>
  <div class="external">
    <h3>External (S2)</h3>
  <p>
      <span>
    <i class="fas fa-thermometer-half" style="color:#059e8a;"></i> 
    <span id="temperature-2">%TEMPERATURE2%</span>
    <sup class="units">&deg;C</sup>
    </span>
      <span>
    <i class="fas fa-tint" style="color:#00add6;"></i> 
    <span id="humidity-2">%HUMIDITY2%</span>
    <sup class="units">&#37;</sup>
    </span>
  </p>
  </div>
  <div class="settings">
    <p>
    <i id="settings" class="fas fa-cogs" style="color:#5b8080;"></i>
      <form id="settings-form" style="display:none">
      <div style="border: 1px dashed green;">
        <label for="minTemp">Min temperature:</label><br>
        <input type="number" id="minTemp" name="minTemp" value="%MIN_TEMP%" oninput="this.value = Math.abs(Math.round(this.value));" min="15" max="30"  size="10"><br>
        <label for="maxTemp">Max temperature:</label><br>
        <input type="number" id="maxTemp" name="maxTemp" value="%MAX_TEMP%" oninput="this.value = Math.abs(Math.round(this.value));" min="15" max="30"  size="10"><br>
        <label for="minHum">Min humidity:</label><br>
        <input type="number" id="minHum" name="minHum" value="%MIN_HUM%" oninput="this.value = Math.abs(Math.round(this.value));" min="50" max="99"  size="10"><br>
        <label for="maxHum">Max humidity:</label><br>
        <input type="number" id="maxHum" name="maxHum" value="%MAX_HUM%" oninput="this.value = Math.abs(Math.round(this.value));" min="50" max="99"  size="10"><br>
        <br>
        <button id="update">Update</button><button id="cancel">Cancel</button>
        </div>
      </form>
    </p>
  </div>
</body>
<script>

document.getElementById('heating').addEventListener('click', function (event) {
  event.preventDefault();
  fetch('/toggle?heater=true');
}, false);

document.getElementById('fan').addEventListener('click', function (event) {
  event.preventDefault();
  fetch('/toggle?fan=true');
}, false);
  
document.getElementById('settings').addEventListener('click', function (event) {
  event.preventDefault();
  document.getElementById('settings').style.display = 'none';
  document.getElementById('settings-form').style.display = '';
}, false);

document.getElementById('update').addEventListener('click', function (event) {
  event.preventDefault();
  let minTemp = document.getElementById('minTemp').value;
  let maxTemp = document.getElementById('maxTemp').value;
  let minHum = document.getElementById('minHum').value;
  let maxHum = document.getElementById('maxHum').value;
  fetch(`/update-settings?minTemp=${minTemp}&maxTemp=${maxTemp}&minHum=${minHum}&maxHum=${maxHum}`).then((response) => {
    if (response.status == 200) {
       document.getElementById('minTempRange').textContent = minTemp;
       document.getElementById('maxTempRange').textContent = maxTemp;
       document.getElementById('minHumRange').textContent = minHum;
       document.getElementById('maxHumRange').textContent = maxHum;
    }
   });


  
  document.getElementById('settings').style.display = '';
  document.getElementById('settings-form').style.display = 'none';

}, false);

document.getElementById('cancel').addEventListener('click', function (event) {
  event.preventDefault()
  document.getElementById('settings').style.display = '';
  document.getElementById('settings-form').style.display = 'none';

}, false);

let getSensor = (sensor) => {
  fetch('/sensor?sensor=' + sensor)
        .then(response => response.text())
        .then(sendorData => {
                const parts = sendorData.split('|');
                let id = "humidity-"+sensor
                console.log('applying ' + parts[0] + ' to ' + id);
                document.getElementById(id).innerHTML = parts[0];
                
                id = "temperature-"+sensor
                console.log('applying ' + parts[1] + ' to ' + id);
                document.getElementById(id).innerHTML = parts[1];
          });
  };

let getEnv = () => {
  fetch('/env')
        .then(response => response.text())
        .then(envData => {
                const parts = envData.split('|');
                let id = "heating";
                console.log('applying ' + parts[0] + ' to ' + id);
                document.getElementById(id).style.color = parts[0];
          
                id = "fan";
                console.log('applying ' + parts[1] + ' to ' + id);
                document.getElementById(id).style.color = parts[1];
         });
  };


setInterval(getSensor, 10000, '1' ) ;
setInterval(getSensor, 10000, '2' ) ;
setInterval(getEnv, 10000) ;

</script>
</html>)rawliteral";

// Replaces placeholder with DHT values
String processor(const String& var){
  if(var == "TEMPERATURE1"){
    return String(t1);
  }  else if(var == "HUMIDITY1"){
    return String(h1);
  } else if(var == "TEMPERATURE2"){
    return String(t2);
  } else if(var == "HUMIDITY2"){
    return String(h2);
  } else if(var == "FAN_COLOUR"){
    return fanColour();
  } else if(var == "HEATING_COLOUR"){
    return heatColour();
  } else if(var == "MIN_TEMP"){
    return String(getMinTemp());
  } else if(var == "MAX_TEMP"){
    return String(getMaxTemp());
  } else if(var == "MIN_HUM"){
    return String(getMinHum());
  } else if(var == "MAX_HUM"){
    return String(getMaxHum());
  }
  else
  return String();
}

void setup(){
  // Serial port for debugging purposes
  Serial.begin(115200);

  Serial.println("\n SETTING UP \n");
      
  EEPROM.begin(FLASH_STOREAGE_BYTES);
    
  dht1.begin();
  dht2.begin();

  setupFan();
  setupHeating();
  
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println(".");
  }

  //secure client
//  Serial.printf("Using fingerprint '%s'\n", fingerprint);
//  client.setFingerprint(fingerprint);
  client.setInsecure(); //connect to HTTPS but don't validate certificate, fingerprints keep changing
  
  // Print ESP8266 Local IP Address
  Serial.println(WiFi.localIP());
  
  // Route for root / web page
  setupWebServer();

  registerStartup();

  // Start server
  server.begin();
  EEPROM.commit(); //stores any setup values
}
 
void loop(){  
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    // save the last time you updated the DHT values
    previousMillis = currentMillis;
    getMeasurements();
  }

  //reset once a day because memory leaks (and I prefer to focus my time elsewehere)
  if (msSinceLastReset > msInADay) {
     Serial.println("resetting");
     resetFunc();
  }
}

void getMeasurements(){
    unsigned long currentMillis = millis();
    
    bool shouldReport = currentMillis - previousReport >= reportInterval;
    if (shouldReport) {
      previousReport = currentMillis;
    }
    
    // Read temperature as Celsius (the default)
    float newT1 = dht1.readTemperature();
    // Read temperature as Fahrenheit (isFahrenheit = true)
    //float newT = dht.readTemperature(true);
    // if temperature read failed, don't change t value
    if (isnan(newT1)) {
      Serial.println("Failed to read temp from DHT1 sensor!");
    }
    else {
      t1 = newT1;
      int maxTemp = getMaxTemp();
      int minTemp = getMinTemp();
      
      Serial.println("Checking temp is between " + String(minTemp) + "c and " + String(maxTemp) + "c");
      if (!heatingIsActive) {
        heaterOn(false);
      } else if (t1 >= maxTemp) {
        heaterOn(false);
      } else if (t1 <= minTemp) {
        heaterOn(true);
      }
      
      Serial.println("Sensor 1 temperature " + String(t1) + "c");
      if (shouldReport) {
        reportMeasurement("1", "temperature", t1);
      }
    }

    // Read Humidity
    float newH1 = dht1.readHumidity();
    // if humidity read failed, don't change h value 
    if (isnan(newH1)) {
      Serial.println("Failed to read humidity from DHT1 sensor!");
    }
    else {
      h1 = newH1;
      int maxHum = getMaxHum();
      int minHum = getMinHum();
      
      Serial.println("Checking humidity is between " + String(minHum) + "% and " + String(maxHum) + "%");

      if (!fanIsActive) {
        stopFan();
      } else if (h1 >= maxHum) {
        fanSpeed(PWM_FULL);
      } else if (h1 <= minHum) {
        stopFan();
      }
      Serial.println("Sensor 1 humidity " + String(h1) + "%");
      if (shouldReport) {
        reportMeasurement("1", "humidity", h1);
      }
    }

     float newT2 = dht2.readTemperature();
         if (isnan(newT2)) {
      Serial.println("Failed to read temp from DHT2 sensor!");
    }
    else {
      t2 = newT2;
      Serial.println("Sensor 2 temperature " + String(t2) + "c");
      if (shouldReport) {
        reportMeasurement("2", "temperature", t2);
      }
    }
    
    float newH2 = dht2.readHumidity();
    // if humidity read failed, don't change h value 
    if (isnan(newH2)) {
      Serial.println("Failed to read humidity from DHT2 sensor!");
    }
    else {
      h2 = newH2;
      Serial.println("Sensor 2 humidity " + String(h2) + "%");
      if (shouldReport) {
        reportMeasurement("2", "humidity", h2);
      }
    }

    if (shouldReport) {
      reportMeasurement("fan", "speed", currentFanSpeed);
      reportMeasurement("heater", "on", heaterIsHeating ? 1 : 0);
    }
}

void registerStartup() {
  String url = "/register";
  
  char content[200];
  strcpy(content, "{\"apiSecret\":\"");
  strcat(content, apiSecret);
  strcat(content, "\", \"localIpAddress\": \"");
  strcat(content, WiFi.localIP().toString().c_str());
  strcat(content, "\"}");

  postRequest(url, content);
 
}

void reportMeasurement(String sensorId, String measurementType, float measurement) {
  String url = "/measurement";
  
    char mString[6]; 
    gcvt(measurement, 4, mString);
  
  char content[500];
  strcpy(content, "{\"apiSecret\":\"");
  strcat(content, apiSecret);
  strcat(content, "\", \"localIpAddress\": \"");
  strcat(content, WiFi.localIP().toString().c_str());
  strcat(content, "\", \"sensorId\": \"");
  strcat(content, sensorId.c_str());
  strcat(content, "\", \"measurementType\": \"");
  strcat(content, measurementType.c_str());
  strcat(content, "\", \"measurement\": ");
  strcat(content, mString);
  strcat(content, "}");

  postRequest(url, content);
}

void postRequest(String url, char* content) {
       // Use WiFiClientSecure class to create TLS connection

  if (!client.connect(host, httpsPort)) {
    Serial.println("connection failed");
    return;
  }


  int contentLength = strlen(content);

  client.print("POST " + url + " HTTP/1.1\r\n" +
               "Host: " + host + "\r\n" +
               "User-Agent: " + deviceVersion + "\r\n" + 
               "Connection: close\r\n" + 
               "Accept: */*\r\n" +
               "Content-Type: application/json\r\n" + 
               "Content-Length: " + String(contentLength) + "\r\n" + 
               "\r\n" +
               content +
               "\r\n");

  while (client.connected()) {
    String line = client.readStringUntil('\n');
    if (line == "\r") {
      break;
    }
  }
  String line = client.readStringUntil('\n');
  Serial.println(line);
}

void setupFan() {
  //ensure fan paramewters are set
  int maxHum = getMaxHum();
  if (maxHum == 255|| maxHum == 0) {
    Serial.println("Storing max humidity value at " + String(HUM_MAX_LOCATION) + " with default value " + String(DEFAULT_MAX_HUMIDITY));
    setMaxHum(DEFAULT_MAX_HUMIDITY);
  } else {
    Serial.println("Humidity max read at " + String(HUM_MAX_LOCATION) + " is " + String(maxHum));
  }

  int minHum = getMinHum();
  if (minHum == 255 || minHum == 0) {
    setMinHum(DEFAULT_MIN_HUMIDITY);
  } else {
    Serial.println("Humidity min read at " + String(HUM_MIN_LOCATION) + " is " + String(minHum));
  }

  fanIsActive = isFanActive();
  Serial.println("Fan is active " + String(fanIsActive));
  
  pinMode( Fan_PWM, OUTPUT );
  pinMode( Fan_DIR, OUTPUT );
  stopFan();
}


void setupHeating() {
    //relay
    pinMode(RELAY_PIN, OUTPUT);
  
    //ensure heating paramewters are set
  int maxTemp = getMaxTemp();
  if (maxTemp == 255|| maxTemp == 0) {
    Serial.println("Storing max temp value at " + String(TEMP_MAX_LOCATION) + " with default value " + String(DEFAULT_MAX_TEMP));
    setMaxTemp(DEFAULT_MAX_TEMP);
  } else {
    Serial.println("Temp max read at " + String(TEMP_MAX_LOCATION) + " is " + String(maxTemp));
  }

  int minTemp = getMinTemp();
  if (minTemp == 255 || minTemp == 0) {
    Serial.println("Storing min temp value at " + String(TEMP_MAX_LOCATION) + " with default value " + String(DEFAULT_MAX_TEMP));
    setMinTemp(DEFAULT_MIN_TEMP);
  } else {
    Serial.println("Temp min read at " + String(TEMP_MIN_LOCATION) + " is " + String(minTemp));
  }

  heatingIsActive = isHeatActive();
  Serial.println("Heating is active " + String(heatingIsActive));
  
}

void stopFan()
{
  currentFanSpeed = 0;
  Serial.println("Stopping fan");
  digitalWrite( Fan_PWM, LOW );
  digitalWrite( Fan_DIR, LOW );
}

void fanSpeed(int speed)
{
  Serial.println("Setting fan speed " + String(speed));
  currentFanSpeed = speed;
  // set the motor speed and direction
  digitalWrite( Fan_DIR, HIGH );
  analogWrite( Fan_PWM, 255-speed ); // PWM speed = fast   
}

void heaterOn(bool shouldTurnOn)
{
  heaterIsHeating = shouldTurnOn;
  
  if (shouldTurnOn) {
    Serial.println("Turning heater ON");
     digitalWrite(RELAY_PIN, HIGH);
  } else {
    Serial.println("Turning heater OFF");
     digitalWrite(RELAY_PIN, LOW);
  }
}

String heatColour() {
    if (!heatingIsActive) {
    return HEATING_INACTIVE_COLOUR;
  }
  return heaterIsHeating ? HEATING_ON_COLOUR : HEATING_OFF_COLOUR ;
}

String fanColour() {
  if (!fanIsActive) {
    return FAN_INACTIVE_COLOUR;
  }
  return currentFanSpeed > 0 ? FAN_ON_COLOUR : FAN_OFF_COLOUR;
}

int getMinTemp() {
  return EEPROM.read(TEMP_MIN_LOCATION);
}

int getMaxTemp() {
  return EEPROM.read(TEMP_MAX_LOCATION);
}

int getMinHum() {
  return EEPROM.read(HUM_MIN_LOCATION);
}

int getMaxHum() {
  return EEPROM.read(HUM_MAX_LOCATION);
}

bool isFanActive() {
  return EEPROM.read(FAN_ACTIVE_LOCATION) == 1;
}

bool isHeatActive() {
  return EEPROM.read(HEATING_ACTIVE_LOCATION) == 1;
}

void setMinTemp(int minTemp) {
  if (minTemp < 15 || minTemp > 29 || minTemp >= getMaxTemp()) {
    Serial.println("Received invalid min temp " + String(minTemp));
    return;
  }
  Serial.println("Setting min temp as " + String(minTemp));
  EEPROM.write(TEMP_MIN_LOCATION, minTemp);
  EEPROM.commit();
}

void setMaxTemp(int maxTemp) {
  if (maxTemp < 16 || maxTemp > 30 || maxTemp <= getMinTemp()) {
    Serial.println("Received invalid max temp " + String(maxTemp));
    return;
  }
  Serial.println("Setting max temp as " + String(maxTemp));
  EEPROM.write(TEMP_MAX_LOCATION, maxTemp);
  EEPROM.commit();
}

void setMinHum(int minHum) {
    if (minHum < 50 || minHum > 98 || minHum >= getMaxHum()) {
    Serial.println("Received invalid min hummidity " + String(minHum));
    return;
  }
  Serial.println("Setting min humidity as " + String(minHum));
  EEPROM.write(HUM_MIN_LOCATION, minHum);
  EEPROM.commit();
}

void setMaxHum(int maxHum) {
    if (maxHum < 51 || maxHum > 99 || maxHum <= getMinHum()) {
    Serial.println("Received invalid max hummidity " + String(maxHum));
    return;
  }
    Serial.println("Setting max humidity as " + String(maxHum));
  EEPROM.write(HUM_MAX_LOCATION, maxHum);
  EEPROM.commit();
}

void setHeatActive(bool heatingActive) {
  Serial.println("Setting heating active as " + String(heatingActive));
  EEPROM.write(HEATING_ACTIVE_LOCATION, heatingActive ? 1: 0);
  EEPROM.commit();
}

void setFanActive(bool fanActive) {
  Serial.println("Setting fan active as " + String(fanActive));
  EEPROM.write(FAN_ACTIVE_LOCATION, fanActive ? 1: 0);
  EEPROM.commit();
}

void setupWebServer() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });

  server.on("/update-settings", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("updating settings");
    
   if(request->hasParam("minTemp")) {
      setMinTemp(atoi(request->getParam("minTemp")->value().c_str()));
    } else {
      Serial.println("no minTemp parameter found");
    }
   if(request->hasParam("maxTemp")) {
      setMaxTemp(atoi(request->getParam("maxTemp")->value().c_str()));
    } else {
      Serial.println("no maxTemp parameter found");
    }
   if(request->hasParam("minHum")) {
      setMinHum(atoi(request->getParam("minHum")->value().c_str()));
    } else {
      Serial.println("no minHum parameter found");
    }
   if(request->hasParam("maxHum")) {
      setMaxHum(atoi(request->getParam("maxHum")->value().c_str()));
    } else {
      Serial.println("no maxHum parameter found");
    }
    request->send_P(200, "text/plain", String("OK").c_str());
  });
  
  server.on("/sensor", HTTP_GET, [](AsyncWebServerRequest *request){
//    Serial.println("getting data for sensor");
    char sensor[10];
    char sensorData[50] = "";
    if(request->hasParam("sensor")) {
      strcpy(sensor, request->getParam("sensor")->value().c_str());
      if (strcmp(sensor,"1") == 0) {
        strcat(sensorData, String(h1).c_str());
        strcat(sensorData, "|");
        strcat(sensorData, String(t1).c_str());
        request->send_P(200, "text/plain", sensorData);
      } else if (strcmp(sensor,"2") == 0) {
        strcat(sensorData, String(h2).c_str());
        strcat(sensorData, "|");
        strcat(sensorData, String(t2).c_str());
        request->send_P(200, "text/plain", sensorData);
      } else {
        Serial.println("Unknown sensor data requested ? " +  String(sensor));
        request->send_P(200, "text/plain", String("0|0").c_str());
      }
    } else {
      request->send_P(200, "text/plain", String("0|0").c_str());
    }
  });

  server.on("/env", HTTP_GET, [](AsyncWebServerRequest *request){
    char envData[50] = "";
    strcat(envData, heatColour().c_str());
    strcat(envData, "|");
    strcat(envData, fanColour().c_str());
    request->send_P(200, "text/plain", envData);
  });

  
  server.on("/toggle", HTTP_GET, [](AsyncWebServerRequest *request){
    if(request->hasParam("heater")) {
      heatingIsActive = !heatingIsActive;
      setHeatActive(heatingIsActive);
    }
    
    if(request->hasParam("fan")) {
      fanIsActive = !fanIsActive;
      setFanActive(fanIsActive);
    }
    request->send_P(200, "text/plain", String("OK").c_str());
  });

  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/favicon.ico", "image/png");
  });
}
