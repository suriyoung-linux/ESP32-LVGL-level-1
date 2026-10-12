# รายงานอากาศบนจอสัมผัส ESP32 CYD

โปรแกรมมีหน้า **WEATHER**, **LDR / RGB** และ **DHT** โดยใช้จอสัมผัส XPT2046 บนบอร์ด
มีตัวเลือก **Chiang Mai**, **Bangkok** และ **WiFi area** (พื้นที่พักอาศัยโดยประมาณจาก IP)
ข้อความบน TFT เป็นภาษาอังกฤษตามฟอนต์ที่ติดตั้งอยู่

## เริ่มใช้งาน

1. ใส่ชื่อ WiFi 2.4 GHz รหัสผ่าน และ OpenWeather API key ใน `include/secrets.h` แล้วบันทึก
   (หากยังไม่มีไฟล์ ให้คัดลอกจาก `include/secrets.h.example`)
   เพิ่มบรรทัด `#define OPENWEATHER_API_KEY "YOUR_API_KEY"`
   key เดียวใช้กับ Current Weather และ Air Pollution สำหรับทั้งสามพื้นที่
   `secrets.h` ถูก `.gitignore` และไม่รวมในซอร์สบน GitHub
2. ปิด Serial Monitor เดิมด้วย `Ctrl+C` แล้วเลือก
   **Terminal → Run Task… → PlatformIO: Upload and Monitor (esp32dev)**
3. ครั้งแรกจอแสดง **Touch calibration**: กดค้างที่กากบาทประมาณครึ่งวินาที แล้วปล่อย
   ทำตามเป้าทั้ง 4 จุด: บนซ้าย → บนขวา → ล่างซ้าย → ล่างขวา
   หากตรวจจุดสุดท้ายไม่ผ่าน โปรแกรมจะให้ทำใหม่
4. เมื่อสำเร็จจะเปิด **Touch accuracy test** อัตโนมัติ ให้กดค้างแล้วปล่อยตามเป้าหมาย 5 จุด
   ผลแสดงระยะคลาดเคลื่อนแต่ละจุด ค่าเฉลี่ย และค่าสูงสุด; PASS เมื่อทุกจุดไม่เกิน 10 พิกเซล
   กด **BACK** กลับ SET แล้ว **BACK** อีกครั้งเพื่อกลับหน้า WEATHER
   ค่า calibration เก็บใน NVS จึงไม่ต้องทำใหม่ทุกครั้ง
   เปิดทดสอบซ้ำได้จาก **SET → TEST TOUCH**; หากตำแหน่งคลาดเคลื่อน เลือก **CAL TOUCH** เพื่อปรับเทียบใหม่
5. รอ WiFi เชื่อมต่อ รับ IPv4 และตั้งเวลาผ่าน NTP แล้วข้อมูลอากาศจะโหลดเบื้องหลัง
   ระหว่างโหลดสามารถเปลี่ยนหน้าและสั่งหยุด RGB ได้

หาก Serial ยังขึ้น `SSID NOT FOUND` ต้องแก้การเชื่อม WiFi ก่อน จอจะรายงานว่าข้อมูลยังไม่พร้อม
ไม่แสดงข้อมูลจำลองหรือแทนข้อมูลที่ขาดหายด้วยเลขศูนย์

## คลิกเลือกพื้นที่และชุดข้อมูล

| ปุ่มพื้นที่ | ตำแหน่งที่ใช้ |
|---|---|
| Chiang Mai | จุดกลางเมืองเชียงใหม่ 18.7883, 98.9853 |
| Bangkok | จุดกลางกรุงเทพฯ 13.7563, 100.5018 |
| WiFi area | พิกัดโดยประมาณจาก IP อินเทอร์เน็ตของเครือข่ายที่ ESP32 เชื่อมต่อ |

ข้อมูลสองจังหวัดเป็นตัวอย่างที่พิกัดกลางเมือง ไม่ใช่ค่าเฉลี่ยทั้งจังหวัด
WiFi area ใช้ HTTPS เรียก `ipwho.is` โดยไม่ส่งชื่อ WiFi, รหัสผ่าน หรือรายการ BSSID
ตำแหน่งอาจอยู่ที่ทางออกอินเทอร์เน็ตของผู้ให้บริการ หรือที่ตั้ง VPN ไม่ใช่บ้านจริง
จอแสดง `IP approx` และชื่อเมืองที่บริการระบุ ไม่มี GPS และไม่มีการวัดตำแหน่งบ้านโดยตรง
ตรวจตำแหน่งใหม่เมื่อ WiFi เชื่อมต่ออีกครั้ง และประมาณทุกหนึ่งชั่วโมง

| ปุ่มชุดข้อมูล | ค่าและหน่วย |
|---|---|
| Weather | อุณหภูมิ °C, ความชื้นสัมพัทธ์ %, คำอธิบายสภาพอากาศจาก OpenWeather condition ID |
| Rain | อัตราฝน `rain.1h` หน่วย mm/h และพื้นที่ปกคลุมเมฆ % |
| Wind | ความเร็วลม km/h และทิศที่ลมพัดมา ทั้งองศาและ N/NE/E/... |
| PM2.5 | ความเข้มข้น PM2.5 ในหน่วย µg/m³ จาก OpenWeather Air Pollution |

เวลาที่แถบบนคือเวลาท้องถิ่นปัจจุบันในรูปแบบ `HH:MM:SS` ใช้ NTP ร่วมกับ UTC offset ของตำแหน่งนั้น
แสดงคู่กับ `WiFi connected` และระดับสัญญาณ หรือ `Serial ready`; ไม่แสดง IPv4 บน TFT
แตะ `SET` เพื่อเลือกพักหน้าจอหลังไม่สัมผัส 2, 5, 10 นาที หรือ `Never sleep` ในหน้าตั้งค่าแยก
ค่าเก็บใน NVS; หรี่แสงเตือน 15 วินาทีก่อนครบเวลาพักจอ แล้วปิด backlight หากยังไม่สัมผัส
แตะเพื่อคืนความสว่างและเริ่มนับเวลาใหม่โดยไม่สั่งปุ่ม การพักจอไม่หยุดเซนเซอร์หรือ API
ตัวเลือก `Never sleep` ไม่หรี่แสงหรือพักจอ
แตะ `SET → LIGHT / DARK` เพื่อเลือก LIGHT, DARK หรือ AUTO
LIGHT/DARK คงสีที่เลือก; AUTO ใช้เวลาเดียวกับนาฬิกาบนจอ โดยค่าเริ่มต้น Light 07:00–19:00 และ Dark 19:00–07:00
ปุ่ม `− / +` ปรับเวลาเริ่มแต่ละโหมดครั้งละ 15 นาที โหมดและเวลาบันทึกใน NVS
ก่อนเวลา NTP พร้อม AUTO จะคงสีเดิม (Dark เมื่อเปิดเครื่องใหม่) พร้อมแจ้งว่ารอนาฬิกา
การเปลี่ยนสีไม่ปลุกจอหรือเริ่มนับเวลาพักใหม่ ดูรายละเอียดใน README หัวข้อ Light / Dark และการเปลี่ยนตามเวลา
แถบ WEATHER / LDR / RGB ใช้ตัวอักษรเล็กและขยับขึ้นเพื่อให้ข้อมูลอากาศมีพื้นที่มากขึ้น
ค่าข้อมูลหลักใช้ Font 2 ขยายสองเท่า แยกอุณหภูมิกับความชื้นคนละบรรทัด เพื่อลดชุดฟอนต์ใน Flash
ข้อความที่ยาวเกินพื้นที่จะลดฟอนต์เพื่อไม่ให้ล้นขอบจอ
`Data` เป็นเวลาของข้อมูล API แยกจากนาฬิกาปัจจุบัน และแยกเวลาของอากาศกับ PM2.5
ฝนแสดงอัตรา `mm/h` ตาม `rain.1h` ของ OpenWeather ไม่ใช่ฝนสะสมตลอดวัน
หากไม่มี field `rain` ในคำตอบที่ถูกต้อง API ระบุว่าไม่มีปรากฏการณ์ฝน จึงแสดง 0
PM2.5 ไม่มีช่วงเฉลี่ยระบุใน API จึงไม่แสดงช่วงเวลาเฉลี่ยที่สมมติขึ้น
ทิศลมที่ขาดหายแสดง `Direction unavailable` และ JSON เป็น `null` / `N/A`
ค่าอื่นที่ขาดหรือผิดช่วงถือว่าข้อมูลใช้ไม่ได้ และยังคง cache ที่สำเร็จก่อนหน้าโดยติดป้าย STALE
HTTP 401 หมายถึง key ไม่ถูกต้อง/ยังไม่พร้อมใช้งาน; HTTP 429 หมายถึงเกินข้อจำกัด API

อ่านข้อมูลทุก 10 นาที มี cache แยกสามพื้นที่ และแยกผลอากาศกับ PM2.5
แตะ **REFRESH** เพื่อขอใหม่ได้ โดยเว้นอย่างน้อย 30 วินาทีระหว่างคำขอของพื้นที่เดียวกัน
หากโหลดไม่สำเร็จ จะลองใหม่หลัง 5 นาที และเก็บข้อมูลเดิมพร้อมป้าย **STALE**
ข้อมูลอากาศและฝุ่นมาจากบริการ OpenWeather ส่วนค่าบนหน้า DHT วัดจากเซนเซอร์ GPIO27 โดยตรง

## หน้า LDR / RGB

แตะ **LDR / RGB** เพื่อดูค่า LDR, แรงดัน V, ค่าประมาณ R และรูปสถานะ LED ทั้งสามสี

- **STOP RGB**: ดับ LED ทั้งสามสี การวัด LDR และระบบอากาศยังทำงาน
- **START RGB**: เลือกสีจาก LDR ต่อทันที: >1000 แดง, 500–1000 เขียว, <500 น้ำเงิน
- สถานะ START/STOP เก็บใน NVS และใช้หลังเปิดเครื่องใหม่
- การสลับหน้าไม่มีผลต่อการทำงานของ LED

## หน้า DHT: ค่าจากเซนเซอร์ที่ GPIO27

แตะ **DHT** หรือปัดซ้าย–ขวาเพื่อสลับหน้า WEATHER / LDR-RGB / DHT แบบวนรอบ
หน้า DHT วัดอุณหภูมิ °C และความชื้นสัมพัทธ์ % ที่ตำแหน่งบอร์ดจริงทุก 2 วินาที
ใช้ DHT22 เป็นค่าเริ่มต้น; หากเป็น DHT11 เปลี่ยน `CYD_DHT_MODEL` เป็น `11` ใน `include/app_config.h`
แล้ว Build / Upload ใหม่ หน้าเซนเซอร์ทำงานได้แม้ WiFi ไม่เชื่อมต่อ

เมื่อไม่ได้ต่ออุปกรณ์หรืออ่านล้มเหลว จอแสดง **No sensor / sensor error** พร้อมค่า `--.-`
ไม่ใช้ค่าที่อ่านรอบก่อนเป็นค่าปัจจุบัน และจะลองอ่านใหม่อัตโนมัติทุก 2 วินาที
อ่านคำเตือนใน Serial ได้เช่นกัน การอ่านล้มเหลวไม่สามารถจำแนกสาเหตุแน่ชัดระหว่างสาย/อุปกรณ์/การไม่เชื่อมต่อ
การต่อสายและ pull-up อยู่ใน [README](README.md#อ่านอุณหภูมิและความชื้นจริงจาก-dht11--dht22)
JSON API ด้านล่างยังส่งข้อมูล WEATHER จาก OpenWeather; ค่า DHT แสดงในหน้าเซนเซอร์และ Serial

## ประสิทธิภาพและเวอร์ชัน

เฟิร์มแวร์ 1.1.0 กรอง JSON ของ provider เหลือเฉพาะข้อมูลที่ใช้
บัฟเฟอร์ลดจาก 4 KB เป็น 1 KB โดยยังตรวจช่องที่หาย/ผิดรูปแบบ
หน้า WEATHER อัปเดตข้อมูลเมื่อ cache เปลี่ยนและวาดนาฬิกาเฉพาะมุมล่างทุกวินาที
หน้า DHT และ RGB ไม่วาดค่าซ้ำเมื่อข้อมูลไม่เปลี่ยน

`GET /api/status` คืน `firmware_version`, `heap_free_bytes`, `heap_min_free_bytes`,
`heap_largest_block_bytes` และ `weather_stack_min_free_bytes`
ช่อง stack เป็นค่าต่ำสุดตั้งแต่ worker เริ่มงาน โดยเป็น `null` ก่อนมีผลครั้งแรก
รายละเอียดขนาด Build และข้อจำกัดการตรวจอยู่ใน [OPTIMIZATION_REPORT.md](OPTIMIZATION_REPORT.md)

## JSON API ของ ESP32

เปิดจากอุปกรณ์ในเครือข่ายเดียวกัน โดยใช้ IPv4 ที่ Serial แสดงแทน `192.168.1.123`
เปิด `/` เพื่อดูหน้าเว็บแดชบอร์ดตั้งแต่เฟิร์มแวร์ 1.2.0; endpoint ด้านล่างคืน JSON
วิธีเปิดจากมือถือ/คอมพิวเตอร์อยู่ใน [WEB_BROWSER_GUIDE.md](WEB_BROWSER_GUIDE.md)

| Method | Endpoint | ผล |
|---|---|---|
| GET | `/` | หน้าเว็บอากาศและเซนเซอร์ |
| GET | `/api/sensors` | ค่า DHT / LDR ล่าสุด, สถานะอ่านและ RGB |
| GET | `/api/status` | เวอร์ชันเฟิร์มแวร์, WiFi, IPv4, นาฬิกา, worker, RGB และ heap/stack จริง |
| GET | `/api/locations` | รายการพื้นที่ พิกัด แหล่งตำแหน่ง และเวลาท้องถิ่น |
| GET | `/api/weather?location=chiang_mai` | ข้อมูลเชียงใหม่ |
| GET | `/api/weather?location=bangkok` | ข้อมูลกรุงเทพฯ |
| GET | `/api/weather?location=home` | ข้อมูลตำแหน่งประมาณจากเครือข่าย WiFi |
| GET | `/api/rgb` | สถานะการทำงานของ RGB |
| POST | `/api/rgb` | สั่ง RGB ด้วย JSON `{"enabled":true}` หรือ `{"enabled":false}` |

```bash
curl 'http://192.168.1.123/api/weather?location=chiang_mai'
curl -X POST 'http://192.168.1.123/api/rgb' \
  -H 'Content-Type: application/json' -d '{"enabled":false}'
```

ผลอากาศแยกเป็น `location`, `weather`, `rain_cloud`, `wind`, `air_quality`, `status` และ `source`
ข้อมูลที่ใช้ไม่ได้เป็น `null`; ตรวจ `weather_available`, `air_available` และป้าย stale ประกอบ
`weather_data_time_utc` และ `air_data_time_utc` เป็น Unix timestamp ใน UTC
`rain_cloud.rain_rate_mm_per_hour` เป็นอัตราฝน; `air_quality.interval_seconds` เป็น `null` เพราะผู้ให้บริการไม่ได้ระบุ
`local_time` ใช้ `timezone` และ `utc_offset_seconds` ที่คืนมาด้วย

HTTP 200 หมายถึงมีข้อมูลอย่างน้อยหนึ่งชุด (อาจมี cache เก่า ต้องตรวจ stale)
HTTP 202 หมายถึงยังไม่มีข้อมูลและคำขอกำลังรอผล
HTTP 503 หมายถึงข้อมูลยังไม่พร้อมและไม่ได้อยู่ระหว่างโหลด เช่น ไม่มี WiFi หรือ NTP ยังไม่ตั้งเวลา
HTTP 400 หมายถึงชื่อพื้นที่/JSON คำสั่งไม่ถูกต้อง และ HTTP 404 หมายถึง endpoint ไม่มีอยู่

## โครงสร้างและการตรวจ

- `src/main.cpp`: วัด LDR, ควบคุม LED, WiFi, NTP และ REST API
- `src/dashboard.cpp`: หน้า TFT, ปุ่มสัมผัส และ calibration
- `include/touch_accuracy.h`: วัดระยะคลาดเคลื่อน 5 จุดด้วยตัวอย่างสัมผัสที่นิ่ง และเกณฑ์ PASS 10 พิกเซล
- `include/display_theme.h`: ชุดสี Light/Dark และช่วงเวลาของโหมด Auto
- `src/mqtt_service.cpp`: HiveMQ MQTT สำหรับ Temp/Humidity พร้อม Last Will, ACK และ reconnect; ดู Topic ใน `MQTT_GUIDE.md`
- `src/data_logger.cpp`: ตรวจ microSD, บันทึก CSV ทุก 1 นาที และคืนบัสให้จอสัมผัส
- `src/weather_service.cpp`: FreeRTOS worker สำหรับ HTTPS และ cache
- `include/weather_data.h`: ตรวจ JSON, พิกัด, weather code และทิศลม
- `include/weather_config.h`: ช่วงอัปเดตและ timeout
- `include/api_root_certificates.h`: CA roots สำหรับยืนยัน HTTPS; ไม่ใช้ `setInsecure()`

จอใช้ HSPI ตามเดิม จอสัมผัสและ microSD ใช้ VSPI ตัวเดียวแต่สลับขาตาม `board_config.h`
เลือก `SET → LOG ON/LOG OFF` เมื่อพบการ์ด; หากไม่พบจะแสดงสถานะและปุ่ม `CHECK SD`
เริ่มเปิดเครื่องหรือใส่การ์ดกลับจะไม่เปิดบันทึกเอง ต้องให้ผู้ใช้เลือกก่อน
ไฟล์ `/weather_log.csv` เขียนต่อท้ายทุก 1 นาที มีข้อมูลอากาศ 3 พื้นที่พร้อมอุณหภูมิ/ความชื้น DHT11 หรือ DHT22
สถานะ `stale`/`unavailable` แยกข้อมูลเก่าและอ่านไม่ได้; การพักจอไม่หยุดบันทึก
ดูรายละเอียดคอลัมน์และวิธีนำการ์ดออกใน README หัวข้อบันทึก CSV ลง microSD
คำเตือน `TOUCH_CS` ของ TFT_eSPI หมายถึง touch helper ของ TFT_eSPI ไม่ได้เปิด
โปรแกรมนี้ใช้ XPT2046_Touchscreen แยกบนอีกบัส

```bash
"$HOME/.platformio/penv/bin/platformio" run -e esp32dev -j 1
python3 scripts/check_weather.py
python3 scripts/check_weather_service.py
python3 scripts/check_dht.py
python3 scripts/check_logger.py
python3 scripts/check_mqtt.py
```

การตรวจ native ใช้ g++ พร้อม AddressSanitizer/UndefinedBehaviorSanitizer กับฟังก์ชันจริง
ทดสอบ JSON provider, ค่าขาดหาย, หน่วยฝน, การแปลง touch, GPIO START/STOP และ API handlers
fixture ใน `tests/fixtures` เป็นผล API ที่บันทึกไว้เพื่อให้ตรวจซ้ำได้โดยไม่พึ่งอินเทอร์เน็ต
ผลตรวจนี้ไม่แทนการตรวจการสัมผัสและเครือข่ายบนบอร์ดจริง

Ubuntu เครื่องนี้เคยมีพื้นที่ root เต็ม จึงตั้ง `build_dir` เป็น `/tmp/cyd-esp32-weather-build`
PlatformIO สร้าง Build ใหม่ได้หลังรีบูต แต่ไฟล์ใน `/tmp` อาจหายได้
ย้ายเฉพาะ download cache เดิมไป `/tmp/cyd-platformio-download-cache-preserved` เพื่อคืนพื้นที่ root
โค้ด ไลบรารีในโปรเจกต์ และ `secrets.h` ยังอยู่ในโปรเจกต์

แหล่งข้อมูล: [OpenWeather Current Weather](https://openweathermap.org/current),
[OpenWeather Air Pollution](https://openweathermap.org/api/air-pollution),
[OpenWeather condition codes](https://openweathermap.org/weather-conditions),
[IP geolocation](https://ipwhois.io/documentation),
[XPT2046_Touchscreen](https://github.com/PaulStoffregen/XPT2046_Touchscreen)
