import paho.mqtt.client as mqtt
import mysql.connector
import json
from datetime import datetime

MQTT_BROKER = "172.20.10.3"
MQTT_PORT = 1883

DB_CONFIG = {
    'host': 'localhost',
    'user': 'root',
    'password': 'LONGnguyen2004',
    'database': 'parking_system'
}

db = mysql.connector.connect(**DB_CONFIG)
cursor = db.cursor()

def to_dt(ts):
    try:
        return datetime.fromisoformat(ts)
    except:
        return datetime.now()

def on_message(client, userdata, msg):
    topic = msg.topic
    payload = msg.payload.decode()

    print("RX", topic, payload)

    if topic == "parking/event":
        try:
            data = json.loads(payload)
        except json.JSONDecodeError:
            print("Invalid JSON")
            return

        uid = data.get("uid")
        action = data.get("action")   # ENTRY | EXIT
        status = data.get("status")   # OK | FAIL
        ts = data.get("timestamp")
        dt = to_dt(ts)

        # Log ghi lại event
        cursor.execute(
            "INSERT INTO parking_logs (uid, action, status, event_time) VALUES (%s,%s,%s,%s)",
            (uid, action, status, dt)
        )
        db.commit()

        # ---- HANDLE ENTRY ----
        if action == "ENTRY" and status == "OK":
            cursor.execute(
                "INSERT INTO parking_state (uid, entry_time) VALUES (%s,%s) "
                "ON DUPLICATE KEY UPDATE entry_time=%s",
                (uid, dt, dt)
            )
            db.commit()

        # ---- HANDLE EXIT ----
        elif action == "EXIT" and status == "OK":
            cursor.execute("SELECT entry_time FROM parking_state WHERE uid=%s", (uid,))
            row = cursor.fetchone()

            if row:
                entry_time = row[0]
                duration = dt - entry_time
                print(f"{uid} stayed {duration}")

                cursor.execute("DELETE FROM parking_state WHERE uid=%s", (uid,))
                db.commit()

def on_connect(client, userdata, flags, rc):
    print("Connected to MQTT", rc)
    client.subscribe("parking/event")

client = mqtt.Client()
client.on_connect = on_connect
client.on_message = on_message

client.connect(MQTT_BROKER, MQTT_PORT, 60)
client.loop_forever()
