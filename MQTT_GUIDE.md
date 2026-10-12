# MQTT ผ่าน HiveMQ Public Broker

ESP32 ส่งอุณหภูมิและความชื้นจริงจาก DHT11/DHT22 ผ่าน MQTT พร้อมสถานะบนจอ TFT
ใช้ [HiveMQ Public Broker](https://www.hivemq.com/mqtt/public-mqtt-broker/) แบบไม่ต้องสมัครบัญชีหรือกรอก username/password
เป็น broker สาธารณะสำหรับทดลอง ค่าที่เผยแพร่อ่านได้โดยผู้ที่รู้ Topic; พอร์ต TCP 1883 ใช้ MQTT แบบไม่เข้ารหัส
การรับข้อมูลอาศัยเครือข่ายและบริการภายนอก จึงต้องดูสถานะเชื่อมต่อและความสดของข้อมูลร่วมด้วย

## การเชื่อมต่อ

| รายการ | ค่า |
|---|---|
| Host | `broker.hivemq.com` |
| Port | `1883` |
| Protocol | MQTT 3.1.1 / TCP |
| Username / Password | เว้นว่าง |
| Publish | ทุก 10 วินาทีหลังเชื่อมต่อ โดยอ่าน cache ของ DHT |
| QoS / Retain | QoS 1 / Retained ทุก Topic |
| Keepalive / Reconnect | 30 วินาที / ลองใหม่ทุก 15 วินาที |

เปิด Serial Monitor 115200 baud หรือแตะ **SET → MQTT** เพื่อดู Topic จริงของบอร์ด
แต่ละบอร์ดสร้าง ID สุ่ม เช่น `cyd-1234abcd` แล้วเก็บใน NVS จึงใช้ Topic เดิมหลังรีสตาร์ต
ID ไม่ได้มาจาก MAC, ชื่อ WiFi หรือพิกัด หากล้าง NVS จะสร้าง ID ใหม่
ค่าตัวอย่างด้านล่างต้องเปลี่ยนเป็น ID ที่บอร์ดของคุณแสดง

## Topic ที่ใช้

กำหนด `<root>` เป็น `esp32/cyd/<device-id>` เช่น `esp32/cyd/cyd-1234abcd`

| Topic | Payload | หน่วย/ความหมาย |
|---|---|---|
| `<root>/temperature` | `26.50` หรือ `null` | อุณหภูมิ DHT, °C |
| `<root>/humidity` | `61.00` หรือ `null` | ความชื้นสัมพัทธ์ DHT, % |
| `<root>/telemetry` | JSON ตามตัวอย่าง | อุณหภูมิ ความชื้น และคุณภาพการอ่าน |
| `<root>/status` | `online` / `offline` | สถานะการเชื่อมต่อ MQTT ของ ESP32 |
| `<root>/#` | Subscribe wildcard | รับทุก Topic ของบอร์ด |

ตัวอย่าง `/telemetry`:

```json
{"valid":true,"status":"ready","temperature_c":26.5,"humidity_percent":61}
```

เมื่อ DHT ยังไม่เคยอ่าน, อ่านไม่ได้ หรือค่าขาดการอัปเดต:

```json
{"valid":false,"status":"unavailable","temperature_c":null,"humidity_percent":null}
```

`status` ของ telemetry ใช้ `waiting`, `ready`, `unavailable` หรือ `stale`
`stale` หมายถึงค่า DHT ล่าสุดเก่ากว่า 3 วินาที (`DHT_READ_INTERVAL_MS + 1000`)
ค่า scalar ใน `/temperature` และ `/humidity` จะเป็น `null` ด้วยเมื่อข้อมูลไม่พร้อม; ค่า 0 ที่อ่านได้จริงยังส่งเป็น `0.00`
`online` บอกว่า MQTT เชื่อมต่ออยู่ และไม่ใช่การรับรองว่า DHT อ่านได้ ต้องดู `telemetry.valid` ร่วมด้วย

เมื่อ broker ตรวจพบการหลุดโดยไม่ส่ง DISCONNECT จะเผยแพร่ Last Will `offline` แบบ Retained
อาจมีช่วงรอการตรวจพบตาม keepalive และเครือข่าย สถานะไม่ได้เปลี่ยนทันทีที่ถอดสายไฟ
Retained message คือค่าล่าสุดที่ broker เก็บไว้ ไม่ใช่ข้อมูลใหม่ทุกครั้งที่ subscribe
ถ้าอุปกรณ์ offline ค่าอุณหภูมิ/ความชื้นที่ค้างอยู่ต้องอ่านร่วมกับ `/status`; broker สาธารณะอาจรีสตาร์ตและล้างข้อมูล
QoS 1 อาจส่งซ้ำได้ และ ACK ยืนยันว่า broker รับข้อความ ไม่ใช่ยืนยันว่าโปรแกรมปลายทางประมวลผลแล้ว

## การแสดงผลบนจอ

แถบบนมี `MQTT` และสถานะ:

- `OK`: เชื่อมต่อ broker
- `WAIT`: กำลังเชื่อมต่อหรือลองเชื่อมต่อใหม่
- `OFF`: รอ WiFi
- `ERR`: เริ่ม service ไม่ได้ หรือส่งเข้าคิวไม่สำเร็จ

**SET → MQTT** แสดง host, root Topic, suffix, จำนวนชุดที่เข้าคิว, จำนวน ACK และอายุ ACK ล่าสุด
การเชื่อมต่อ/ส่งข้อความทำใน task ของ ESP-MQTT; main loop ส่งเข้าคิวและไม่วนรอ TCP
เมื่อคิวชุดก่อนยังรอ ACK จะพักการส่งชุดใหม่เพื่อจำกัด RAM และไม่ได้สะสมค่าทุก 10 วินาทีขณะ offline
ระบบ MQTT ทำงานต่อขณะหรี่/พักจอ และสถานะ MQTT ไม่ปลุกจอหรือเปลี่ยนเวลาสัมผัสล่าสุด
ใช้ **BACK** กลับ SET แล้ว **BACK** อีกครั้งกลับหน้าหลัก

## ทดลอง Subscribe

หากเครื่องมี `mosquitto_sub` อยู่แล้ว:

```bash
mosquitto_sub -h broker.hivemq.com -p 1883 -t 'esp32/cyd/cyd-1234abcd/#' -v
```

หรือใช้ [HiveMQ Browser Client](https://www.hivemq.com/demos/websocket-client/)
โดยเลือก host `broker.hivemq.com`, WebSocket port `8000`, client ID ที่ต่างจากบอร์ด, เว้น username/password ว่าง
แล้ว Subscribe root ของบอร์ดตามด้วย `/#`
พอร์ต WebSocket เป็นคนละพอร์ตกับ TCP 1883; ตรวจค่าปัจจุบันได้ที่ [HiveMQ MQTT Dashboard](https://broker.hivemq.com/)

## Build และทดสอบ

```bash
"$HOME/.platformio/penv/bin/platformio" run -e esp32dev -j 1
python3 scripts/check_mqtt.py
python3 scripts/check_dht.py
```

ใช้ ESP-MQTT ที่มากับ framework Arduino-ESP32/ESP-IDF ของโปรเจกต์ จึงไม่ต้องเพิ่มไลบรารี MQTT ภายนอกใน `platformio.ini`
`mqtt_config.h` กำหนด broker และรอบส่ง; `mqtt_payload.h` กำหนดข้อมูล DHT; `mqtt_service.cpp` จัดการคิว/สถานะ/Last Will
ชุดทดสอบ native ใช้ MQTT backend จำลองกับโค้ด service จริง ตรวจ anonymous connection, การเริ่มไม่สำเร็จ,
reconnect, ACK, คิวค้าง, การอ่านเก่า/ขาดหาย/ค่า 0, รอบส่งและ millis wrap รวมถึง UI บนจอ
ผลนี้ไม่แทนการทดสอบการเชื่อมต่อ HiveMQ และจอสัมผัสบนบอร์ดจริงหลังอัปโหลด

## ขอบเขตข้อมูล MQTT

ตามตัวเลือกของผู้ใช้ ส่งเฉพาะ Temp/Humidity จาก DHT11/DHT22 พร้อมคุณภาพการอ่านและสถานะการเชื่อมต่อ MQTT
ข้อมูล LDR และสถานะ RGB ใช้งานบนอุปกรณ์ตามเดิม แต่ไม่ส่งผ่าน MQTT
ค่า RSSI, heap, firmware, WiFi SSID/password, IP, MAC และ API key ไม่อยู่ใน payload
