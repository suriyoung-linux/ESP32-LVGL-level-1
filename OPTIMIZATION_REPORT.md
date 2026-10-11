# ผล Optimization เฟิร์มแวร์ 1.1.0

เทียบกับ Build OpenWeather + DHT22 ก่อนเริ่ม optimization ในเซสชันนี้
ค่าของ Flash และ RAM แบบคงที่อ่านจาก Build environment `esp32dev`
ค่าบัฟเฟอร์ JSON ตรวจด้วย compiler ของ ESP32 จริง; ไม่รวมหน่วยความจำ TLS และ heap ของระบบ

| รายการ | ก่อน (bytes) | หลัง (bytes) | ลดลง |
|---|---:|---:|---:|
| Flash โปรแกรม | 1,035,325 | 1,032,577 | 2,748 (0.27%) |
| Static RAM | 50,052 | 50,044 | 8 |
| JSON จาก provider ต่อคำขอ | 4,096 | 1,024 | 3,072 (75%) |
| JSON `/api/weather` | 3,072 | 928 | 2,144 (69.8%) |
| JSON `/api/locations` | 2,048 | 1,024 | 1,024 (50%) |

Filter ของ provider ใช้ 336 bytes บน stack ของ weather task ไม่เพิ่ม heap allocation
stack ของ task นี้ยังตั้งไว้ 12,288 bytes เพื่อรองรับ HTTPS/TLS
`weather_stack_min_free_bytes` ใน `/api/status` วัดช่องว่างต่ำสุดของ stack หลัง worker ทำงาน
ก่อนมีผล worker ส่งค่านี้เป็น `null`; ไม่ตีความ 0 ว่าเป็นผลวัดแล้ว

การเปลี่ยนแปลง:

- Deserialize จาก HTTPS stream โดยกรองเฉพาะช่องที่ใช้จริง และจำกัดความลึก JSON
  เก็บ field rain ทั้งชุดเพื่อให้ตรวจ null/ผิดรูปแบบได้ ไม่ทำให้ค่าผิดกลายเป็นไม่มีฝน
- คำนวณความจุ JSON API จากจำนวนสมาชิกและพื้นที่ข้อความ พร้อมตรวจ overflow
  หากพื้นที่ไม่พอคืน error แทน JSON ที่มีสมาชิกขาดหาย; reserve บัฟเฟอร์ข้อความตามขนาดจริง
- ตรวจ schedule ของ weather ทุก 250 ms จากเดิมทุก loop และขอแต่ละพื้นที่หนึ่งครั้งต่อรอบ
  การอ่าน DHT ทุก 2 วินาที, LDR ทุก 500 ms และ touch ทุก 20 ms ยังใช้เวลาเดิม
- ใช้ revision ของ cache ติดตาม loading/ผลใหม่/ข้อผิดพลาด/การย้ายเครือข่าย
  วาดข้อมูล WEATHER เมื่อเปลี่ยน; นาฬิกาทุก 1 วินาทีวาดเฉพาะบริเวณ 266 × 18 pixels
  แทนการล้างพื้นที่ข้อมูล 320 × 140 pixels ขณะค่าคงที่
  นี่เป็นการลดพื้นที่วาดจากโค้ด ไม่ใช่ผล benchmark FPS หรือ CPU บนบอร์ด
- หน้า DHT อัปเดตเฉพาะอายุการอ่านเมื่อค่าคงที่; หน้า RGB ไม่วาดซ้ำเมื่อข้อมูลเท่าเดิม
  การอ่านล้มเหลวยังคงเปลี่ยนเป็น --.- และเตือนทันที
- ใช้ Font 2 ขยายขนาดสำหรับค่าหลักและตัวเลข LDR แทนการเก็บ Font 4 เพิ่ม
  ข้อความที่ยาวใช้ขนาดเล็กลงเพื่อให้พอดีจอ
- เปิด LTO เฉพาะไฟล์ใน `src` ผ่าน `build_src_flags`; framework และไลบรารี interrupt ใช้ flags เดิม
- กำหนดเฟิร์มแวร์ 1.1.0 ใน `include/firmware_version.h`, Serial และ `/api/status`
- เพิ่มค่า heap ปัจจุบัน, heap ต่ำสุด, block ว่างใหญ่สุด และ stack ต่ำสุดใน `/api/status`
- ตรวจช่วงอุณหภูมิ/ความชื้นตามรุ่น DHT ที่เลือก และปฏิเสธ NaN/Infinity

เวอร์ชันที่ตรวจและใช้:

| ส่วนประกอบ | เวอร์ชัน |
|---|---|
| PlatformIO Espressif32 | 7.1.3 |
| Arduino-ESP32 ที่ platform นี้เลือก | 2.0.17 |
| TFT_eSPI | 2.5.43 |
| ArduinoJson | 6.21.6 (อัปเดตจาก 6.21.5) |
| Adafruit DHT sensor library | 1.4.7 |
| Adafruit Unified Sensor | 1.1.15 (pin ให้ตรงกับชุดที่ Build) |
| XPT2046_Touchscreen | commit `f956c5d8ce3bf39169c7378416b89e7cfe70a034` |

ArduinoJson 6.21.6 แก้ buffer overrun ในการแปลงค่าตัวเลขจาก string ตาม
[release notes](https://arduinojson.org/v6/revisions/)
เลือก patch ล่าสุดของ v6 เพื่อคง API และขนาดที่เหมาะกับชุดนี้; v7 เป็น major version อีกชุดหนึ่ง
PlatformIO 7.1.3, TFT_eSPI 2.5.43 และ DHT 1.4.7 ตรงกับ release ล่าสุดที่ตรวจจาก upstream ในรอบนี้:
[Espressif32](https://github.com/platformio/platform-espressif32/releases),
[TFT_eSPI](https://github.com/Bodmer/TFT_eSPI/releases),
[DHT](https://github.com/adafruit/DHT-sensor-library/releases)
ช่วง DHT อ้างอิง [Adafruit DHT overview](https://learn.adafruit.com/dht/overview)

การตรวจที่ผ่าน:

```bash
python3 scripts/check_weather.py
python3 scripts/check_weather_service.py
python3 scripts/check_dht.py
"$HOME/.platformio/penv/bin/platformio" run -e esp32dev -j 1
```

ทดสอบ parsers/API handlers ด้วย fixture OpenWeather จริงที่บันทึกไว้,
กรอง stream, field ผิด/หาย, JSON เต็ม, ข้อความยาวสุด, ทิศ/หน่วยลม,
queue เต็ม/สร้างไม่สำเร็จ, cache เมื่อโหลดล้มเหลว, generation หลัง reconnect,
DHT11/DHT22 อ่านไม่ได้/ต่อกลับ, ปุ่ม/ปัด, การวาดนาฬิกาและการข้ามวาดข้อมูลเดิม
Native checks ใช้ AddressSanitizer และ UndefinedBehaviorSanitizer

ยังไม่ได้ Upload เฟิร์มแวร์เวอร์ชันนี้หรือวัด heap/stack/FPS บน ESP32 จริง
ต้องตรวจจอ touch ค่าจาก DHT22 และ HTTPS หลัง Upload เพื่อยืนยันการทำงานบนอุปกรณ์
API key อยู่ใน secrets.h ที่ถูก .gitignore และไม่ได้เปลี่ยนเป็นค่าจำลอง
