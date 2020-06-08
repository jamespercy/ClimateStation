/*********
  Rui Santos
  Complete project details at https://randomnerdtutorials.com/esp8266-dht11dht22-temperature-and-humidity-web-server-with-arduino-ide/
*********/

// Import required libraries
#include <Adafruit_Sensor.h>
#include <Arduino.h>
#include <DHT.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncTCP.h>
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

// Use web browser to view and copy
// SHA1 fingerprint of the certificate
const char fingerprint[] PROGMEM = "6D A7 25 5D 9F AD 12 7E DF 5A 9B 00 A0 8A 3C 9F 8D 7F BD DF";

#define S1_DHTPIN 14  // Digital pin connected to the DHT sensor
#define S2_DHTPIN 12  // Digital pin connected to the DHT sensor

// Uncomment the type of sensor in use:
//#define DHTTYPE    DHT11     // DHT 11
#define DHTTYPE    DHT22     // DHT 22 (AM2302)
//#define DHTTYPE    DHT21     // DHT 21 (AM2301)

//Fan controll definitions

#define FanIA 5 //fan conrol input A blue
#define FanIB 4 //fan conrol input B yellow

#define Fan_PWM FanIA //fan PWM speed
#define Fan_DIR FanIB //fan direction
// the actual values for "fast" and "slow" depend on the motor
#define PWM_SLOW 50 // arbitrary slow speed PWM duty cycle
#define PWM_FAST 200 // arbitrary fast speed PWM duty cycle
#define PWM_FULL 255 // 255 is the new over 9000

DHT dht1(S1_DHTPIN, DHTTYPE);
DHT dht2(S2_DHTPIN, DHTTYPE);

void getMeasurements();
void setupFan();
void stopFan();
void fanSpeed(int speed);
void registerStartup();
void postRequest(String url, char* payload);
void reportMeasurement(String sensorId, String measurementType, float measurement);

// current temperature & humidity, updated in loop()
float t1 = 0.0;
float t2 = 0.0;
float h1 = 0.0;
float h2 = 0.0;

int currentFanSpeed = 0;

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
    p { font-size: 3.0rem; }
    .units { font-size: 1.2rem; }
    .dht-labels{
      font-size: 1.5rem;
      vertical-align:middle;
      padding-bottom: 15px;
    }
  </style>
</head>
<body>
  <h3>Sensor 1</h3>
  <p>
    <i class="fas fa-thermometer-half" style="color:#059e8a;"></i> 
    <span class="dht-labels">Temperature</span> 
    <span id="temperature-1">%TEMPERATURE1%</span>
    <sup class="units">&deg;C</sup>
  </p>
  <p>
    <i class="fas fa-tint" style="color:#00add6;"></i> 
    <span class="dht-labels">Humidity</span>
    <span id="humidity-1">%HUMIDITY1%</span>
    <sup class="units">&#37;</sup>
  </p>
    <h3>Sensor 2</h3>
  <p>
    <i class="fas fa-thermometer-half" style="color:#059e8a;"></i> 
    <span class="dht-labels">Temperature</span> 
    <span id="temperature-2">%TEMPERATURE2%</span>
    <sup class="units">&deg;C</sup>
  </p>
  <p>
    <i class="fas fa-tint" style="color:#00add6;"></i> 
    <span class="dht-labels">Humidity</span>
    <span id="humidity-2">%HUMIDITY2%</span>
    <sup class="units">&#37;</sup>
  </p>
</body>
<script>

let getTemperature = (sensor) => {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = () => {
    if (this.readyState == 4 && this.status == 200) {
      let tempId = "temperature-"+sensor
      console.log('applying ' + this.responseText + ' to ' + tempId);
      document.getElementById(tempId).innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/temperature?sensor="+sensor, true);
  xhttp.send();
}

let getHumidity = (sensor) => {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = () => {
    if (this.readyState == 4 && this.status == 200) {
      let humId = "humidity-"+sensor
      console.log('applying ' + this.responseText + ' to ' + humId);
      document.getElementById(humId).innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "/humidity?sensor=" + sensor, true);
  xhttp.send();
}

setInterval(getTemperature, 10000, '1' ) ;
setInterval(getTemperature, 10000, '2' ) ;
setInterval(getHumidity, 10000, '1' ) ;
setInterval(getHumidity, 10000, '2' ) ;
</script>
</html>)rawliteral";

// Replaces placeholder with DHT values
String processor(const String& var){
  //Serial.println(var);
  if(var == "TEMPERATURE1"){
    return String(t1);
  }  else if(var == "HUMIDITY1"){
    return String(h1);
  } else if(var == "TEMPERATURE2"){
    return String(t2);
  } else if(var == "HUMIDITY2"){
    return String(h2);
  } else
  return String();
}

void setup(){
  // Serial port for debugging purposes
  Serial.begin(115200);
  dht1.begin();
  dht2.begin();

  setupFan();
  
  // Connect to Wi-Fi
  WiFi.begin(ssid, password);
  Serial.println("Connecting to WiFi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.println(".");
  }

  //secure client
  Serial.printf("Using fingerprint '%s'\n", fingerprint);
  client.setFingerprint(fingerprint);

  // Print ESP8266 Local IP Address
  Serial.println(WiFi.localIP());
  Serial.println(apiSecret);
  // Route for root / web page
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/html", index_html, processor);
  });
  server.on("/temperature", HTTP_GET, [](AsyncWebServerRequest *request){
    char sensor[10];
    if(request->hasParam("sensor")) {
      strcpy(sensor, request->getParam("sensor")->value().c_str());
          if (strcmp(sensor,"1") == 0) {
      request->send_P(200, "text/plain", String(t1).c_str());
      } else if (strcmp(sensor,"2") == 0) {
        request->send_P(200, "text/plain", String(t2).c_str());
      } else {
      Serial.println("Unknown sensor data requested ? " +  String(sensor));
      request->send_P(200, "text/plain", String("0").c_str());
      }
    } else {
      request->send_P(200, "text/plain", String("0").c_str());
    }

  });
  server.on("/humidity", HTTP_GET, [](AsyncWebServerRequest *request){
        char sensor[10];
    if(request->hasParam("sensor")) {
      strcpy(sensor, request->getParam("sensor")->value().c_str());
          if (strcmp(sensor,"1") == 0) {
      request->send_P(200, "text/plain", String(h1).c_str());
      } else if (strcmp(sensor,"2") == 0) {
        request->send_P(200, "text/plain", String(h2).c_str());
      } else {
      Serial.println("Unknown sensor data requested ? " + String(sensor));
      request->send_P(200, "text/plain", String("0").c_str());
      }
    } else {
      Serial.println("Which sensor do you want data from?");
      request->send_P(200, "text/plain", String("0").c_str());
    }
  });

  server.on("/favicon.ico", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/favicon.ico", "image/png");
  });

  registerStartup();

  // Start server
  server.begin();
}
 
void loop(){  
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    // save the last time you updated the DHT values
    previousMillis = currentMillis;
    getMeasurements();
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
      if (h1 > 90) {
        fanSpeed(PWM_FULL);
      } else if (h1 < 80) {
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
  pinMode( Fan_PWM, OUTPUT );
  pinMode( Fan_DIR, OUTPUT );
  stopFan();
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
