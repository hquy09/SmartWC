
#define BLYNK_TEMPLATE_ID "YOUR_TEMPLATE_ID"
#define BLYNK_TEMPLATE_NAME "YOUR_NAME"
#define BLYNK_AUTH_TOKEN "TOKEN"

#include <Wire.h>
#include <BlynkSimpleEsp32.h>
#include <DHT.h>
#include <wifiConfig.h>
#include <MPU6050.h>

#define DHT11_PIN 13
#define RELAY1 18
#define RELAY2 19
#define RELAY3 33
#define BUZZER 17
#define PIR_PIN 16
#define MQ02 34
#define LOG_VPIN V100

DHT dht(DHT11_PIN, DHT11);
MPU6050 mpu;
bool blynkConnect = 0;

void sendLog(const String &msg) {
  Serial.println(msg);
}

BLYNK_CONNECTED() {
  sendLog("[BLYNK] Syncing dữ liệu...");
  Blynk.syncVirtual(V0, V1, V2, V3, V4, V6, V7, V8, V9);
}

BLYNK_WRITE(V2) {
  int state = param.asInt();
  digitalWrite(RELAY1, state);
  sendLog(String("[BLYNK] Relay 1 (V2) -> ") + (state ? "ON" : "OFF"));
}

BLYNK_WRITE(V3) {
  int state = param.asInt();
  digitalWrite(RELAY2, state);
  sendLog(String("[BLYNK] Relay 2 (V3) -> ") + (state ? "ON" : "OFF"));
}

BLYNK_WRITE(V4) {
  int state = param.asInt();
  digitalWrite(RELAY3, state);
  sendLog(String("[BLYNK] Relay 3 (V4) -> ") + (state ? "ON" : "OFF"));
}

BLYNK_WRITE(V6) {
  int state = param.asInt();
  if (state) {
    tone(BUZZER, 4400); 
    sendLog("[BLYNK] Buzzer (V6) -> ON");
  } else {
    noTone(BUZZER);
    sendLog("[BLYNK] Buzzer (V6) -> OFF");
  }
}

float readAccelMagnitude() {
  int16_t ax, ay, az;
  mpu.getAcceleration(&ax, &ay, &az);
  float axf = ax / 16384.0;
  float ayf = ay / 16384.0;
  float azf = az / 16384.0;
  float magnitude = sqrt(axf * axf + ayf * ayf + azf * azf);
  return magnitude;
}

void readSensors() {
  sendLog("[SENSOR] 🔍 Đọc cảm biến chuyển động, khí gas, MPU6050...");
  int pirState = digitalRead(PIR_PIN);
  Blynk.virtualWrite(V8, pirState);
  sendLog(String("[SENSOR] PIR (V8) -> ") + pirState);
  int mq2Value = analogRead(MQ02);
  Blynk.virtualWrite(V9, mq2Value);
  sendLog(String("[SENSOR] MQ2 (V9) -> ") + mq2Value);
  float accelMag = readAccelMagnitude();
  Blynk.virtualWrite(V7, accelMag);
  sendLog(String("[SENSOR] MPU6050 (V7) -> ") + String(accelMag, 2) + " g");
}

void readDHT() {
  sendLog("[DHT11] Đang đọc dữ liệu...");
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  if (isnan(h) || isnan(t)) {
    sendLog("[DHT11] Lỗi đọc cảm biến!");
    return;
  }
  sendLog(String("[DHT11] Nhiệt độ: ") + String(t, 1) + " °C | Độ ẩm: " + String(h, 1) + " %");
  Blynk.virtualWrite(V0, t);
  Blynk.virtualWrite(V1, h);
}

void setup() {
  Serial.begin(115200);
  wifiConfig.begin();
  sendLog("[WiFi] Đang kết nối WiFi...");
  Wire.begin(21, 22);
  sendLog("[I2C] Đã khởi tạo Wire");
  dht.begin();
  sendLog("[DHT11] Đã khởi tạo cảm biến DHT11");
  mpu.initialize();
  sendLog("[MPU6050] Đã khởi tạo MPU6050");
  Blynk.config(BLYNK_AUTH_TOKEN, "blynk.cloud", 80);
  sendLog("[BLYNK] Đang cấu hình kết nối đến Blynk Cloud...");

  pinMode(RELAY1, OUTPUT);
  pinMode(RELAY2, OUTPUT);
  pinMode(RELAY3, OUTPUT);
  pinMode(BUZZER, OUTPUT);
  pinMode(PIR_PIN, INPUT_PULLUP);
  digitalWrite(RELAY1, LOW);
  digitalWrite(RELAY2, LOW);
  digitalWrite(RELAY3, LOW);
  digitalWrite(BUZZER, LOW);
}

void loop() {
  wifiConfig.run();

  if (WiFi.status() == WL_CONNECTED) {
    if (!Blynk.connected()) {
      sendLog("[BLYNK] Mất kết nối Blynk, đang thử lại...");
      if (Blynk.connect(2000)) {
        sendLog("[BLYNK] Đã kết nối lại thành công!");
        blynkConnect = 1;
      } else {
        sendLog("[BLYNK] Kết nối lại thất bại!");
        blynkConnect = 0;
      }
    } else {
      blynkConnect = 1;
    }

    if (blynkConnect) Blynk.run();
  } else {
    sendLog("[WiFi] Mất kết nối WiFi!");
    blynkConnect = 0;
  }

  if (!Blynk.connected()) blynkConnect = 0;
  static unsigned long lastDhtRead = 0;
  static unsigned long lastSensorRead = 0;
  unsigned long currentMillis = millis();
  if (currentMillis - lastDhtRead > 2000) {
    lastDhtRead = currentMillis;
    sendLog("\n--- Cập nhật DHT11 ---");
    readDHT();
  }
  if (currentMillis - lastSensorRead > 1000) {
    lastSensorRead = currentMillis;
    sendLog("\n--- Cập nhật dữ liệu cảm biến ---");
    readSensors();
  }
}
