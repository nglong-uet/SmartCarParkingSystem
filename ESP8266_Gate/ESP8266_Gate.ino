// ESP1_mqtt.ino (VERSION FIX 2025)
// ====== IMPORT ======
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <SPI.h>
#include <MFRC522.h>
#include <Servo.h>

// ====== CONFIG ======
const char* ssid = "Duc Long";
const char* pass = "18092004";
const char* MQTT_SERVER = "172.20.10.3";
const int   MQTT_PORT   = 1883;

WiFiClient espClient;
PubSubClient mqtt(espClient);

// RFID
#define SS_PIN  D4
#define RST_PIN D3
MFRC522 rfid(SS_PIN, RST_PIN);

// Servo
Servo barrier;
#define SERVO_PIN D8

// Buzzer
#define BUZZER_PIN D0

// IR direction
#define IR_TRUOC D1
#define IR_SAU   D2

// Global
String lastUidWaiting = "";
unsigned long lastRequestAt = 0;

// -------------------------
// BEEP
// -------------------------
void beep(int t = 120) {
  digitalWrite(BUZZER_PIN, HIGH);
  delay(t);
  digitalWrite(BUZZER_PIN, LOW);
}

// -------------------------
// READ IR ổn định (debounce chống nhiễu)
// -------------------------
bool stableRead(int pin) {
  int cntLow = 0;
  for (int i = 0; i < 15; i++) {
    if (digitalRead(pin) == LOW) cntLow++;
    delay(2);
  }
  return cntLow > 10;  // >10 lần LOW → coi như thật
}

// -------------------------
String getUID() {
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 16) uid += "0";
    uid += String(rfid.uid.uidByte[i], HEX);
    if (i < rfid.uid.size - 1) uid += " ";
  }
  uid.toUpperCase();
  return uid;
}

// =====================================================
// MQTT CALLBACK
// =====================================================
void callback(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];

  Serial.println("MQTT rx [" + String(topic) + "] " + msg);

  if (String(topic) != "parking/response") return;

  int p = msg.indexOf(':');
  if (p == -1) return;

  String uid = msg.substring(0, p);
  String status = msg.substring(p + 1);
  uid.trim(); status.trim();

  if (uid != lastUidWaiting) return;

  if (status == "OK") {
    Serial.println("Response OK → OPEN barrier");
    barrier.write(90);
    delay(600);
    barrier.write(0);
    Serial.println("Barrier CLOSED");
  } else {
    Serial.println("Access denied: " + status);
    beep(); beep();
  }

  lastUidWaiting = "";
}

// =====================================================
void mqttReconnect() {
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqtt.connect("esp1-node")) {
      Serial.println("connected");
      mqtt.subscribe("parking/response");
    } else {
      Serial.print("failed, rc=");
      Serial.println(mqtt.state());
      delay(900);
    }
  }
}

// =====================================================
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
  while (WiFi.status() != WL_CONNECTED) delay(200);
  Serial.println("WiFi OK: " + WiFi.localIP().toString());

  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  mqtt.setCallback(callback);
}

// =====================================================
// NHẬN DIỆN HƯỚNG CHUẨN – KHÔNG NHIỄU
// =====================================================
String detectDirection() {
  bool front = stableRead(IR_TRUOC);
  bool back  = stableRead(IR_SAU);

  if (!front && !back) return "";

  unsigned long st = millis();
  while (millis() - st < 600) {
    front = stableRead(IR_TRUOC);
    back  = stableRead(IR_SAU);

    if (front && !back) return "ENTRY";
    if (!front && back)  return "EXIT";
    delay(10);
  }
  return "";
}

// =====================================================
void loop() {
  if (!mqtt.connected()) mqttReconnect();
  mqtt.loop();

  // Debug IR
  Serial.print("IR_TRUOC=");
  Serial.print(digitalRead(IR_TRUOC));
  Serial.print("  IR_SAU=");
  Serial.println(digitalRead(IR_SAU));

  bool f = stableRead(IR_TRUOC);
  bool b = stableRead(IR_SAU);

  if (!f && !b) return;

  String dir = detectDirection();
  if (dir == "") return;

  Serial.println("Direction = " + dir);

  // RFID reading
  if (!rfid.PICC_IsNewCardPresent()) return;
  if (!rfid.PICC_ReadCardSerial()) return;

  beep();
  String uid = getUID();
  Serial.println("UID = " + uid);

  String payload = dir + ":" + uid;
  mqtt.publish("parking/request", payload.c_str());
  lastUidWaiting = uid;
  lastRequestAt = millis();

  unsigned long waitStart = millis();
  while (lastUidWaiting != "" && millis() - waitStart < 3000) {
    mqtt.loop();
    delay(10);
  }

  if (lastUidWaiting != "") {
    Serial.println("NO RESPONSE → REJECT");
    beep(); beep();
    lastUidWaiting = "";
  }

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}
