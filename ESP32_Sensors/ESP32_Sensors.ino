// ESP2_mqtt.ino
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <LiquidCrystal_I2C.h>
#include <Vector.h>
#include <TimeLib.h> // optional for timestamp formatting

// === CONFIG
const char* ssid = "Duc Long";
const char* pass = "18092004";
const char* MQTT_SERVER = "172.20.10.3";
const int MQTT_PORT = 1883;

WiFiClient espClient;
PubSubClient mqtt(espClient);

LiquidCrystal_I2C lcd(0x27, 16, 2);

// IR slot
int IR[4] = {D5, D6, D7, D0};

// UID hợp lệ
String validUID[] = {
  "B3 D6 77 56",
  "6E 3D DF 2E",
  "36 55 DF 2E",
  "F3 A9 DF 2E",
  "E7 2C DF 2E"
};

// DS UID đang trong bãi
Vector<String> occupiedUID;

// helper timestamp (simple)
String nowIso() {
  // nếu bạn chưa config NTP, có thể gửi millis() hoặc server thêm thời gian
  unsigned long s = millis() / 1000;
  int ss = s % 60;
  int mm = (s/60) % 60;
  int hh = (s/3600) % 24;
  char buf[32];
  sprintf(buf, "1970-01-01T%02d:%02d:%02d", hh, mm, ss); // placeholder date
  return String(buf);
}

bool checkUID(String uid) {
  int total = sizeof(validUID) / sizeof(validUID[0]);
  for (int i = 0; i < total; i++) {
    if (validUID[i] == uid) return true;
  }
  return false;
}

bool isInside(String uid) {
  for (int i = 0; i < occupiedUID.size(); i++) {
    if (occupiedUID[i] == uid) return true;
  }
  return false;
}

bool removeUID(String uid) {
  for (int i = 0; i < occupiedUID.size(); i++) {
    if (occupiedUID[i] == uid) {
      Serial.println("REMOVED: " + uid);
      occupiedUID.remove(i);
      return true;
    }
  }
  Serial.println("UID NOT FOUND TO REMOVE: " + uid);
  return false;
}

bool slotAvailable() {
  for (int i = 0; i < 4; i++) {
    if (digitalRead(IR[i]) == HIGH) return true;  // HIGH = empty
  }
  return false;
}

void displaySlot() {
  lcd.clear();
  for (int i = 0; i < 4; i++) {
    lcd.setCursor(i * 4, 0);
    lcd.print(i + 1);
    lcd.print(":");
    lcd.print(digitalRead(IR[i]) == LOW ? "F" : "E");
  }
}

// ===== MQTT callback
void onRequest(char* topic, byte* payload, unsigned int length) {
  String msg;
  for (unsigned int i = 0; i < length; i++) msg += (char)payload[i];
  msg.trim();
  Serial.println("MQTT request: " + msg); // format "ENTRY:UID" or "EXIT:UID"

  String action = "";
  String uid = "";

  if (msg.startsWith("ENTRY:")) {
    action = "ENTRY";
    uid = msg.substring(6);
  } else if (msg.startsWith("EXIT:")) {
    action = "EXIT";
    uid = msg.substring(5);
  } else {
    Serial.println("Unknown request");
    return;
  }
  uid.trim();

  // fake display
  lcd.clear();
  lcd.print("UID:"); lcd.print(uid);

  // invalid uid
  if (!checkUID(uid)) {
    lcd.setCursor(0,1); lcd.print("INVALID");
    // publish response and no event (or with status NO)
    String resp = uid + String(":NO");
    mqtt.publish("parking/response", resp.c_str());
    // publish event for server
    String ev = "{\"action\":\"" + action + "\",\"uid\":\"" + uid + "\",\"status\":\"NO\",\"timestamp\":\"" + nowIso() + "\"}";
    mqtt.publish("parking/event", ev.c_str());
    return;
  }

  if (action == "EXIT") {
    if (!isInside(uid)) {
      lcd.setCursor(0,1); lcd.print("NOT IN");
      mqtt.publish("parking/response", (uid + String(":NO")).c_str());
      String ev = "{\"action\":\"EXIT\",\"uid\":\"" + uid + "\",\"status\":\"NO\",\"timestamp\":\"" + nowIso() + "\"}";
      mqtt.publish("parking/event", ev.c_str());
      return;
    }
    removeUID(uid);
    lcd.setCursor(0,1); lcd.print("EXIT OK");
    mqtt.publish("parking/response", (uid + String(":OK")).c_str());
    String ev = "{\"action\":\"EXIT\",\"uid\":\"" + uid + "\",\"status\":\"OK\",\"timestamp\":\"" + nowIso() + "\"}";
    mqtt.publish("parking/event", ev.c_str());
    return;
  }

  // ENTRY:
  if (isInside(uid)) {
    lcd.setCursor(0,1); lcd.print("ALREADY IN");
    mqtt.publish("parking/response", (uid + String(":NO")).c_str());
    String ev = "{\"action\":\"ENTRY\",\"uid\":\"" + uid + "\",\"status\":\"NO\",\"timestamp\":\"" + nowIso() + "\"}";
    mqtt.publish("parking/event", ev.c_str());
    return;
  }

  if (!slotAvailable()) {
    lcd.setCursor(0,1); lcd.print("FULL");
    mqtt.publish("parking/response", (uid + String(":FULL")).c_str());
    String ev = "{\"action\":\"ENTRY\",\"uid\":\"" + uid + "\",\"status\":\"FULL\",\"timestamp\":\"" + nowIso() + "\"}";
    mqtt.publish("parking/event", ev.c_str());
    return;
  }

  occupiedUID.push_back(uid);
  lcd.setCursor(0,1); lcd.print("ENTRY OK");
  mqtt.publish("parking/response", (uid + String(":OK")).c_str());
  String ev = "{\"action\":\"ENTRY\",\"uid\":\"" + uid + "\",\"status\":\"OK\",\"timestamp\":\"" + nowIso() + "\"}";
  mqtt.publish("parking/event", ev.c_str());
}

void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String t = String(topic);
  if (t == "parking/request") onRequest(topic, payload, length);
}

void mqttReconnect() {
  while (!mqtt.connected()) {
    Serial.print("Attempting MQTT connection...");
    if (mqtt.connect("esp2-node")) {
      Serial.println("connected");
      mqtt.subscribe("parking/request");
    } else {
      Serial.print("failed, rc=");
      Serial.println(mqtt.state());
      delay(1000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  for (int i = 0; i < 4; i++) pinMode(IR[i], INPUT_PULLUP);

  lcd.init();
  lcd.backlight();
  lcd.print("Parking Ready");

  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) delay(200);

  Serial.print("ESP2 IP: ");
  Serial.println(WiFi.localIP());

  mqtt.setServer(MQTT_SERVER, MQTT_PORT);
  mqtt.setCallback(mqttCallback);
}

void loop() {
  if (!mqtt.connected()) mqttReconnect();
  mqtt.loop();

  displaySlot();
  delay(200);
}
