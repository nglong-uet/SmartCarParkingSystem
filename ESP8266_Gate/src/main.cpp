#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>

const char *ssid = "Duc Long";
const char *pass = "18092004";
const char *MQTT_SERVER = "172.20.10.3";

WiFiClient espClient;
PubSubClient mqtt(espClient);

#define SS_PIN D4
#define RST_PIN D3
MFRC522 rfid(SS_PIN, RST_PIN);

Servo barrier;
#define SERVO_PIN D8
#define BUZZER_PIN D0
#define IR_TRUOC D1
#define IR_SAU D2

String lastUidWaiting = "";
unsigned long lastRequestTime = 0;
unsigned long reconnectTime = 0;

void beep(int times = 1) {
  for (int i = 0; i < times; i++) {
    digitalWrite(BUZZER_PIN, HIGH);
    delay(120);  // Giữ delay nhỏ cho beep, không ảnh hưởng lớn
    digitalWrite(BUZZER_PIN, LOW);
    if (i < times - 1) delay(120);
  }
}

bool stableRead(int pin) {
  int c = 0;
  for (int i = 0; i < 15; i++) {
    if (digitalRead(pin) == LOW) c++;
    delay(2);
  }
  return c > 10;
}

String getUID() {
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 16) uid += "0";
    uid += String(rfid.uid.uidByte[i], HEX);
    if (i < rfid.uid.size - 1) uid += " ";
  }
  uid.toUpperCase();
  uid.trim();  // Loại bỏ space thừa nếu có
  return uid;
}

void callback(char *topic, byte *payload, unsigned int len) {
  String msg;
  for (uint i = 0; i < len; i++) msg += (char)payload[i];
  int p = msg.indexOf(':');
  if (p == -1) return;

  String uid = msg.substring(0, p);
  String st = msg.substring(p + 1);

  if (uid != lastUidWaiting) return;

  if (st == "OK") {
    barrier.write(90);
    delay(800);  // Giữ delay cho servo, nhưng có thể thay millis nếu cần
    barrier.write(0);
    Serial.println("Response OK for UID: " + uid);
  } else {
    beep(2);
    Serial.println("Response NO for UID: " + uid);
  }
  lastUidWaiting = "";
}

void mqttReconnect() {
  if (millis() - reconnectTime > 5000) {  // Retry every 5s
    reconnectTime = millis();
    if (mqtt.connect("esp1")) {
      mqtt.subscribe("parking/response");
      Serial.println("MQTT reconnected");
    } else {
      Serial.println("MQTT reconnect failed");
    }
  }
}

String detectDir() {
  bool f = stableRead(IR_TRUOC);
  bool b = stableRead(IR_SAU);
  if (!f && !b) return "";

  unsigned long t = millis();
  while (millis() - t < 600) {
    f = stableRead(IR_TRUOC);
    b = stableRead(IR_SAU);
    if (f && !b) return "ENTRY";
    if (!f && b) return "EXIT";
    if (f && b) return "ENTRY";  // Thêm case: Giả sử ENTRY nếu cả hai blocked (xe lớn hoặc kẹt)
  }
  return "";
}

void setup() {
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();

  barrier.attach(SERVO_PIN);
  barrier.write(0);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(IR_TRUOC, INPUT_PULLUP);
  pinMode(IR_SAU, INPUT_PULLUP);

  WiFi.begin(ssid, pass);
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 10000) {
    delay(200);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) Serial.println("WiFi connected");
  else Serial.println("WiFi connect failed");

  mqtt.setServer(MQTT_SERVER, 1883);
  mqtt.setCallback(callback);
}

void loop() {
  if (!mqtt.connected()) mqttReconnect();
  mqtt.loop();

  String dir = detectDir();
  if (dir == "") return;

  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;

  beep();
  String uid = getUID();
  Serial.println("Detected " + dir + " with UID: " + uid);

  String payload = dir + ":" + uid;
  if (mqtt.publish("parking/request", payload.c_str())) {
    lastUidWaiting = uid;
    lastRequestTime = millis();
    Serial.println("Published: " + payload);
  } else {
    Serial.println("Publish failed: " + payload);
    beep(3);  // Beep lỗi publish
  }

  while (lastUidWaiting != "" && millis() - lastRequestTime < 3000) {
    mqtt.loop();
    delay(10);
  }
  if (lastUidWaiting != "") {
    Serial.println("Timeout for UID: " + lastUidWaiting);
    beep(3);  // Beep lỗi timeout
    lastUidWaiting = "";
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}