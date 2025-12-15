#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Firebase_ESP_Client.h>
#include <LiquidCrystal_I2C.h>

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

String now()
{
  unsigned long s = millis() / 1000;
  char buf[32];
  sprintf(buf, "%02lu:%02lu:%02lu", s / 3600, (s / 60) % 60, s % 60);
  return String(buf);
}

bool cardActive(String uid)
{
  uid.replace(" ", "_");
  return Firebase.RTDB.getBool(&fbdo, "/cards/" + uid + "/active") && fbdo.boolData();
}

bool inside(String uid)
{
  uid.replace(" ", "_");
  if (Firebase.RTDB.get(&fbdo, "/parking/inside/" + uid))
    return fbdo.dataType() != "null";
  return false;
}

bool slotAvailable()
{
  for (int i = 0; i < 4; i++)
    if (digitalRead(IR[i]) == HIGH)
      return true;
  return false;
}

void logEvent(String uid, String act, String st)
{
  FirebaseJson j;
  j.set("uid", uid);
  j.set("action", act);
  j.set("status", st);
  j.set("time", now());
  Firebase.RTDB.pushJSON(&fbdo, "/logs", &j);
}

void onRequest(char *t, byte *p, unsigned int l)
{
  String msg;
  for (uint i = 0; i < l; i++)
    msg += (char)p[i];

  int s = msg.indexOf(':');
  String act = msg.substring(0, s);
  String uid = msg.substring(s + 1);

  lcd.clear();
  lcd.print(uid);

  if (!cardActive(uid))
  {
    mqtt.publish("parking/response", (uid + ":NO").c_str());
    logEvent(uid, act, "NO");
    return;
  }

  if (act == "ENTRY")
  {
    if (!slotAvailable() || inside(uid))
    {
      mqtt.publish("parking/response", (uid + ":NO").c_str());
      logEvent(uid, "ENTRY", "NO");
      return;
    }
    uid.replace(" ", "_");
    Firebase.RTDB.setString(&fbdo, "/parking/inside/" + uid + "/time", now());
    mqtt.publish("parking/response", (uid + ":OK").c_str());
    logEvent(uid, "ENTRY", "OK");
  }

  if (act == "EXIT")
  {
    if (!inside(uid))
    {
      mqtt.publish("parking/response", (uid + ":NO").c_str());
      logEvent(uid, "EXIT", "NO");
      return;
    }
    uid.replace(" ", "_");
    Firebase.RTDB.deleteNode(&fbdo, "/parking/inside/" + uid);
    mqtt.publish("parking/response", (uid + ":OK").c_str());
    logEvent(uid, "EXIT", "OK");
  }
}

void mqttCallback(char *t, byte *p, unsigned int l)
{
  if (String(t) == "parking/request")
    onRequest(t, p, l);
}

void mqttReconnect()
{
  while (!mqtt.connected())
  {
    mqtt.connect("esp2");
    delay(500);
  }
  mqtt.subscribe("parking/request");
}

void setup()
{
  Serial.begin(115200);
  for (int i = 0; i < 4; i++)
    pinMode(IR[i], INPUT_PULLUP);

  lcd.init();
  lcd.backlight();

  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED)
    delay(200);

  config.api_key = API_KEY;
  config.database_url = DATABASE_URL;
  Firebase.begin(&config, &auth);

  mqtt.setServer(MQTT_SERVER, 1883);
  mqtt.setCallback(mqttCallback);
}

void loop()
{
  if (!mqtt.connected())
    mqttReconnect();
  mqtt.loop();
}
