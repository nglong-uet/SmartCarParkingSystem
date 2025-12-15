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

void beep()
{
  digitalWrite(BUZZER_PIN, HIGH);
  delay(120);
  digitalWrite(BUZZER_PIN, LOW);
}

bool stableRead(int pin)
{
  int c = 0;
  for (int i = 0; i < 15; i++)
  {
    if (digitalRead(pin) == LOW)
      c++;
    delay(2);
  }
  return c > 10;
}

String getUID()
{
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++)
  {
    if (rfid.uid.uidByte[i] < 16)
      uid += "0";
    uid += String(rfid.uid.uidByte[i], HEX);
    if (i < rfid.uid.size - 1)
      uid += " ";
  }
  uid.toUpperCase();
  return uid;
}

void callback(char *topic, byte *payload, unsigned int len)
{
  String msg;
  for (uint i = 0; i < len; i++)
    msg += (char)payload[i];
  int p = msg.indexOf(':');
  if (p == -1)
    return;

  String uid = msg.substring(0, p);
  String st = msg.substring(p + 1);

  if (uid != lastUidWaiting)
    return;

  if (st == "OK")
  {
    barrier.write(90);
    delay(800);
    barrier.write(0);
  }
  else
  {
    beep();
    beep();
  }
  lastUidWaiting = "";
}

void mqttReconnect()
{
  while (!mqtt.connected())
  {
    mqtt.connect("esp1");
    delay(500);
  }
  mqtt.subscribe("parking/response");
}

String detectDir()
{
  bool f = stableRead(IR_TRUOC);
  bool b = stableRead(IR_SAU);
  if (!f && !b)
    return "";

  unsigned long t = millis();
  while (millis() - t < 600)
  {
    f = stableRead(IR_TRUOC);
    b = stableRead(IR_SAU);
    if (f && !b)
      return "ENTRY";
    if (!f && b)
      return "EXIT";
  }
  return "";
}

void setup()
{
  Serial.begin(115200);
  SPI.begin();
  rfid.PCD_Init();

  barrier.attach(SERVO_PIN);
  barrier.write(0);

  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(IR_TRUOC, INPUT_PULLUP);
  pinMode(IR_SAU, INPUT_PULLUP);

  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED)
    delay(200);

  mqtt.setServer(MQTT_SERVER, 1883);
  mqtt.setCallback(callback);
}

void loop()
{
  if (!mqtt.connected())
    mqttReconnect();
  mqtt.loop();

  String dir = detectDir();
  if (dir == "")
    return;

  if (!rfid.PICC_IsNewCardPresent())
    return;
  if (!rfid.PICC_ReadCardSerial())
    return;

  beep();
  String uid = getUID();

  mqtt.publish("parking/request", (dir + ":" + uid).c_str());
  lastUidWaiting = uid;

  unsigned long t = millis();
  while (lastUidWaiting != "" && millis() - t < 3000)
  {
    mqtt.loop();
    delay(10);
  }
  lastUidWaiting = "";

  rfid.PICC_HaltA();
  rfid.PCD_StopCrypto1();
}
