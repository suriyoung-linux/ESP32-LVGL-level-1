# เปิดดู ESP32 ผ่านเว็บเบราว์เซอร์ — Firmware 1.2.0

ESP32 ให้บริการหน้าเว็บที่ `http://IP-ของ-บอร์ด/` บน port 80
ใช้ Chrome, Edge, Firefox หรือ Safari จากมือถือ/คอมพิวเตอร์ในเครือข่ายเดียวกัน
ไม่ต้อง SSH เข้า Ubuntu เพื่อดูค่า หลังบอร์ดมีไฟและเชื่อม WiFi แล้ว

## เริ่มใช้งาน

1. ตรวจ WiFi และ OpenWeather key ใน `include/secrets.h` แล้ว Build / Upload
   ผ่าน PlatformIO ใน VS Code
2. ดู IPv4 บนจอ TFT หรือบรรทัด Serial `Web dashboard ready: http://.../`
3. เปิด URL นั้นจากอุปกรณ์ที่เชื่อมเครือข่ายเดียวกัน ใช้ `http://` เช่น
   `http://192.168.1.123/` โดยแทน IP ด้วยค่าจริงของบอร์ด
4. ค่า DHT22 และ LDR จะแสดงหลังได้ข้อมูลรอบแรก
   อากาศต้องมี WiFi, เวลา NTP และ API key ที่ใช้ได้ก่อน

ไม่ต้องเพิ่ม API key ในเบราว์เซอร์ หน้าเว็บอ่าน JSON จาก ESP32 และ ESP32 ติดต่อ OpenWeather เอง
IP อาจเปลี่ยนเมื่อ router ให้ DHCP ใหม่ ให้ใช้ IP ล่าสุดที่บอร์ดแสดง
ถ้าหน้าเว็บเข้าไม่ได้ ให้ตรวจเครือข่าย, IP, ไฟบอร์ด และ Guest WiFi/client isolation

## ค่าและการอัปเดต

| ส่วน | แหล่งข้อมูล / การแสดง |
|---|---|
| อุณหภูมิ/ความชื้นที่บอร์ด | DHT22 GPIO27; รุ่นเปลี่ยนตาม `CYD_DHT_MODEL` |
| LDR | GPIO34, raw ADC, mV และ R โดยประมาณตามวงจร/calibration |
| Weather | OpenWeather ตามพื้นที่ที่เลือก |
| PM2.5 | OpenWeather Air Pollution |
| RGB | สถานะควบคุม LED จริง; เปิด/หยุดด้วยปุ่มบนเว็บหรือ TFT |
| ข้อมูลบอร์ด | `/api/status`: เวอร์ชัน, IPv4 และ heap/stack |

หน้าเว็บอ่าน `/api/sensors` และ `/api/status` ประมาณทุก 2 วินาที
อ่าน `/api/weather` ทุก 15 วินาทีและเมื่อเปลี่ยนพื้นที่/แตะตรวจข้อมูล
WeatherService ยังจำกัดการเรียก OpenWeather ตามรอบเดิมและเก็บ cache แยกสามพื้นที่
ปุ่มตรวจข้อมูลดึง cache ล่าสุดของ ESP32 ไม่บังคับเรียก provider ถี่ขึ้น
การเลือกพื้นที่บนเว็บแยกจากพื้นที่บน TFT

เมื่อเซนเซอร์อ่านไม่ได้ แสดง `—` และข้อความไม่มีอุปกรณ์/อ่านมีปัญหา
เมื่อค่าที่อ่านช้าเกินรอบที่กำหนด แสดงว่าเป็นข้อมูลเก่าและส่งค่าปัจจุบันเป็น `null`
เมื่อ browser ติดต่อบอร์ดไม่ได้ ซ่อนค่าปัจจุบันของเซนเซอร์และระบุสถานะขาดการเชื่อมต่อ
ค่าอากาศเก่าจาก cache มีป้ายเตือนและเวลาอากาศ/PM2.5 แยกกัน
การอ่านกลับได้จะคืนค่าอัตโนมัติ; หน้าเว็บไม่สร้างเลขตัวอย่างแทนข้อมูลที่ใช้ไม่ได้
เมื่อแท็บถูกซ่อนจะหยุด polling และกลับมาอ่านเมื่อเปิดแท็บอีกครั้ง

## ซอร์สและ Build

- `web/index.html` รวม HTML, CSS และ JavaScript โดยใช้ฟอนต์จากระบบ
- `scripts/embed_web.py` บีบอัด gzip แบบ deterministic และสร้าง `include/web_dashboard.h`
  ก่อน Build โดยเขียนไฟล์ใหม่เฉพาะเมื่อ HTML เปลี่ยน
- หน้าเว็บส่งจาก PROGMEM ผ่าน `WebServer::send_P` ไม่คัดลอก HTML ทั้งหน้าเข้า String
- ไม่มี CDN หรือ framework ของหน้าเว็บ และไม่ใช้ LittleFS/SD
- `include/firmware_version.h` กำหนดเวอร์ชัน 1.2.0 ที่ Serial และ `/api/status`

```bash
"$HOME/.platformio/penv/bin/platformio" run -e esp32dev -j 1
python3 scripts/check_weather.py
python3 scripts/check_dht.py
python3 scripts/check_weather_service.py
```

Native tests ตรวจ API จริงที่คืนค่าจาก cache, root gzip, status,
null เมื่ออ่านผิด/ข้อมูลเก่า, ค่าศูนย์จริง, millis wrap และการควบคุม RGB
`scripts/check_web.py` เป็นการตรวจหน้าเว็บใน Chromium กับ API จำลอง
ต้องมี Python `playwright` และ browser จาก `python -m playwright install chromium` ก่อนรัน
API จำลองมีเฉพาะใน test; เฟิร์มแวร์ใช้อุปกรณ์และ OpenWeather จริง

ยังต้อง Upload เวอร์ชันนี้ก่อนเปิดเว็บจากบอร์ดจริง และตรวจค่าบนฮาร์ดแวร์หลัง Upload
