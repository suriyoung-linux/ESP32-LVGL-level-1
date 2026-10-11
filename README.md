# Level 1 / Lab 1 — Template Demo (โปรเจกต์แรกของบอร์ด ESP32 CYD)

> **เฟิร์มแวร์ 1.2.0** — เพิ่มหน้าเว็บดูข้อมูลจาก ESP32; ผลลดทรัพยากรเดิม: [OPTIMIZATION_REPORT.md](OPTIMIZATION_REPORT.md)
>
> **โปรแกรมปัจจุบัน:** หน้า WEATHER บน TFT แบบสัมผัส เลือกเชียงใหม่ กรุงเทพฯ และพื้นที่ WiFi/IP โดยประมาณ
> แยกชุดอากาศ ฝน/เมฆ ลม และ PM2.5 พร้อมเวลาท้องถิ่น และ JSON API
> หน้า LDR / RGB แยกต่างหาก มี START/STOP พร้อมตัวเลข V/R และรูป LED
> หน้า DHT อ่านอุณหภูมิและความชื้นจาก DHT22 ที่ GPIO27 ทุก 2 วินาที พร้อมแจ้งเตือนเมื่ออ่านไม่ได้
> แตะปุ่ม WEATHER / LDR / RGB / DHT หรือปัดซ้าย–ขวาเพื่อเปลี่ยนหน้า
> วิธีใช้งานฉบับปัจจุบัน: [WEATHER_API_GUIDE.md](WEATHER_API_GUIDE.md)
>
> **ส่วน LDR เดิม:** อ่าน LDR แสดงตัวเลขกับรูป LED บนจอ TFT และเลือกสี LED จริง
> ตามค่าดิบ ADC (> 1000 แดง, 500–1000 เขียว, < 500 น้ำเงิน)
> เชื่อมต่อ WiFi อัตโนมัติ พร้อมแสดงสถานะและ IPv4 บน TFT และ Serial
> ดูหัวข้อ "อ่านค่า LDR บนบอร์ดผ่าน Serial Monitor" และ "แสดงค่า LDR และสถานะ LED บนจอ TFT"
> เนื้อหา Lab 1 และใบงานด้านล่างอธิบายโปรแกรมกะพริบ LED ต้นฉบับ

Template โปรเจกต์ PlatformIO สำหรับบอร์ด **ESP32-2432S028 (CYD)** ที่รวมไฟล์ตั้งค่าสำคัญของทั้งคอร์สไว้ให้แล้ว
และมีโปรแกรมตัวอย่างง่ายๆ ที่พิสูจน์ว่าเครื่องมือและบอร์ดทำงานครบ:

1. พิมพ์ **Hello World** ออกทาง Serial Monitor
2. ให้ **RGB LED** บนบอร์ดกระพริบ แดง → ดับ → เขียว → ดับ → น้ำเงิน → ดับ วนไปเรื่อยๆ พร้อมพิมพ์ชื่อสีและเลข GPIO ใน Serial ให้ตรงกับสีที่เห็น

> **ใบงานฉบับละเอียด (อธิบายทฤษฎี อ่านโค้ดทีละบรรทัด แบบฝึกหัด และเฉลย) อยู่ที่ [WORKSHEET.md](WORKSHEET.md)**
> เฉลยสำหรับผู้สอน: [WORKSHEET_ANSWERS.md](WORKSHEET_ANSWERS.md)

## เป้าหมาย

- เปิดโปรเจกต์ด้วย VS Code + PlatformIO, build, upload และเปิด Serial Monitor ได้
- เข้าใจว่าไฟล์ตั้งค่าแต่ละไฟล์ทำหน้าที่อะไร และต้องแก้ไฟล์ไหน
- ใช้โฟลเดอร์นี้เป็นต้นแบบเริ่ม Lab ถัดไปได้ (คัดลอก → เปลี่ยนชื่อ → แก้ `src/main.cpp`)

## Repository บน GitHub

โปรเจกต์เก็บอยู่ที่ [suriyoung-linux/ESP32-LVGL-level-1](https://github.com/suriyoung-linux/ESP32-LVGL-level-1)
เป็น repository แบบ **Private** และใช้ branch **`main`**
โฟลเดอร์บน Ubuntu นี้เริ่มใช้ Git และเชื่อม `origin` แล้ว จึงใช้คำสั่งอัปเดตด้านล่างได้ทันที

### อัปโหลด README ที่แก้ไข

เปิด **Terminal → New Terminal** ใน VS Code ที่เชื่อมกับ Ubuntu แล้วรันทีละบรรทัด:

```bash
cd "$HOME/PlatformIO/Projects/Test ESP32-dev"
git status
git add README.md
git commit -m "docs: add GitHub upload instructions"
git push origin main
```

`git add` เลือกไฟล์ที่จะบันทึก, `git commit` บันทึกการเปลี่ยนแปลงในเครื่อง
และ `git push` ส่ง commit ขึ้น GitHub เมื่อสำเร็จให้รีเฟรชหน้า repository เพื่อดู README ล่าสุด

### อัปโหลดการเปลี่ยนแปลงทั้งโปรเจกต์ครั้งต่อไป

บันทึกไฟล์ที่แก้ไขใน VS Code ก่อน แล้วรัน:

```bash
cd "$HOME/PlatformIO/Projects/Test ESP32-dev"
git status
git diff
git add .
git diff --cached --stat
git commit -m "Update ESP32 weather dashboard"
git push origin main
```

เปลี่ยนข้อความใน `git commit -m` ให้ตรงกับงาน เช่น `Fix weather API parsing`
หาก `git status` แสดง `nothing to commit, working tree clean` แปลว่าไม่มีไฟล์ใหม่ให้บันทึก
แต่ถ้ามี commit ในเครื่องที่ยังไม่ได้ส่ง สามารถใช้ `git push origin main` ได้เลย

ไฟล์ `include/secrets.h`, `.pio/`, ไฟล์ Build และ Python cache ถูกยกเว้นด้วย `.gitignore`
ใน GitHub จะมี `include/secrets.h.example` เป็นแม่แบบ เมื่อใช้เครื่องใหม่ให้คัดลอกแม่แบบ
เป็น `include/secrets.h` แล้วกรอกชื่อ WiFi และรหัสผ่านในเครื่องนั้น

### ตรวจปลายทางและรับข้อมูลล่าสุด

```bash
git remote -v
git branch --show-current
git log -1 --oneline
```

ปลายทาง `origin` ต้องเป็น `https://github.com/suriyoung-linux/ESP32-LVGL-level-1.git`
และ branch ปัจจุบันต้องเป็น `main`
หากแก้ไฟล์จากเครื่องอื่นหรือจากหน้า GitHub ให้รับข้อมูลล่าสุดก่อนเริ่มแก้งาน
ขณะที่ไม่มีการเปลี่ยนแปลงค้างในเครื่อง:

```bash
git pull --ff-only origin main
```

หาก `push` แจ้ง `rejected (non-fast-forward)` หลังมี commit ใหม่ในเครื่อง
ให้รัน `git pull --rebase origin main` แล้ว `git push origin main` อีกครั้ง
หาก rebase พบ conflict ให้แก้ไฟล์ที่ Git แจ้งและทำ rebase ให้จบก่อน push

### อัปเดตผ่านเมนู VS Code

กด **Ctrl+Shift+G** เพื่อเปิด **Source Control** เลือก repository ของโปรเจกต์นี้
จากนั้นกด **+** เพื่อ Stage ไฟล์ ใส่ข้อความ commit แล้วกด **Commit**
ตามด้วย **… → Push** หากมีหน้าต่างเข้าสู่ระบบ GitHub ให้ใช้บัญชี `suriyoung-linux`

การอัปโหลดขึ้น GitHub ใช้เก็บซอร์สโค้ด ส่วนการส่งโปรแกรมลงบอร์ด ESP32
ยังใช้ **PlatformIO: Upload (Ubuntu)** ตามขั้นตอนด้านล่าง

## ดูผ่าน Web Browser

เฟิร์มแวร์ **1.2.0** มีหน้าเว็บที่บอร์ด ESP32 ให้บริการโดยตรง
เปิดดูจากมือถือหรือคอมพิวเตอร์ที่อยู่ในเครือข่าย WiFi/LAN เดียวกัน
หลังอัปโหลดแล้วเปิดเว็บได้โดยไม่ต้อง SSH เข้า Ubuntu หรือเปิด VS Code ค้างไว้

1. Build / Upload ด้วย PlatformIO แล้วรอ ESP32 เชื่อมต่อ WiFi
2. ดู IPv4 บนจอ TFT หรือ Serial ที่แสดง `Web dashboard ready: http://.../`
3. เปิดเบราว์เซอร์แล้วพิมพ์ `http://IP-ของ-ESP32/`
   เช่น `http://192.168.1.123/` (ตัวอย่างเท่านั้น ให้ใช้ IP ของบอร์ดจริง)

หน้าเว็บแสดง:

- อุณหภูมิและความชื้นจาก DHT22 ที่ GPIO27 พร้อมสถานะรอ/อ่านไม่ได้/ข้อมูลเก่า
- ค่าดิบ LDR, แรงดัน และความต้านทานโดยประมาณจากบอร์ด
- อากาศ ฝน เมฆ ลม และ PM2.5 จาก OpenWeather พร้อมเลือกเชียงใหม่ กรุงเทพฯ หรือพื้นที่ WiFi
- สถานะไฟ RGB และปุ่มเปิด/หยุดที่ใช้ร่วมกับปุ่มบน TFT
- เวอร์ชันเฟิร์มแวร์และข้อมูล heap/stack ที่วัดบน ESP32

ข้อมูลเซนเซอร์อ่านจาก cache ของบอร์ดประมาณทุก 2 วินาที ไม่สั่ง DHT อ่านถี่ขึ้น
ข้อมูลอากาศบนหน้าเว็บตรวจทุก 15 วินาที โดย provider ยังอัปเดตตามรอบ 10 นาทีเดิม
หากอ่านเซนเซอร์ไม่ได้หรือการเชื่อมต่อขาด หน้าเว็บแสดงสถานะและซ่อนค่าปัจจุบันที่ใช้ไม่ได้
หน้าเว็บหยุด polling ขณะแท็บถูกซ่อน

ซอร์สหน้าเว็บอยู่ใน `web/index.html`; `scripts/embed_web.py` จะบีบอัดและสร้าง
`include/web_dashboard.h` อัตโนมัติเมื่อ Build หน้าเว็บเก็บใน Flash
ไม่ต้องติดตั้งไลบรารีเว็บเพิ่มหรือเตรียม LittleFS/SD และไม่ส่ง API key ไปที่เบราว์เซอร์
หากเปิดไม่ได้ ให้ตรวจ IP ล่าสุด เครือข่ายเดียวกัน และการแยกอุปกรณ์ของ Guest WiFi
รายละเอียดเพิ่มเติม: [WEB_BROWSER_GUIDE.md](WEB_BROWSER_GUIDE.md)

## เวอร์ชันและผล Optimization

ตั้งแต่เฟิร์มแวร์ **1.1.0** แสดงเวอร์ชันบน Serial และ `GET /api/status`
ลดบัฟเฟอร์ JSON, วาดจอเฉพาะส่วนที่เปลี่ยน, ใช้ฟอนต์ที่มีอยู่ขยายขนาด
และเปิด LTO สำหรับซอร์สโปรแกรม พร้อมอัปเดต ArduinoJson เป็น 6.21.6
ผล Build ก่อน/หลังและการตรวจอยู่ใน [OPTIMIZATION_REPORT.md](OPTIMIZATION_REPORT.md)

`/api/status` มี `firmware_version`, `heap_free_bytes`, `heap_min_free_bytes`,
`heap_largest_block_bytes` และ `weather_stack_min_free_bytes`
ค่า heap/stack มาจาก ESP32 จริงเมื่อเรียก API; stack เป็น `null` ก่อน worker มีผลตรวจ
ต้อง Build / Upload เพื่อให้บอร์ดใช้เวอร์ชันนี้

## ตั้งค่า OpenWeather API key

อุณหภูมิ ความชื้น ฝน เมฆ และลมบนหน้า WEATHER มาจาก **OpenWeather Current Weather**
ส่วน PM2.5 มาจาก **OpenWeather Air Pollution** ใช้ key เดียวกันสำหรับทั้งสามพื้นที่

ใส่ key ใน `include/secrets.h` เพิ่มจากชื่อและรหัสผ่าน WiFi:

```cpp
#define OPENWEATHER_API_KEY "ใส่_API_KEY_ของคุณที่นี่"
```

หากยังไม่มีไฟล์นี้ ให้คัดลอก `include/secrets.h.example` เป็น `include/secrets.h` ก่อน
ไฟล์ `secrets.h` อยู่ใน `.gitignore` จึงไม่อัปโหลด key และรหัส WiFi ขึ้น GitHub
หลังบันทึกต้อง Build / Upload ใหม่ แล้วรอ WiFi และ NTP พร้อม
จะแสดงข้อมูลจาก API และอัปเดตอัตโนมัติทุก 10 นาที หรือแตะ REFRESH

ฝนแสดงอัตรา `mm/h`; ลมแปลงจาก `m/s` เป็น `km/h`
ถ้า API ไม่คืนทิศลมจะแสดงว่าไม่มีข้อมูล ไม่สมมติว่าลมมาจากทิศเหนือ
HTTP 401 แสดงว่า key ไม่ถูกต้องหรือยังไม่พร้อมใช้งาน; HTTP 429 แสดงว่าเกินข้อจำกัด API
ข้อมูลและวิธีใช้งานเพิ่มเติมอยู่ใน [WEATHER_API_GUIDE.md](WEATHER_API_GUIDE.md)

## อ่านอุณหภูมิและความชื้นจริงจาก DHT11 / DHT22

หน้า **DHT** อ่านจากเซนเซอร์ที่ต่อกับบอร์ดโดยตรง และทำงานได้โดยไม่ต้องเชื่อม WiFi
หน้า **WEATHER** อ่านจาก OpenWeather ตามพิกัดที่เลือก โดยใช้ API key ที่ตั้งไว้

| ขาเซนเซอร์ | ต่อกับบอร์ด CYD |
|---|---|
| DATA / OUT / S | GPIO27 |
| VCC / + | 3.3 V สำหรับโมดูลที่รองรับแรงดันนี้ |
| GND / − | GND |

ตรวจชื่อขาที่พิมพ์บนโมดูล เพราะลำดับขาของโมดูลสามขาอาจต่างกัน
หากเป็นเซนเซอร์เปล่าสี่ขา ให้ต่อ resistor pull-up 10 kΩ ระหว่าง DATA กับ 3.3 V
และไม่ต่อขา NC ดู [คู่มือการต่อ DHT ของ Adafruit](https://learn.adafruit.com/dht/connecting-to-a-dhtxx-sensor)
สัญญาณ DATA ที่เข้า ESP32 ต้องเป็นระดับ 3.3 V; หากโมดูลต้องใช้ไฟ 5 V ต้องจัดการระดับสัญญาณให้เหมาะสม

ตั้งค่าเริ่มต้นเป็น **DHT22** ใน `include/app_config.h`:

```cpp
#define CYD_DHT_MODEL 22
```

หากเปลี่ยนอุปกรณ์เป็น DHT11 ให้เปลี่ยน `22` เป็น `11` แล้ว Build / Upload ใหม่
โค้ดอ่านทุก 2 วินาที ใช้ผลจากการอ่านครั้งเดียวกันสำหรับอุณหภูมิ °C และความชื้นสัมพัทธ์ %
หลังเปิดเครื่องรออย่างน้อย 2 วินาทีก่อนอ่านครั้งแรก

- แตะ **DHT** ที่แถวบนเพื่อเปิดหน้าเซนเซอร์
- ปัดซ้ายไปหน้าถัดไป ปัดขวากลับหน้าก่อนหน้า ตามลำดับ **WEATHER → LDR / RGB → DHT → WEATHER**
- จะแยกการแตะกับการปัดเมื่อปล่อยนิ้ว จึงไม่สั่ง START/STOP หรือ REFRESH ขณะปัด
- **Sensor OK** หมายถึงอ่านค่าได้; **Waiting for first sensor reading...** หมายถึงกำลังรอรอบแรก
- **No sensor / sensor error** หมายถึงไม่ได้ต่อเซนเซอร์หรืออ่านไม่ได้ ให้ตรวจสายและชนิด DHT
- เมื่ออ่านล้มเหลว อุณหภูมิและความชื้นแสดง `--.-` แล้วลองอ่านใหม่ทุก 2 วินาที
  เมื่อต่ออุปกรณ์กลับและอ่านสำเร็จ ค่าจริงจะกลับมาแสดงเอง

ผลการอ่านและคำเตือนปรากฏใน Serial Monitor ด้วย เช่น
`DHT22 GPIO27: temperature=26.5 C, humidity=61.0 %`
ค่าในตัวอย่างนี้ใช้แสดงรูปแบบข้อความ; บนบอร์ดจะเป็นค่าที่อ่านได้จริง
การอ่านล้มเหลวไม่สามารถยืนยันแยกได้ว่าไม่ได้ต่ออุปกรณ์ สายผิด หรือเซนเซอร์เสีย

ทดสอบตรรกะการอ่านและปัดหน้าจอบนเครื่องพัฒนาได้ด้วย:

```bash
python3 scripts/check_dht.py
python3 scripts/check_weather.py
python3 scripts/check_weather_service.py
```

## โครงสร้างโปรเจกต์

```
Test ESP32-dev/
├─ platformio.ini        ค่าตั้งของ PlatformIO: บอร์ด, baud, flag ของจอ TFT/LVGL, ไลบรารี TFT_eSPI
├─ include/
│  ├─ board_config.h     ทุก pin + ข้อมูลจอ/touch + ฟังก์ชันช่วย LED  (ฮาร์ดแวร์ ห้ามแก้ถ้าไม่จำเป็น)
│  ├─ app_config.h       ค่าที่ปรับได้ของ Lab นี้ เช่น ความเร็ว LED, ข้อความ  (แก้ได้เลย)
│  ├─ ldr_measurement.h  สูตรประมาณความต้านทาน LDR จากวงจรแบ่งแรงดัน
│  ├─ dht_sensor.h       ข้อมูลและสถานะการอ่านเซนเซอร์ DHT ที่ GPIO27
│  ├─ touch_gesture.h    แยกการแตะกับการปัดซ้าย/ขวา
│  ├─ lv_conf.h          ค่าตั้งของ LVGL (Lab นี้ยังไม่ใช้ แต่เตรียมไว้ให้ Lab ที่มีจอ)
│  └─ secrets.h.example  แม่แบบชื่อ WiFi/รหัสผ่าน; ค่าจริงใส่ secrets.h ซึ่งถูก gitignore
├─ src/
│  ├─ main.cpp           อ่าน LDR/DHT, ควบคุม RGB LED, เชื่อม WiFi และแสดงผลบน TFT/Serial
│  ├─ dashboard.cpp      หน้า WEATHER / LDR-RGB / DHT และปุ่ม/การปัดหน้าจอ
│  ├─ dht_sensor.cpp     อ่าน DHT จริง ตรวจข้อมูลและลองใหม่เมื่ออ่านไม่ได้
│  └─ weather_service.cpp  ดึงข้อมูลสภาพอากาศและ PM2.5 จาก API
├─ .vscode/extensions.json  แนะนำ extension PlatformIO ให้ VS Code
├─ .vscode/tasks.json       คำสั่ง Build / Upload / Monitor สำหรับ Ubuntu
└─ .gitignore            ไม่เก็บ .pio, ไฟล์ build และ secrets.h
```

| ไฟล์ | แก้เมื่อไร |
|---|---|
| `board_config.h` | แทบไม่แก้ (ยกเว้นค่า calibration ของ touch ที่ Lab touch, หรือ `LED_ACTIVE_LOW` ถ้า LED กลับด้าน) |
| `app_config.h` | แก้บ่อย ใช้ลองเปลี่ยนค่าต่างๆ ของแต่ละ Lab |
| `platformio.ini` | เปิด `lib_deps` เมื่อเริ่มใช้จอ/touch/LVGL |
| `lv_conf.h` | ปรับเมื่อต้องเปิด widget/ฟอนต์เพิ่ม |

## วิธีเปิดโปรเจกต์บน Ubuntu ผ่าน Remote SSH

เปิด Home ในแท็บ VS Code: กด **Ctrl+Shift+P → Developer: Reload Window**
หลังติดตั้งการแก้ครั้งแรก แล้วค้นหา **PlatformIO Home** ใน Command Palette
และเลือกคำสั่งของ PlatformIO หรือกด **PIO Home → Open** ในแถบ PlatformIO

โปรเจกต์ตั้งให้ Home เลือกพอร์ตว่างอัตโนมัติใน `.vscode/settings.json`
และส่วนขยาย PlatformIO IDE 3.3.4 บน Ubuntu นี้ได้รับการแก้ให้ URL ของแท็บ
ผ่าน `vscode.env.asExternalUri` เพื่อใช้ SSH tunnel ของ VS Code
การแก้นี้อาจถูกแทนที่เมื่ออัปเดตส่วนขยาย; ใช้
`python3 scripts/fix_pio_home_ssh.py` เพื่อใช้การแก้ซ้ำสำหรับเวอร์ชัน 3.3.4
หรือเพิ่ม `--restore` เพื่อคืนไฟล์ส่วนขยายเดิม แล้ว Reload Window
สคริปต์จะหยุดถ้าโครงสร้างไฟล์ส่วนขยายไม่ตรงกับเวอร์ชันที่รองรับ

เปิด Home ในเบราว์เซอร์บน Windows ด้วย URL เซสชันปัจจุบันได้จาก
**Terminal → Run Task... → PlatformIO: Open Home in Browser (SSH)**
โดยเปิด PlatformIO Home ในหน้าต่าง SSH ก่อน; สคริปต์อ่านพอร์ตและเซสชันล่าสุด
งานนี้หารหัสเซสชันและเปิดเบราว์เซอร์ให้เองทุกครั้ง ไม่ต้องจำหรือพิมพ์รหัสยาว

1. ในหน้าต่าง VS Code ที่เชื่อมต่อ SSH แล้ว เลือก **File → Open Folder** และเปิด
   `/home/ubuntu/PlatformIO/Projects/Test ESP32-dev` ซึ่งมี `platformio.ini` อยู่โดยตรง
2. รอ PlatformIO โหลดโปรเจกต์ จากนั้นกด **✓ Build** หรือ **Ctrl + Alt + B**
   อีกทางหนึ่งคือ **Ctrl + Shift + B** ซึ่งใช้ Build task ที่เตรียมไว้
3. การ Build ไม่ต้องต่อบอร์ด แต่การ Upload / Monitor ต้องให้เครื่อง Ubuntu ปลายทาง
   มองเห็น USB ของบอร์ด การเสียบ USB ที่เครื่อง Windows ต้นทางอย่างเดียวไม่ทำให้
   Remote SSH เห็นพอร์ตนั้น ต้องต่อกับ Ubuntu หรือส่งต่อ USB ให้ Ubuntu ก่อน

หากเปิดผ่าน Terminal ให้ใช้คำสั่ง ไม่ใช่พิมพ์พาธเปล่าๆ:

```bash
cd "$HOME/PlatformIO/Projects/Test ESP32-dev"
code -r .
```

## Build / Upload / Serial Monitor

คำสั่งต่อไปนี้ใช้ PlatformIO จาก virtual environment ที่ติดตั้งอยู่บน Ubuntu
จึงใช้ได้แม้ Terminal ปกติไม่รู้จัก `pio` หรือ `python`:

```bash
cd "$HOME/PlatformIO/Projects/Test ESP32-dev"
"$HOME/.platformio/penv/bin/platformio" run
"$HOME/.platformio/penv/bin/platformio" device list
```

เลือกพอร์ตที่แสดงจริงจาก `device list` เช่น `/dev/ttyUSB0` หรือ `/dev/ttyACM0`
ตัวอย่างด้านล่างใช้ `/dev/ttyUSB0` ซึ่งต้องเปลี่ยนให้ตรงกับบอร์ดของคุณ:

```bash
"$HOME/.platformio/penv/bin/platformio" run -t upload --upload-port /dev/ttyUSB0
"$HOME/.platformio/penv/bin/platformio" device monitor -b 115200 --port /dev/ttyUSB0
```

- **Ctrl + Shift + P → Tasks: Run Task** มีคำสั่ง Build, Upload, Serial Monitor และ Device List
  โดย Upload / Monitor task ให้ PlatformIO เลือกพอร์ตอัตโนมัติ หากมีหลายบอร์ดให้ใช้คำสั่งที่ระบุพอร์ตด้านบน
- **ปิด Serial Monitor (`Ctrl+C`) ก่อน Upload** เพื่อคืนพอร์ตให้โปรแกรมอัปโหลด
- อย่าใช้ `COM3` ใน Terminal ของ Ubuntu เพราะเป็นชื่อพอร์ตของ Windows
- ถ้าขึ้น `Permission denied` ให้ตรวจสิทธิ์ของพอร์ตและกลุ่มผู้ใช้บน Ubuntu
  ถ้าไม่เห็นพอร์ต USB เลย ให้ตรวจการต่อ USB กับเครื่องปลายทางก่อน

โปรเจกต์นี้มี `scripts/serial_permissions.py` ซึ่งทำงานหลัง PlatformIO ตั้งค่าตัวอัปโหลด
หากบัญชี Ubuntu อยู่ในกลุ่ม `dialout` แล้ว แต่ VS Code ยังใช้เซสชันเดิม สคริปต์จะเรียก
ตัวอัปโหลดผ่าน `sg dialout` เพื่อใช้สิทธิ์ที่เพิ่มไว้ทันที ปุ่ม Upload ของ PlatformIO
และ Upload task จึงใช้ได้โดยไม่ต้องรีสตาร์ต VM ส่วน task **PlatformIO: Serial Monitor (Ubuntu)**
ใช้ `sg dialout` เช่นกัน (เลือกจาก **Tasks: Run Task**)

หากบัญชียังไม่ได้อยู่ในกลุ่มนี้ ให้รัน `sudo usermod -aG dialout ubuntu` ใน Terminal ก่อน
สคริปต์ใช้ได้เฉพาะสิทธิ์ที่บัญชีได้รับแล้ว หากมี `[Errno 5] Input/output error` แม้พอร์ตยังปรากฏ
ให้ถอดเสียบ USB ของบอร์ดใหม่ และใน VMware เชื่อม QinHeng/CH340 USB Serial กลับเข้า Ubuntu VM
จากนั้นตรวจ Device List อีกครั้ง พอร์ตอาจเปลี่ยนชื่อเมื่อเชื่อมต่อใหม่

ข้อความ `the port is busy or doesn't exist` ของ esptool เป็นข้อความทั่วไป ให้ดูข้อผิดพลาดในวงเล็บด้วย
ถ้าเป็น `Input/output error` ให้ตรวจ USB/ไดรเวอร์ ถ้าเป็น `Permission denied` ให้ตรวจสิทธิ์พอร์ต
การลด `upload_speed` ไม่แก้กรณีที่ระบบเปิดพอร์ตไม่ได้; ใช้เมื่อเปิดพอร์ตและเชื่อม bootloader ได้แล้วแต่ส่งข้อมูลผิดพลาด
ตรวจผู้ใช้พอร์ตและข้อความ USB ล่าสุดได้ด้วย:

```bash
fuser -v /dev/ttyUSB0
journalctl -k --since '-5 min' --no-pager
```

การตรวจครั้งนี้พบ `ch341-uart ttyUSB0: failed to read modem status: -110` ซึ่งเป็น USB timeout
และอัปโหลดผ่านที่ความเร็วเดิม 921600 baud หลังเชื่อม USB ใหม่ ดูผลแต่ละขั้นใน [VERIFICATION_REPORT.md](VERIFICATION_REPORT.md)

หากมีคำเตือน `Please install 99-platformio-udev.rules` ด้วย ให้ติดตั้งไฟล์ rules ของ PlatformIO
คำเตือนนี้แยกจาก USB timeout: ในระบบนี้บัญชี `ubuntu` มีสิทธิ์ `dialout` ถูกต้องแล้ว
ไฟล์ rules จาก PlatformIO ที่ติดตั้งอยู่ถูกคัดลอกไว้ที่ `.pio/verification/99-platformio-udev.rules`
รันจาก Terminal ในโฟลเดอร์โปรเจกต์ (ต้องยืนยัน `sudo` บนเครื่อง):

```bash
sudo install -m 644 .pio/verification/99-platformio-udev.rules /etc/udev/rules.d/99-platformio-udev.rules
sudo udevadm control --reload-rules
```

สำหรับพอร์ตที่มี USB timeout สามารถลองรีเซ็ตเฉพาะ CH340 ที่ตรวจพบได้ด้วย
`sudo usbreset 1a86:7523` จากนั้นตรวจพอร์ตอีกครั้ง หากรีเซ็ตไม่สำเร็จ ให้ถอดเสียบ USB
และใน VMware เลือก Removable Devices → QinHeng/USB Serial เพื่อเชื่อมกลับเข้า Ubuntu
การติดตั้ง rules และรีเซ็ต USB ยังไม่ได้ดำเนินการจากเซสชันนี้ เพราะ `sudo` ต้องยืนยันด้วยรหัสผ่านบนเครื่อง

## ผลที่ควรเห็นจากโปรแกรมกะพริบ LED ต้นฉบับ

Serial Monitor (ตัวอย่างรูปแบบข้อความที่ตรวจพบจากบอร์ด; ชิป/หน่วยความจำอาจต่างกัน)

```
==========================================
Hello World from ESP32-2432S028 (CYD)!
Level 1 / Lab 1 : Template Demo
Mode : RGB LED blink (ON 2000 ms / OFF 200 ms)
Chip : ESP32-D0WD-V3 rev 3, CPU 240 MHz
Flash: 4096 KB, free heap: 350728 bytes
==========================================
Hello World #1  LED = RED (GPIO4)
LED = OFF
Hello World #2  LED = GREEN (GPIO16)
LED = OFF
Hello World #3  LED = BLUE (GPIO17)
LED = OFF
Hello World #4  LED = RED (GPIO4)
...
```

RGB LED ติดแต่ละสีเป็นเวลา `LED_STEP_MS` (2000 ms หรือ 2 วินาที) และดับเป็นเวลา `LED_OFF_MS` (200 ms หรือ 0.2 วินาที) ก่อนเปลี่ยนสี ค่าเหล่านี้แก้ได้ใน `include/app_config.h` **สีที่เห็นต้องตรงกับชื่อใน Serial เสมอ**

ระยะห่างระหว่างบรรทัดเริ่มสีสองบรรทัด โดยข้าม `LED = OFF` จึงประมาณ **2.2 วินาที**
เท่ากับ `(LED_STEP_MS + LED_OFF_MS) / 1000` ไม่ใช่เวลาติดเพียงอย่างเดียว

หลัง Upload ให้เปิด Serial Monitor ที่ 115200 baud แล้วกด RESET ควรเห็น `Mode : RGB LED blink (ON 2000 ms / OFF 200 ms)` เพื่อยืนยันว่าบอร์ดกำลังรันโปรแกรมเวอร์ชันนี้ หากไม่เห็นข้อความนี้ ให้ตรวจผล Upload และพอร์ตที่เลือกก่อน

| ลำดับ | ข้อความใน Serial | สีที่ต้องเห็นบนบอร์ด |
|---|---|---|
| 1 | `LED = RED` | แดง |
| 2 | `LED = GREEN` | เขียว |
| 3 | `LED = BLUE` | น้ำเงิน |

ถ้าสีที่เห็นไม่ตรงกับชื่อ ดูหัวข้อ "ปัญหาที่พบบ่อย" ด้านล่าง

โปรแกรมกะพริบ LED ต้นฉบับไม่ได้ใช้จอ TFT ส่วนโปรแกรมปัจจุบันใช้จอแสดงค่า LDR และรูป LED ตามหัวข้อด้านล่าง

## อ่านค่า LDR บนบอร์ดผ่าน Serial Monitor

โปรแกรมปัจจุบันเพิ่มการอ่าน LDR ที่ `cyd::LDR_PIN` (GPIO34) ด้วย `analogRead()`
โดยตั้งความละเอียด ADC เป็น 12 บิต (0–4095) อ่านค่าครั้งแรกทันที แล้วอ่านซ้ำทุก 500 ms
ใช้ `millis()` จับเวลา แต่ละครั้งที่อ่านจะเลือกสี LED และพิมพ์ค่ากับชื่อสีในบรรทัดเดียวกัน
LED ค้างตามค่าที่อ่านล่าสุดจนถึงการอ่านครั้งถัดไป

| ค่าดิบ LDR | สี LED |
|---|---|
| มากกว่า 1000 | แดง |
| 500 ถึง 1000 รวมค่าเท่ากับ 500 และ 1000 | เขียว |
| น้อยกว่า 500 | น้ำเงิน |

ปรับช่วงเวลาอ่านได้ที่ `LDR_READ_INTERVAL_MS` และปรับเกณฑ์ที่ `LDR_LOW_THRESHOLD`
กับ `LDR_HIGH_THRESHOLD` ใน `include/app_config.h` โดยค่าต่ำต้องน้อยกว่าค่าสูง

หลัง Upload เปิด Serial Monitor ที่ 115200 baud แล้วลองบัง LDR และเพิ่มแสงที่ตกกระทบ
สังเกตค่าที่เปลี่ยนไปเพื่อหาความสัมพันธ์ระหว่างแสงกับค่าที่อ่านได้บนบอร์ดของคุณ
ตัวอย่างรูปแบบข้อความ (ตัวเลขสมมติ ไม่ใช่ผลวัดจริง):

```text
LDR (GPIO34) = 1200 (raw ADC)  V = 0.900 V  R~ 600.0 kOhm  LED = RED (GPIO4)
LDR (GPIO34) = 750 (raw ADC)  V = 0.600 V  R~ 285.7 kOhm  LED = GREEN (GPIO16)
LDR (GPIO34) = 300 (raw ADC)  V = 0.300 V  R~ 111.1 kOhm  LED = BLUE (GPIO17)
```

ค่าดิบ ADC ไม่ใช่ค่าความสว่างหน่วย lux ดูรายละเอียด API ได้ที่
[Espressif: ADC](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/adc.html)
ผล Upload และ Serial ในรายงานเดิมเป็นของโปรแกรมก่อนเพิ่ม LDR
ผล Build ของโปรแกรม LDR + TFT อยู่ในหัวข้อท้าย [VERIFICATION_REPORT.md](VERIFICATION_REPORT.md)

## แสดงค่า LDR และสถานะ LED บนจอ TFT

โปรแกรมใช้ `TFT_eSPI@2.5.43` กับจอ ILI9341 ตามค่าที่เตรียมไว้ใน `platformio.ini`
เริ่มจอแนวนอน 320×240 และเปิด backlight ผ่าน `cyd::enableBacklight()`
หน้าจอแสดง:

- ตัวเลข LDR ขนาดใหญ่ ช่วง 0–4095 และแถบสัดส่วนค่าดิบ ADC
- สถานะ WiFi และ IPv4 ด้านบนของหน้าจอ
- แรงดันที่ GPIO34 เทียบกับ GND ในหน่วย V และความต้านทาน LDR โดยประมาณ (`R~`)
- รูปหลอด LED แดง เขียว น้ำเงิน: สีที่กำลังติดจะสว่าง มีกรอบสีและข้อความ `ON`
  อีกสองหลอดเป็นสีเทาและแสดง `OFF`
- สีของตัวเลขและแถบตรงกับ LED ที่กำลังติด

ค่าที่อ่านหนึ่งครั้งใช้ควบคุม LED จริง วาดจอ และพิมพ์ Serial จึงใช้เงื่อนไขชุดเดียวกัน
อ่านทุก 500 ms โดยวาดตัวเลขเฉพาะเมื่อค่าเปลี่ยน และวาดรูป LED ใหม่เฉพาะเมื่อสีเปลี่ยน
ไม่ได้ล้างทั้งหน้าจอทุกครั้ง จึงลดการกะพริบโดยไม่ต้องจองหน่วยความจำสำหรับ framebuffer
ใช้ข้อความอังกฤษกับฟอนต์ที่มากับไลบรารี

วิธีทดลอง: ปิด Monitor เดิมด้วย `Ctrl+C` แล้วเลือก
**Terminal → Run Task… → PlatformIO: Upload and Monitor (esp32dev)**
เมื่ออัปโหลดผ่าน ให้ลองบัง LDR และเพิ่มแสง ตรวจตัวเลขและรูป LED เทียบกับ Serial และ LED จริง
สามารถใช้งานจอด้วยการจ่ายไฟให้บอร์ดหลังอัปโหลดสำเร็จ โดยไม่ต้องเปิด Serial Monitor
ภาพบนจอแสดงสถานะที่โปรแกรมสั่ง GPIO; ต้องตรวจสี LED จริงบนบอร์ดประกอบด้วย

### ค่าแรงดัน V และค่าประมาณความต้านทาน R

ใช้ `esp_adc_cal_characterize()` ตั้งข้อมูล calibration แล้วแปลงค่าดิบที่อ่านครั้งเดียวกัน
ด้วย `esp_adc_cal_raw_to_voltage()` เป็น mV ก่อนหาร 1000 เพื่อแสดง V
ใช้ข้อมูล eFuse ของชิปหากมี มิฉะนั้นใช้ Vref ภายใน ADC ค่าเริ่มต้น 1100 mV
Serial แสดงชนิด calibration ตอนเริ่มโปรแกรม
อ้างอิง [Espressif: ADC calibration](https://docs.espressif.com/projects/esp-idf/en/v4.4.7/esp32/api-reference/peripherals/adc.html#adc-calibration)

อ้างอิง [แผนผังวงจรบอร์ดต้นฉบับ](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display/blob/main/OriginalDocumentation/5-Schematic/ESP32-2432S028-MCU.jpg):
R15 = 1 MOhm ต่อจาก 3.3 V ไปยัง GPIO34; R19 = 1 MOhm ต่อขนานกับ LDR ลง GND
แรงดัน V ที่อ่านจึงเป็นแรงดันคร่อม LDR ด้วย สูตรที่ใช้คือ:

```text
I_LDR = (Vs - V) / R15 - V / R19
R_LDR = V / I_LDR
```

เมื่อ R15 = R19 = 1 MOhm และ Vs = 3.3 V จะได้ `R_LDR = 1000000 * V / (3.3 - 2 * V)`
เช่น V = 1.1 V ได้ R ประมาณ 1 MOhm ต้องคิดผลของ R19 ด้วย จึงไม่ใช้สูตรแบ่งแรงดันที่มีตัวต้านทานสองตัวอย่างเดียว

แสดง `R~` เพราะเป็นค่าคำนวณจากค่าตัวต้านทานและแรงดันจ่ายตามสมมติฐาน
ปรับ `LDR_SUPPLY_VOLTAGE` ใน `app_config.h` ตามแรงดันจ่ายที่วัดจริง และปรับค่า R15/R19
ใน `board_config.h` หากบอร์ดที่ใช้ต่างจากวงจรต้นฉบับ ตัวต้านทานในแผนผังมี tolerance ±5%
ค่า ADC และการโหลดวงจรยังมีผลต่อความแม่นยำ จึงควรเทียบกับมัลติมิเตอร์ก่อนใช้เป็นเครื่องมือวัด
`kOhm` = 1,000 Ohm; `MOhm` = 1,000,000 Ohm

แสดง `R: N/A` เมื่อ V ต่ำกว่า 0.15 V (ช่วงล่างของ ADC ที่ attenuation นี้)
หรือกระแส LDR ที่คำนวณได้ไม่เป็นบวก สำหรับวงจรค่าเริ่มต้น กรณีหลังเกิดเมื่อ V ≥ 1.65 V
จึงไม่แสดงค่าความต้านทานติดลบหรือหารด้วยศูนย์ เงื่อนไขสี LED ยังใช้ค่าดิบ ADC ตามเดิม

## เชื่อมต่อ WiFi และแสดง IPv4

1. เปิด `include/secrets.h` แล้วใส่ชื่อ WiFi และรหัสผ่านในเครื่องหมายคำพูด:

   ```cpp
   inline constexpr char DEFAULT_WIFI_SSID[] = "ชื่อ WiFi ของคุณ";
   inline constexpr char DEFAULT_WIFI_PASSWORD[] = "รหัสผ่าน WiFi ของคุณ";
   ```

   ใช้เครือข่าย **2.4 GHz** สำหรับ ESP32 บน CYD รุ่นนี้
   หากไม่มีไฟล์นี้ ให้คัดลอก `include/secrets.h.example` เป็น `include/secrets.h`
   เครือข่ายเปิดที่ไม่มีรหัสผ่านให้ใช้ `DEFAULT_WIFI_PASSWORD[] = ""`
2. บันทึกไฟล์ ปิด Serial Monitor เดิมด้วย `Ctrl+C` แล้วเลือก
   **Terminal → Run Task… → PlatformIO: Upload and Monitor (esp32dev)**
3. หลัง Upload บอร์ดเริ่มเชื่อมต่อ WiFi อัตโนมัติ เมื่อสำเร็จ จอ TFT และ Serial แสดง เช่น:

   ```text
   WiFi: CONNECTED  IPv4: 192.168.1.123
   ```

   IP นี้เป็นตัวอย่าง บอร์ดรับ IPv4 จริงจากเราเตอร์ด้วย DHCP และค่าอาจเปลี่ยนเมื่อเชื่อมต่อใหม่

เมื่อยังไม่ได้ใส่ชื่อ WiFi จอขึ้น `WiFi: SET SSID` และ `IPv4: --`
เมื่อเปิดเครื่องจะสแกนเครือข่ายแบบไม่หยุดรอหนึ่งครั้ง โดยแสดง `SCANNING`
Serial บอกว่าพบเครือข่ายชื่อตรงกับค่าที่ตั้งไว้หรือไม่ พร้อม RSSI, channel และ security
แสดงเฉพาะรายละเอียดของเครือข่ายเป้าหมาย ไม่พิมพ์รหัสผ่าน
ขณะเชื่อมต่อแสดง `CONNECTING`; เมื่อไม่พบเครือข่ายแสดง `SSID NOT FOUND`
หรือ `CONNECT FAILED` เมื่อเชื่อมต่อไม่สำเร็จ ให้ตรวจชื่อเครือข่าย รหัสผ่าน และย่าน 2.4 GHz
Serial แสดงรหัสเหตุผลที่ไดรเวอร์รายงาน เช่น `201 (NO_AP_FOUND)`
ซึ่งยังอาจเกิดจากสัญญาณอ่อนหรือเครือข่ายไม่ผ่านเงื่อนไขความปลอดภัย จึงไม่ยืนยันว่าเป็นรหัสผ่านผิด
หากสแกนรายงาน `failed` จะยังลองเชื่อมต่อ แต่ไม่มีผลสแกนที่ใช้ยืนยันเครือข่ายที่มองเห็นได้
หากสแกนสำเร็จและเห็นเครือข่ายอื่นแต่ `0 match configured SSID` ให้ตรวจชื่อที่เราเตอร์
ปล่อยบนย่าน 2.4 GHz โดยตรง โทรศัพท์อาจเชื่อมเครือข่ายชื่อที่ตั้งไว้ผ่าน 5 GHz
ลองแยกชื่อย่าน 2.4 GHz ให้ชัดเจน หรือทดสอบ Hotspot 2.4 GHz ใกล้บอร์ด
แล้วแก้ชื่อ/รหัสผ่านใน `secrets.h` และ Upload ใหม่เพื่อแยกปัญหาของเราเตอร์กับบอร์ด
เมื่อการเชื่อมต่อหลุด โปรแกรมลบ IPv4 เดิมและลองเชื่อมต่อใหม่ทุก 15 วินาที
โดยยังอ่าน LDR, แสดง V/R และควบคุม LED ทุก 500 ms

ใช้ `WiFi.h` ที่มากับ Arduino ESP32 ไม่ต้องเพิ่มไลบรารีใน `platformio.ini`
ตั้ง Station mode (`WIFI_STA`) และตรวจสถานะใน `loop()` ทุก 250 ms
ไม่มีวงวนรอ WiFi; ไม่วาด TFT จาก event callback ที่ทำงานคนละ task
GPIO34 เป็น ADC1 จึงใช้ LDR ร่วมกับ WiFi ได้
ปรับช่วงตรวจสถานะ/ลองใหม่ได้ที่ `WIFI_CHECK_INTERVAL_MS` / `WIFI_RETRY_INTERVAL_MS` ใน `app_config.h`

ไฟล์ `include/secrets.h` ถูก `.gitignore`; แม่แบบมีค่าเป็นข้อความว่าง
โปรแกรมยัง Build และวัด LDR ได้หากไม่มีไฟล์ค่าจริงหรือยังไม่ได้กรอกชื่อ WiFi
หากเปลี่ยนชื่อหรือรหัสผ่าน ต้อง Build/Upload ใหม่

อ้างอิง [Espressif: WiFi Station API](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/wifi.html#basic-usage)
และ [ESP32 Datasheet](https://www.espressif.com/sites/default/files/documentation/esp32_datasheet_en.pdf)

## แบบฝึกหัดของโปรแกรมกะพริบ LED ต้นฉบับ

1. **เปลี่ยนความเร็ว:** แก้ `LED_STEP_MS` ใน `app_config.h` เป็น `200` แล้ว upload ใหม่
2. **เปลี่ยนข้อความ:** แก้ `HELLO_TEXT` เป็นชื่อของตัวเอง ดูผลใน Serial Monitor
3. **แก้ลำดับสี:** แก้อาร์เรย์ `steps[]` ใน `main.cpp` ให้เป็น น้ำเงิน → เขียว → แดง (สลับลำดับ) แล้วสังเกตว่าการวนซ้ำใช้ `% STEP_COUNT` ปรับตามจำนวนสมาชิกให้เองโดยไม่ต้องแก้ที่อื่น
   ท้าทายต่อ: เพิ่มขั้นสีเหลืองโดยใช้ `{"YELLOW", cyd::LED_PIN_R, true, true, false}`
4. **อ่านโค้ด:** ใน `main.cpp` โปรแกรมไม่ใช้ `delay()` ในการรอแต่ละสี แต่ใช้อะไร และดีกว่าอย่างไร
5. **ท้าทาย:** ทำให้ Serial พิมพ์ `uptime` (วินาทีตั้งแต่เปิดเครื่อง) ต่อท้ายทุกบรรทัด ด้วย `millis() / 1000`

## ใช้เป็น Template เริ่ม Lab ใหม่

1. คัดลอกโฟลเดอร์ `Test ESP32-dev` ไปวางข้างๆ แล้วตั้งชื่อใหม่ เช่น `Lab2_My_First_Screen`
2. เปิดโฟลเดอร์ใหม่ด้วย VS Code (Open Folder)
3. แก้ `src/main.cpp` ให้เป็นโปรแกรมของ Lab ใหม่ และปรับ `app_config.h`
4. ถ้าต้องใช้จอ/touch/LVGL ให้เอา `;` หน้า `lib_deps` ใน `platformio.ini` ออก
5. ถ้าต้องใช้ WiFi ให้คัดลอก `include/secrets.h.example` เป็น `include/secrets.h` แล้วกรอกค่า (ไฟล์นี้ไม่ถูกอัปขึ้น git)

> ห้ามแก้ pin ใน `board_config.h` ตามอำเภอใจ และ **ห้ามใช้ GPIO21 ต่อเซนเซอร์** (เป็นขา backlight ของจอ)
> ใช้ GPIO27 แทน (ดูคอมเมนต์ท้ายหมวด expansion ใน `board_config.h`)

เมื่อต้องใช้จอ + Touch + microSD พร้อมกัน ให้จัดการ SPI ก่อน: จอใช้ HSPI ส่วน Touch ใน template
และ SD ใช้ VSPI แต่ต่อคนละชุดขา ESP32 รุ่นนี้มี hardware SPI ให้ใช้งานทั่วไปสองชุด
จึงต้องจัดการสลับขา/บัสอย่างถูกต้อง หรือใช้ไลบรารี Touch ที่รองรับ software SPI
อ้างอิง [SPI ของ Espressif](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/spi.html)
และ [ข้อมูลขาของ CYD](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display/blob/main/PINS.md)

## ปัญหาที่พบบ่อย

| อาการ | สาเหตุ / วิธีแก้ |
|---|---|
| ไม่เห็นพอร์ต USB บน Ubuntu | ตรวจว่าสายส่งข้อมูลได้ และ USB เชื่อมต่อกับ Ubuntu ปลายทาง; ใช้ Device List ตรวจ |
| upload ไม่ผ่าน: port busy | ปิด Serial Monitor, ตรวจชื่อพอร์ต `/dev/ttyUSB*` หรือ `/dev/ttyACM*` (อาจเปลี่ยนหลังเสียบใหม่) |
| upload: `Permission denied` | ตรวจ `id ubuntu` ว่ามี `dialout`; สคริปต์ Upload จะใช้สิทธิ์กลุ่มใหม่แม้เซสชัน VS Code ยังเก่า |
| upload / monitor: `[Errno 5] Input/output error` หรือ USB timeout | ตรวจ kernel log; ลอง `sudo usbreset 1a86:7523` สำหรับ CH340 ที่ตรวจพบ หรือถอดเสียบ USB แล้วเชื่อมกลับเข้า Ubuntu ใน VMware ตรวจพอร์ตอีกครั้ง แม้ Device List ยังเห็นพอร์ตก็อาจเปิดใช้งานไม่ได้ |
| Serial Monitor มีแต่ตัวอักษรเพี้ยน | ตั้ง baud 115200 |
| สีผสมหรือตรงข้ามกับที่ควรเป็น (เช่น ตอน RED เห็นเป็นฟ้า) | ตรวจ `LED_ACTIVE_LOW` ให้ตรงกับวงจร; template นี้ตั้งเป็น `true` เปลี่ยนเป็น `false` เฉพาะเมื่อยืนยันว่าบอร์ดเป็น active-HIGH |
| ติดสีเดียวแต่ผิดชื่อ (เช่น ตอน GREEN เห็นเป็นแดง) | เลข GPIO ของสีนั้นผิด ดูเลข GPIO ใน Serial แล้วสลับ `LED_PIN_R/G/B` ใน `board_config.h` ให้ตรงสีที่เห็นจริง |
| LED ไม่ติดเลย | ตรวจว่าไม่ได้แก้ `LED_PIN_R/G/B` และมองให้ถูกตำแหน่ง (LED RGB ดวงเล็กบนบอร์ด) |
| ไม่เห็นข้อความตอนเปิดเครื่อง | เปิด monitor ช้ากว่าบอร์ดบูต กดปุ่ม EN/RESET บนบอร์ด |

## เช็คลิสต์ผ่าน Lab

- [ ] build และ upload สำเร็จ
- [ ] เห็น `Hello World` ใน Serial Monitor
- [ ] RGB LED แสดงแดง → เขียว → น้ำเงิน และสีตรงกับชื่อใน Serial ทุกขั้น
- [ ] เปลี่ยนความเร็วหรือข้อความด้วย `app_config.h` ได้
- [ ] อธิบายได้ว่า `board_config.h` กับ `app_config.h` ต่างกันอย่างไร

## สถานะการตรวจและการสร้างไฟล์ VS Code ใหม่

เมื่อ 11 ตุลาคม 2569 ตรวจ Build และ Upload โปรแกรมหลักผ่านบน Ubuntu แล้ว โดย Upload ตรวจ hash ครบทุกส่วน
อ่าน Serial ได้ในการตรวจครั้งแรกและวัดช่วงเริ่มสีได้ประมาณ 2.2 วินาที แต่พบ USB timeout กลับมาอีกตอนเปิด
Monitor หลัง Upload ผลตรวจล่าสุดและข้อจำกัดอยู่ใน [VERIFICATION_REPORT.md](VERIFICATION_REPORT.md)
สี LED ที่มองเห็นยังต้องตรวจจากบอร์ดจริง ส่วนจอ TFT / Touch / LVGL ยังไม่ถูกใช้ใน Lab 1

ถ้าย้ายโฟลเดอร์โปรเจกต์ ให้เปิดโฟลเดอร์ใหม่ แล้วสร้างไฟล์ IntelliSense / Debug ใหม่:

```bash
"$HOME/.platformio/penv/bin/platformio" project init --ide vscode
```

`c_cpp_properties.json` และ `launch.json` เป็นไฟล์ที่ PlatformIO สร้างให้
จึงไม่ควรคัดลอกพาธของเครื่องอื่นหรือแก้ไฟล์เหล่านี้แทนการแก้ `platformio.ini`

เอกสารอ้างอิง: [Project init](https://docs.platformio.org/en/latest/core/userguide/project/cmd_init.html),
[Device list](https://docs.platformio.org/en/latest/core/userguide/device/cmd_list.html)
