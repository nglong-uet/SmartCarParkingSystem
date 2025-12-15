# 🚗 Smart Parking System using ESP8266, MQTT & Firebase
## 📌 Giới thiệu
Hệ thống **Smart Parking** cho phép quản lý ra/vào bãi đỗ xe tự động bằng **RFID**, **cảm biến IR**, **MQTT** và **Firebase Realtime Database**.

Hệ thống gồm **2 ESP8266 độc lập**:
* **ESP8266 Gate**: điều khiển barrier, đọc RFID, phát hiện hướng xe
* **ESP8266 Sensors**: xử lý logic, kiểm tra Firebase, quản lý slot & ghi log

---

## 🏗️ Kiến trúc tổng thể hệ thống
### 🔧 Thành phần phần cứng
* ESP8266 (NodeMCU / ESP-12E)
* RFID RC522
* Servo barrier
* Buzzer
* 2 IR xác định hướng (ENTRY / EXIT)
* 4 IR kiểm tra slot
* LCD I2C 16x2
* Router WiFi
* Server chạy **Mosquitto MQTT**

---

### 📡 Sơ đồ kiến trúc (Architecture Diagram)
```
+--------------------+        MQTT         +----------------------+
|  ESP8266 GATE      |  ───────────────▶  |  ESP8266 SENSORS     |
|                    |  parking/request   |                      |
| - RFID RC522       |                    | - Firebase RTDB      |
| - IR (Entry/Exit)  |  ◀───────────────  | - Slot IR            |
| - Servo Barrier    |  parking/response  | - LCD I2C            |
| - Buzzer           |                    |                      |
+--------------------+                    +----------------------+
            │                                          │
            │ WiFi                                     │ WiFi
            ▼                                          ▼
      +------------------+                    +------------------+
      |  MQTT Broker     |                    | Firebase RTDB    |
      |  (Mosquitto)     |                    |                  |
      +------------------+                    +------------------+
```

---

## 🔄 Luồng hoạt động hệ thống (Flow)
### 1️⃣ Xe vào / ra bãi
* IR phát hiện xe → xác định **ENTRY / EXIT**
* RFID đọc UID thẻ

### 2️⃣ ESP Gate gửi MQTT
```
Topic: parking/request
Payload: ENTRY:UID
Payload: EXIT:UID
```

### 3️⃣ ESP Sensors xử lý
* Kiểm tra thẻ hợp lệ (`/cards/{uid}/active`)
* Kiểm tra slot còn trống
* Kiểm tra xe đã ở trong bãi hay chưa
* Ghi log Firebase

### 4️⃣ ESP Sensors phản hồi
```
Topic: parking/response
Payload: UID:OK
Payload: UID:NO
```

### 5️⃣ ESP Gate
* OK → mở barrier
* NO → buzzer cảnh báo

---

## 🔀 Flowchart hệ thống
```
[Xe đến]
   ↓
[IR phát hiện]
   ↓
[Xác định ENTRY / EXIT]
   ↓
[Đọc RFID]
   ↓
[Gửi MQTT request]
   ↓
[ESP Sensors xử lý]
   ↓
[Kiểm tra Firebase]
   ↓
[OK ?]
  ├─ YES → Mở barrier → Ghi log
  └─ NO  → Buzzer → Ghi log
```

---

## 🗂️ Cấu trúc thư mục project
```
SMARTPARKING/
│
├── ESP8266_Gate/
│   ├── src/main.cpp
│   ├── include/
│   ├── lib/
│   └── platformio.ini
│
├── ESP8266_Sensors/
│   ├── src/main.cpp
│   ├── include/
│   ├── lib/
│   └── platformio.ini
│
├── mosquitto/
│   ├── docker-compose.yml
│   └── mosquitto.conf
│
├── .gitignore
└── README.md
```

---

## 🔥 Firebase Realtime Database Structure
```json
{
  "cards": {
    "B3_D6_XX_YY": {
      "active": true
    }
  },
  "parking": {
    "inside": {
      "B3_D6_XX_YY": {
        "time": "00:10:32"
      }
    }
  },
  "logs": {
    "-Nabc123": {
      "uid": "B3 D6 XX YY",
      "action": "ENTRY",
      "status": "OK",
      "time": "00:10:32"
    }
  }
}
```

---

## 🔐 Firebase Security Rules
```json
{
  "rules": {
    ".read": true,
    ".write": true,

    "cards": {
      "$uid": {
        ".read": true,
        ".write": true
      }
    },

    "parking": {
      "inside": {
        "$uid": {
          ".read": true,
          ".write": true
        }
      }
    },

    "logs": {
      ".read": true,
      ".write": true
    }
  }
}
```

👉 Khi triển khai thực tế:
✔️ dùng **Authentication + rules theo auth != null**

---

## 🧪 Hướng dẫn chạy hệ thống
### 1️⃣ MQTT Broker (Mosquitto)
Khởi động Docker
Vào VS Code, mở terminal chạy lệnh sau:
```bash
cd mosquitto
docker compose up -d
```

Broker chạy cổng:
```
1883
```

---

### 2️⃣ ESP8266 Gate
* Sửa lại ssid, pass, MQTT_SERVER theo máy
* Nạp code `ESP8266_Gate`
* Kết nối:
  * RFID RC522
  * Servo
  * IR trước / sau
  * Buzzer
* Subscribe: `parking/response`
* Publish: `parking/request`

---

### 3️⃣ ESP8266 Sensors
* Sửa lại ssid, pass, MQTT_SERVER theo máy
* Nạp code `ESP8266_Sensors`
* Kết nối:
  * LCD I2C
  * IR slot
* Firebase:
  * API KEY
  * Database URL
* Subscribe: `parking/request`
* Publish: `parking/response`

---

**Smart Parking System – ESP8266 + MQTT + Firebase**
Phục vụ học tập & đồ án IoT / Embedded Systems
