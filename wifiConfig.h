#pragma once
#include <EEPROM.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WebServer.h>
#include <Ticker.h>

#define LED_PIN 2
#define PUSHTIME 5000

WebServer webServer(80);
Ticker blinker;

String ssid, password;
int wifiMode = 0; // 0: config, 1: connected, 2: lost
unsigned long blinkTime = 0;
int connectRetry = 0;
const int maxRetry = 20;

const char html[] PROGMEM = R"html(
<!DOCTYPE html>
<html><head>
<meta charset="utf-8"><meta name="viewport" content="width=device-width, initial-scale=1">
<title>WiFi Setup</title>
<style>
body{display:flex;justify-content:center;align-items:center;flex-direction:column;font-family:sans-serif}
button{width:135px;height:40px;margin-top:10px;border-radius:6px}
input,select{width:275px;height:30px;font-size:16px;margin-bottom:10px}
</style>
</head>
<body>
<h3>WiFi Setup</h3>
<p id="info">Scanning...</p>
<select id="ssid"><option>Choose WiFi</option></select><br>
<input id="password" placeholder="Password"><br>
<button onclick="saveWifi()" style="background-color:cyan">SAVE</button>
<button onclick="reStart()" style="background-color:pink">RESTART</button>
<script>
let x=new XMLHttpRequest();
function scanWifi(){
  x.onreadystatechange=function(){
    if(x.readyState==4&&x.status==200){
      let arr=JSON.parse(x.responseText);
      document.getElementById('info').innerHTML='Scan done!';
      let sel=document.getElementById('ssid');
      arr.forEach(s=>sel.add(new Option(s,s)));
    }
  };
  x.open('GET','/scanWifi',true);x.send();
}
window.onload=scanWifi;
function saveWifi(){
  let s=document.getElementById('ssid').value;
  let p=document.getElementById('password').value;
  x.open('GET',`/saveWifi?ssid=${s}&pass=${p}`,true);x.send();
  alert('Saved! Restart ESP.');
}
function reStart(){x.open('GET','/reStart',true);x.send();}
</script>
</body></html>
)html";

void setupWebServer() {
  webServer.on("/", []() {
    webServer.send_P(200, "text/html", html);
  });

  webServer.on("/scanWifi", []() {
    int n = WiFi.scanNetworks();
    DynamicJsonDocument doc(1024);
    for (int i = 0; i < n; i++) doc.add(WiFi.SSID(i));
    String wifiList;
    serializeJson(doc, wifiList);
    webServer.send(200, "application/json", wifiList);
  });

  webServer.on("/saveWifi", []() {
    EEPROM.writeString(0, webServer.arg("ssid"));
    EEPROM.writeString(32, webServer.arg("pass"));
    EEPROM.commit();
    webServer.send(200, "text/plain", "WiFi saved!");
  });

  webServer.on("/reStart", []() {
    webServer.send(200, "text/plain", "Restarting...");
    delay(1000);
    ESP.restart();
  });

  webServer.begin();
}

void startAPMode() {
  WiFi.mode(WIFI_AP);

  uint8_t mac[6];
  WiFi.softAPmacAddress(mac);
  String apName = "ESP32-" + String(mac[4], HEX) + String(mac[5], HEX);
  apName.toUpperCase();

  WiFi.softAP(apName.c_str());
  Serial.println("AP Mode: " + apName);
  Serial.println("Access: " + WiFi.softAPIP().toString());

  wifiMode = 0;
  setupWebServer();
}


void blinkLed(uint32_t t) {
  if (millis() - blinkTime > t) {
    digitalWrite(LED_PIN, !digitalRead(LED_PIN));
    blinkTime = millis();
  }
}

void ledControl() {
  if (wifiMode == 0) blinkLed(150);      // AP
  else if (wifiMode == 1) blinkLed(3000); // Connected
  else blinkLed(300);                    // Lost WiFi
}

void WiFiEventHandler(WiFiEvent_t event, WiFiEventInfo_t info) {
  switch (event) {
    case IP_EVENT_STA_GOT_IP:
      Serial.println("Connected to WiFi");
      wifiMode = 1;
      connectRetry = 0;
      break;

    case WIFI_EVENT_STA_DISCONNECTED:
      Serial.println("Lost WiFi, retrying...");
      wifiMode = 2;
      connectRetry++;

      if (connectRetry >= maxRetry) {
        Serial.println("Can't reconnect → Switching to AP mode...");
        WiFi.disconnect(true);
        delay(500);
        startAPMode();
      } else {
        WiFi.reconnect();
      }
      break;
  }
}

void setupWifi() {
  if (ssid.length() > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.onEvent(WiFiEventHandler);
    WiFi.begin(ssid.c_str(), password.c_str());
    Serial.println("🔌 Connecting to WiFi...");

    unsigned long startAttempt = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - startAttempt < 12000) {
      delay(200);
    }

    if (WiFi.status() != WL_CONNECTED) {
      Serial.println("Cannot connect → Switching to AP mode...");
      startAPMode();
    }
  } else {
    startAPMode();
  }
}

class Config {
public:
  void begin() {
    pinMode(LED_PIN, OUTPUT);
    EEPROM.begin(200);
    blinker.attach_ms(50, ledControl);

    ssid = EEPROM.readString(0);
    password = EEPROM.readString(32);

    if (ssid.length() > 0) {
      Serial.println("Saved WiFi: " + ssid);
    }
    setupWifi();
  }

  void run() {
    if (wifiMode == 0) webServer.handleClient();
  }
} wifiConfig;
