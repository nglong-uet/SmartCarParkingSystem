#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Firebase_ESP_Client.h>
#include <LiquidCrystal_I2C.h>
#include <WiFiUdp.h>
#include <NTPClient.h>

// CONFIG
#define API_KEY "AIzaSyBBDd6RMSbYOniIWalNi5MUHo2ACG2b_mo"
#define DATABASE_URL "https://smartparkingsystem-f8fdb-default-rtdb.asia-southeast1.firebasedatabase.app"

const char *ssid = "Duc Long";
const char *pass = "18092004";
const char *MQTT_SERVER = "172.20.10.3";

// OBJECT
WiFiClient espClient;
PubSubClient mqtt(espClient);
FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

LiquidCrystal_I2C lcd(0x27, 16, 2);

// IR slot mapping
int IR[4] = {D5, D6, D7, D0};

// NTP (Network Time Protocol)
WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 25200); // UTC+7

unsigned long lastLCD = 0;
bool tempMsg = false;
unsigned long tempStart = 0;

// TIME
String now() {
  timeClient.update();
  time_t t = timeClient.getEpochTime();
  char buf[16];
  sprintf(buf, "%02d:%02d:%02d", hour(t), minute(t), second(t));
  return String(buf);
}

// SLOT LOGIC
int findEmptySlot() {
  for (int i = 1; i <= 4; i++) {
    String path = "/parking/slots/slot" + String(i) + "/occupied";
    if (Firebase.RTDB.getBool(&fbdo, path)) {
      if (!fbdo.boolData()) return i;
    }
  }
  return -1;
}

int findSlotByUID(String uid) {
  for (int i = 1; i <= 4; i++) {
    String path = "/parking/slots/slot" + String(i) + "/uid";
    if (Firebase.RTDB.getString(&fbdo, path)) {
      if (fbdo.stringData() == uid) return i;
    }
  }
  return -1;
}

// FIREBASE
bool cardActive(String uid) {
  uid.replace(" ", "_");
  if (Firebase.RTDB.getBool(&fbdo, "/cards/" + uid + "/active"))
    return fbdo.boolData();
  return false;
}

void logEvent(String uid, String act, String st) {
  FirebaseJson j;
  j.set("uid", uid);
  j.set("action", act);
  j.set("status", st);
  j.set("time", now());
  Firebase.RTDB.pushJSON(&fbdo, "/logs", &j);
}

// LCD
void showSlots() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Slots: ");
  for (int i = 0; i < 4; i++) {
    char s = (digitalRead(IR[i]) == HIGH) ? 'E' : 'F';
    lcd.print(String(i + 1) + s + " ");
  }
  lcd.setCursor(0, 1);
  lcd.print(now());
}

void showTemp(String l1, String l2) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(l1.substring(0, 16));
  lcd.setCursor(0, 1);
  lcd.print(l2.substring(0, 16));
  tempMsg = true;
  tempStart = millis();
}

// MQTT
void onRequest(char *t, byte *p, unsigned int l) {
  String msg;
  for (uint i = 0; i < l; i++) msg += (char)p[i];

  int s = msg.indexOf(':');
  if (s == -1) return;

  String act = msg.substring(0, s);
  String uid = msg.substring(s + 1);
  uid.replace(" ", "_");

  Serial.println("REQ: " + act + " - " + uid);

  if (!cardActive(uid)) {
    mqtt.publish("parking/response", (uid + ":NO").c_str());
    logEvent(uid, act, "NO");
    showTemp("UID: " + uid, "Khong hop le");
    return;
  }

  if (act == "ENTRY") {
    int slot = findEmptySlot();
if (slot == -1 || digitalRead(IR[slot - 1]) == LOW) {
  mqtt.publish("parking/response", (uid + ":NO").c_str());
  logEvent(uid, "ENTRY", "NO");
  showTemp("Bai day", "Tu choi");
  return;
}

    String base = "/parking/slots/slot" + String(slot);
    Firebase.RTDB.setBool(&fbdo, base + "/occupied", true);
    Firebase.RTDB.setString(&fbdo, base + "/uid", uid);
    Firebase.RTDB.setString(&fbdo, base + "/time", now());

    mqtt.publish("parking/response", (uid + ":OK").c_str());
    logEvent(uid, "ENTRY", "OK");
    showTemp("Xe vao slot", String(slot));
  }

  if (act == "EXIT") {
    int slot = findSlotByUID(uid);
if (slot == -1) {
  mqtt.publish("parking/response", (uid + ":NO").c_str());
  showTemp("Khong tim thay", "UID");
  return;
}

if (digitalRead(IR[slot - 1]) == LOW) {
  mqtt.publish("parking/response", (uid + ":WAIT").c_str());
  logEvent(uid, "EXIT", "WAIT");
  showTemp("Xe chua roi", "Slot " + String(slot));
  return;
}

    String base = "/parking/slots/slot" + String(slot);
    Firebase.RTDB.setBool(&fbdo, base + "/occupied", false);
    Firebase.RTDB.deleteNode(&fbdo, base + "/uid");
    Firebase.RTDB.deleteNode(&fbdo, base + "/time");

    mqtt.publish("parking/response", (uid + ":OK").c_str());
    logEvent(uid, "EXIT", "OK");
    showTemp("Xe ra slot", String(slot));
  }
}

void mqttCallback(char *t, byte *p, unsigned int l) {
  if (String(t) == "parking/request")
    onRequest(t, p, l);
}

void mqttReconnect() {
  if (!mqtt.connected()) {
    mqtt.connect("esp2");
    mqtt.subscribe("parking/request");
  }
}

// SETUP
void setup() {
  Serial.begin(115200);

  for (int i = 0; i < 4; i++)
    pinMode(IR[i], INPUT_PULLUP);

  lcd.init();
  lcd.backlight();

  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) delay(200);

  timeClient.begin();

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  Firebase.begin(&config, &auth);

  mqtt.setServer(MQTT_SERVER, 1883);
  mqtt.setCallback(mqttCallback);

  showSlots();
}

// LOOP
void loop() {
static bool lastIR[4] = {1, 1, 1, 1};

for (int i = 0; i < 4; i++) {
  bool cur = digitalRead(IR[i]);

  if (cur != lastIR[i]) {
    String base = "/parking/slots/slot" + String(i + 1) + "/uid";

    if (!Firebase.RTDB.getString(&fbdo, base) || fbdo.stringData() == "") {
      Firebase.RTDB.setBool(
        &fbdo,
        "/parking/slots/slot" + String(i + 1) + "/occupied",
        cur == LOW
      );
    }
    lastIR[i] = cur;
  }
}

  if (!mqtt.connected()) mqttReconnect();
  mqtt.loop();

  if (!tempMsg && millis() - lastLCD > 1000) {
    showSlots();
    lastLCD = millis();
  }

  if (tempMsg && millis() - tempStart > 3000) {
    tempMsg = false;
    showSlots();
  }
}
