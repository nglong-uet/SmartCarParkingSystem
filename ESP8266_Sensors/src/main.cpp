#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Firebase_ESP_Client.h>
#include <LiquidCrystal_I2C.h>
#include <NTPClient.h>  // Thêm cho NTP(Network Time Protocol)
#include <WiFiUdp.h>    // Thêm cho NTP

#define API_KEY "AIzaSyBBDd6RMSbYOniIWalNi5MUHo2ACG2b_mo"
#define DATABASE_URL "https://smartparkingsystem-f8fdb-default-rtdb.asia-southeast1.firebasedatabase.app"

const char *ssid = "Duc Long";
const char *pass = "18092004";
const char *MQTT_SERVER = "172.20.10.3";

WiFiClient espClient;
PubSubClient mqtt(espClient);

FirebaseData fbdo;
FirebaseAuth auth;
FirebaseConfig config;

LiquidCrystal_I2C lcd(0x27, 16, 2);
int IR[4] = {D5, D6, D7, D0};

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "pool.ntp.org", 25200);  // UTC+7 cho VN, adjust offset

unsigned long reconnectTime = 0;
unsigned long lastDisplayTime = 0;
unsigned long tempDisplayStart = 0;
bool showingTempMessage = false;
String tempMessageLine1 = "";
String tempMessageLine2 = "";

String now() {
  timeClient.update();
  unsigned long epoch = timeClient.getEpochTime();
  if (epoch == 0) return "Time error";  // Handle NTP fail
  int hours = (epoch / 3600) % 24;
  int mins = (epoch / 60) % 60;
  int secs = epoch % 60;
  char buf[32];
  sprintf(buf, "%02d:%02d:%02d", hours, mins, secs);
  return String(buf);
}

bool cardActive(String uid) {
  uid.replace(" ", "_");
  if (Firebase.RTDB.getBool(&fbdo, "/cards/" + uid + "/active")) {
    if (fbdo.dataType() == "boolean") return fbdo.boolData();
    else Serial.println("Firebase error: Wrong type for active");
  } else {
    Serial.println("Firebase get active failed: " + fbdo.errorReason());
  }
  return false;
}

bool inside(String uid) {
  uid.replace(" ", "_");
  if (Firebase.RTDB.get(&fbdo, "/parking/inside/" + uid)) {
    return fbdo.dataType() != "null";
  } else {
    Serial.println("Firebase get inside failed: " + fbdo.errorReason());
  }
  return false;
}

bool slotAvailable() {
  for (int i = 0; i < 4; i++)
    if (digitalRead(IR[i]) == HIGH) return true;
  return false;
}

void logEvent(String uid, String act, String st) {
  FirebaseJson j;
  j.set("uid", uid);
  j.set("action", act);
  j.set("status", st);
  j.set("time", now());
  if (!Firebase.RTDB.pushJSON(&fbdo, "/logs", &j)) {
    Serial.println("Firebase log failed: " + fbdo.errorReason());
  }
}

void displaySlots() {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print("Slots: ");
  for (int i = 0; i < 4; i++) {
    char status = (digitalRead(IR[i]) == HIGH) ? 'E' : 'F';
    lcd.print(String(i + 1) + status + " ");
  }
  // Dòng 2 có thể hiển thị time hoặc gì đó nếu cần
  lcd.setCursor(0, 1);
  lcd.print(now());
}

void showTempMessage(String line1, String line2, unsigned long duration) {
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1.substring(0, 16));  // Cắt nếu dài hơn 16 char
  lcd.setCursor(0, 1);
  lcd.print(line2.substring(0, 16));
  tempDisplayStart = millis();
  showingTempMessage = true;
  tempMessageLine1 = line1;
  tempMessageLine2 = line2;
}

void onRequest(char *t, byte *p, unsigned int l) {
  String msg;
  for (uint i = 0; i < l; i++) msg += (char)p[i];

  int s = msg.indexOf(':');
  if (s == -1) return;
  String act = msg.substring(0, s);
  String uid = msg.substring(s + 1);
  uid.replace(" ", "_");  // Replace nhất quán ngay đầu

  Serial.println("Request: " + act + " for UID: " + uid);

  String status = "NO";
  bool isValid = cardActive(uid);

  if (isValid) {
    if (act == "ENTRY") {
      if (slotAvailable() && !inside(uid)) {
        if (Firebase.RTDB.setString(&fbdo, "/parking/inside/" + uid + "/time", now())) {
          if (mqtt.publish("parking/response", (uid + ":OK").c_str())) {
            logEvent(uid, "ENTRY", "OK");
            status = "OK";
          } else Serial.println("Publish OK failed");
        } else {
          Serial.println("Firebase set inside failed: " + fbdo.errorReason());
        }
      } else {
        if (mqtt.publish("parking/response", (uid + ":NO").c_str())) {
          logEvent(uid, "ENTRY", "NO");
        } else Serial.println("Publish NO failed");
      }
    } else if (act == "EXIT") {
      if (inside(uid)) {
        if (Firebase.RTDB.deleteNode(&fbdo, "/parking/inside/" + uid)) {
          if (mqtt.publish("parking/response", (uid + ":OK").c_str())) {
            logEvent(uid, "EXIT", "OK");
            status = "OK";
          } else Serial.println("Publish OK failed");
        } else {
          Serial.println("Firebase delete failed: " + fbdo.errorReason());
        }
      } else {
        if (mqtt.publish("parking/response", (uid + ":NO").c_str())) {
          logEvent(uid, "EXIT", "NO");
        } else Serial.println("Publish NO failed");
      }
    }
  } else {
    if (mqtt.publish("parking/response", (uid + ":NO").c_str())) {
      logEvent(uid, act, "NO");
    } else Serial.println("Publish NO failed");
  }

  // Hiển thị UID và hợp lệ (OK/NO)
  String validity = (status == "OK") ? "Hop le" : "Khong hop le";
  showTempMessage("UID: " + uid, validity, 3000);  // Hiển thị 3 giây
}

void mqttCallback(char *t, byte *p, unsigned int l) {
  if (String(t) == "parking/request") onRequest(t, p, l);
}

void mqttReconnect() {
  if (millis() - reconnectTime > 5000) {
    reconnectTime = millis();
    if (mqtt.connect("esp2")) {
      mqtt.subscribe("parking/request");
      Serial.println("MQTT reconnected");
    } else {
      Serial.println("MQTT reconnect failed");
    }
  }
}

void setup() {
  Serial.begin(115200);
  for (int i = 0; i < 4; i++) pinMode(IR[i], INPUT_PULLUP);

  lcd.init();
  lcd.backlight();

  WiFi.begin(ssid, pass);
  unsigned long wifiStart = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 10000) {
    delay(200);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED) Serial.println("WiFi connected");
  else Serial.println("WiFi connect failed");

  timeClient.begin();  // Khởi động NTP

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  Firebase.begin(&config, &auth);

  mqtt.setServer(MQTT_SERVER, 1883);
  mqtt.setCallback(mqttCallback);

  displaySlots();  // Hiển thị ban đầu
}

void loop() {
  if (!mqtt.connected()) mqttReconnect();
  mqtt.loop();

  // Cập nhật hiển thị slots mỗi 1 giây nếu không có temp message
  if (!showingTempMessage && millis() - lastDisplayTime > 1000) {
    displaySlots();
    lastDisplayTime = millis();
  }

  // Kiểm tra nếu hết thời gian hiển thị temp, quay về slots
  if (showingTempMessage && millis() - tempDisplayStart > 3000) {
    showingTempMessage = false;
    displaySlots();
  }
}