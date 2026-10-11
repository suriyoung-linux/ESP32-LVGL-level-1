# รายงานอากาศบนจอสัมผัส ESP32 CYD

โปรแกรมเปิดหน้า **WEATHER** และแยกหน้า **LDR / RGB** โดยใช้จอสัมผัส XPT2046 บนบอร์ด
มีตัวเลือก **Chiang Mai**, **Bangkok** และ **WiFi area** (พื้นที่พักอาศัยโดยประมาณจาก IP)
ข้อความบน TFT เป็นภาษาอังกฤษตามฟอนต์ที่ติดตั้งอยู่

## เริ่มใช้งาน

1. ใส่ชื่อ WiFi 2.4 GHz และรหัสผ่านใน `include/secrets.h` แล้วบันทึก
2. ปิด Serial Monitor เดิมด้วย `Ctrl+C` แล้วเลือก
   **Terminal → Run Task… → PlatformIO: Upload and Monitor (esp32dev)**
3. ครั้งแรกจอแสดง **Touch calibration**: กดค้างที่กากบาทประมาณครึ่งวินาที แล้วปล่อย
   ทำตามเป้าทั้ง 4 จุด: บนซ้าย → บนขวา → ล่างซ้าย → ล่างขวา
   หากตรวจจุดสุดท้ายไม่ผ่าน โปรแกรมจะให้ทำใหม่
4. เมื่อสำเร็จจะแสดงหน้า WEATHER ค่า calibration เก็บใน NVS จึงไม่ต้องทำใหม่ทุกครั้ง
   หากตำแหน่งสัมผัสคลาดเคลื่อน ให้แตะ **CAL** มุมบนขวา
5. รอ WiFi เชื่อมต่อ รับ IPv4 และตั้งเวลาผ่าน NTP แล้วข้อมูลอากาศจะโหลดเบื้องหลัง
   ระหว่างโหลดสามารถเปลี่ยนหน้าและสั่งหยุด RGB ได้

หากยังขึ้น `SSID NOT FOUND` ต้องแก้การเชื่อม WiFi ก่อน จอจะรายงานว่าข้อมูลยังไม่พร้อม
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
| Weather | อุณหภูมิ °C, ความชื้นสัมพัทธ์ %, คำอธิบายสภาพอากาศจาก WMO code |
| Rain | ฝน `rain + showers` หน่วย mm ในช่วงเวลาของข้อมูล และพื้นที่ปกคลุมเมฆ % |
| Wind | ความเร็วลม km/h และทิศที่ลมพัดมา ทั้งองศาและ N/NE/E/... |
| PM2.5 | ความเข้มข้น PM2.5 ในหน่วย µg/m³ จากแบบจำลอง CAMS |

วันเดือนปีและเวลาที่มุมล่างซ้ายคือเวลาท้องถิ่นปัจจุบัน ใช้ NTP ร่วมกับเขตเวลาและ UTC offset ของตำแหน่งนั้น
แถบ WEATHER / LDR / RGB ใช้ตัวอักษรเล็กและขยับขึ้นเพื่อให้ข้อมูลอากาศมีพื้นที่มากขึ้น
ค่าข้อมูลหลักใช้ฟอนต์สูง 26 พิกเซลจากเดิม 16 พิกเซล แยกอุณหภูมิกับความชื้นคนละบรรทัด
ข้อความที่ยาวเกินพื้นที่จะลดฟอนต์เพื่อไม่ให้ล้นขอบจอ
`Model` เป็นเวลาของข้อมูล API แยกจากนาฬิกาปัจจุบัน และแยกเวลาของอากาศกับ PM2.5
ฝนแสดงช่วงเวลาจริง เช่น `mm / 15 min` ไม่ใช่ฝนสะสมตลอดวัน

อ่านข้อมูลทุก 10 นาที มี cache แยกสามพื้นที่ และแยกผลอากาศกับ PM2.5
แตะ **REFRESH** เพื่อขอใหม่ได้ โดยเว้นอย่างน้อย 30 วินาทีระหว่างคำขอของพื้นที่เดียวกัน
หากโหลดไม่สำเร็จ จะลองใหม่หลัง 5 นาที และเก็บข้อมูลเดิมพร้อมป้าย **STALE**
ข้อมูลอากาศและฝุ่นเป็นค่าจากแบบจำลองของบริการ ไม่ใช่ค่าที่เซนเซอร์บน CYD วัดเอง

## หน้า LDR / RGB

แตะ **LDR / RGB** เพื่อดูค่า LDR, แรงดัน V, ค่าประมาณ R และรูปสถานะ LED ทั้งสามสี

- **STOP RGB**: ดับ LED ทั้งสามสี การวัด LDR และระบบอากาศยังทำงาน
- **START RGB**: เลือกสีจาก LDR ต่อทันที: >1000 แดง, 500–1000 เขียว, <500 น้ำเงิน
- สถานะ START/STOP เก็บใน NVS และใช้หลังเปิดเครื่องใหม่
- การสลับหน้าไม่มีผลต่อการทำงานของ LED

## JSON API ของ ESP32

เปิดจากอุปกรณ์ในเครือข่ายเดียวกัน โดยใช้ IPv4 ที่จอ/Serial แสดงแทน `192.168.1.123`
API นี้ส่ง JSON; การเลือกหน้าจอใช้ TFT จึงไม่มีหน้าเว็บแดชบอร์ด

| Method | Endpoint | ผล |
|---|---|---|
| GET | `/api/status` | สถานะ WiFi, IPv4, นาฬิกา, worker และ RGB |
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
`weather_model_time_utc` และ `air_model_time_utc` เป็น Unix timestamp ใน UTC
`local_time` ใช้ `timezone` และ `utc_offset_seconds` ที่คืนมาด้วย

HTTP 200 หมายถึงมีข้อมูลอย่างน้อยหนึ่งชุด (อาจมี cache เก่า ต้องตรวจ stale)
HTTP 202 หมายถึงยังไม่มีข้อมูลและคำขอกำลังรอผล
HTTP 503 หมายถึงข้อมูลยังไม่พร้อมและไม่ได้อยู่ระหว่างโหลด เช่น ไม่มี WiFi หรือ NTP ยังไม่ตั้งเวลา
HTTP 400 หมายถึงชื่อพื้นที่/JSON คำสั่งไม่ถูกต้อง และ HTTP 404 หมายถึง endpoint ไม่มีอยู่

## โครงสร้างและการตรวจ

- `src/main.cpp`: วัด LDR, ควบคุม LED, WiFi, NTP และ REST API
- `src/dashboard.cpp`: หน้า TFT, ปุ่มสัมผัส และ calibration
- `src/weather_service.cpp`: FreeRTOS worker สำหรับ HTTPS และ cache
- `include/weather_data.h`: ตรวจ JSON, พิกัด, weather code และทิศลม
- `include/weather_config.h`: ช่วงอัปเดตและ timeout
- `include/api_root_certificates.h`: CA roots สำหรับยืนยัน HTTPS; ไม่ใช้ `setInsecure()`

จอใช้ HSPI ตามเดิม จอสัมผัสใช้ VSPI ตามขาใน `board_config.h` และยังไม่ได้ใช้ microSD
คำเตือน `TOUCH_CS` ของ TFT_eSPI หมายถึง touch helper ของ TFT_eSPI ไม่ได้เปิด
โปรแกรมนี้ใช้ XPT2046_Touchscreen แยกบนอีกบัส

```bash
"$HOME/.platformio/penv/bin/platformio" run -e esp32dev -j 1
python3 scripts/check_weather.py
```

การตรวจ native ใช้ g++ พร้อม AddressSanitizer/UndefinedBehaviorSanitizer กับฟังก์ชันจริง
ทดสอบ JSON provider, ค่าขาดหาย, หน่วยฝน, การแปลง touch, GPIO START/STOP และ API handlers
fixture ใน `tests/fixtures` เป็นผล API ที่บันทึกไว้เพื่อให้ตรวจซ้ำได้โดยไม่พึ่งอินเทอร์เน็ต
ผลตรวจนี้ไม่แทนการตรวจการสัมผัสและเครือข่ายบนบอร์ดจริง

Ubuntu เครื่องนี้พื้นที่ root เต็ม จึงตั้ง `build_dir` เป็น `/tmp/cyd-esp32-weather-build`
PlatformIO สร้าง Build ใหม่ได้หลังรีบูต แต่ไฟล์ใน `/tmp` อาจหายได้
ย้ายเฉพาะ download cache เดิมไป `/tmp/cyd-platformio-download-cache-preserved` เพื่อคืนพื้นที่ root
โค้ด ไลบรารีในโปรเจกต์ และ `secrets.h` ยังอยู่ในโปรเจกต์

แหล่งข้อมูล: [Open-Meteo Weather](https://open-meteo.com/en/docs),
[Open-Meteo Air Quality / CAMS](https://open-meteo.com/en/docs/air-quality-api),
[CAMS Global Atmospheric Composition Forecasts](https://ads.atmosphere.copernicus.eu/datasets/cams-global-atmospheric-composition-forecasts),
[IP geolocation](https://ipwhois.io/documentation),
[XPT2046_Touchscreen](https://github.com/PaulStoffregen/XPT2046_Touchscreen)
