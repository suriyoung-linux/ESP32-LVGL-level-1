# รายงานตรวจ Build / Upload / Serial / PIO Home — ESP32 CYD

ตรวจเมื่อ 11 ตุลาคม 2569 บน Ubuntu ผ่าน VMware โดยใช้ PlatformIO environment `esp32dev`
บอร์ดที่ตรวจพบคือ ESP32-D0WD-V3, Flash 4 MB, USB Serial CH340 (`1a86:7523`) ที่ `/dev/ttyUSB0`

Build ผ่าน และอัปโหลดผ่านที่ 921600 baud หลังผู้ใช้เชื่อม USB กลับเข้า Ubuntu
แต่การเปิด Serial หลังอัปโหลดครั้งนั้นพบ USB timeout ซ้ำ จึงต้องตรวจผลอัปโหลดกับผล Monitor แยกกัน
ภาพล่าสุดที่ผู้ใช้ส่งแสดง Upload สำเร็จอีกครั้งใน 11.20 วินาที พร้อม `Hash of data verified.`
และ `Hard resetting via RTS pin...`; ผลรอบนี้ยืนยันจากภาพ ยังไม่ได้อ่าน Serial หลังรอบนี้
แท็บ PIO Home ในภาพยังว่าง แต่หลังเปิดเซสชันเดียวกันในเบราว์เซอร์ ผู้ใช้ยืนยันว่าเห็นหน้า Home พร้อมปุ่ม New Project / Open Project แล้ว

| รายการ | ผลตรวจ |
|---|---|
| Build โปรแกรมหลักหลังแก้ | `[SUCCESS]`; RAM 21,480 / 327,680 bytes (6.6%); โปรแกรม 271,325 / 1,310,720 bytes (20.7%) |
| ตัวอย่าง `loop()` ในใบงาน | ตรวจแล้วตรงกับ `src/main.cpp` |
| เฉลยกะพริบข้อ 6 หลังแก้ | Build ผ่านในโปรเจกต์แยก `/tmp/cyd-lab6-build`; ยังไม่ได้ Upload แบบฝึกหัดลงบอร์ด |
| สิทธิ์พอร์ต | ผู้ใช้ `ubuntu` อยู่ในกลุ่ม `dialout`; มีสิทธิ์อ่าน/เขียน `/dev/ttyUSB0` |
| โปรแกรมอื่นจองพอร์ตขณะเกิดข้อผิดพลาด | ไม่พบจาก `fuser` |
| `99-platformio-udev.rules` | ยังไม่ติดตั้งใน `/etc/udev/rules.d` หรือ `/lib/udev/rules.d`; เตรียมไฟล์จาก PlatformIO ไว้ใน `.pio/verification` แล้ว |
| สิทธิ์รีเซ็ต USB / ติดตั้ง rules | เซสชันนี้ใช้ `sudo` แบบไม่โต้ตอบไม่ได้; ต้องรันขั้นตอนผ่าน Terminal และยืนยันบนเครื่อง |
| Upload ครั้งแรก | ล้มเหลวตอนเปิดพอร์ต: `[Errno 5] Input/output error`; ยังไม่เริ่มเขียน Flash |
| Upload หลังเชื่อม USB ใหม่ | `[SUCCESS]` ใน 6.39 วินาที; `Hash of data verified.` ครบทั้ง 4 ส่วน และ `Hard resetting via RTS pin...` |
| Upload ในภาพล่าสุดของผู้ใช้ | `[SUCCESS]` ใน 11.20 วินาที; เห็นบรรทัดตรวจ hash และ reset; ไม่ได้รันทดสอบรอบนี้โดยผู้ช่วย |
| Serial ในการตรวจครั้งก่อน | อ่านที่ 115200 baud ได้ลำดับ RED → OFF → GREEN → OFF → BLUE และช่วงเริ่มสีประมาณ 2.2 วินาที |
| Serial หลัง Upload รอบแรกที่ผ่าน | เปิดพอร์ตไม่ได้ด้วย `Input/output error`; kernel บันทึก USB timeout ซ้ำ |
| สี LED ที่เห็นจริง / จอ / Touch / SD | ยังไม่ได้ตรวจทางกายภาพ; Lab 1 ไม่เรียกใช้จอ/Touch/SD |

## ปัญหาที่พบและการแก้ไข

1. **การเชื่อมต่อ USB มี timeout แม้ชื่อพอร์ตยังปรากฏ**
   ทั้ง esptool และการเปิดพอร์ตโดยตรงพบ `Input/output error` และ kernel บันทึก
   `ch341-uart ttyUSB0: failed to read modem status: -110` พร้อม `failed to send/receive control message: -110`
   หลังผู้ใช้เชื่อม USB กลับเข้า Ubuntu สามารถอัปโหลดได้ที่ความเร็วเดิม
   จึงยืนยันว่าข้อผิดพลาดที่เปิดพอร์ตครั้งแรกเกิดในเส้นทาง USB/ไดรเวอร์
   หลักฐานนี้ยังแยกไม่ได้ว่า timeout มาจากสาย อุปกรณ์ หรือการส่งต่อ USB ของ VMware
   เพิ่มวิธีแยก `Permission denied`, `port busy` และ `Input/output error` ใน README และใบงาน

   ตรวจเพิ่มเติมจากภาพ Upload ที่ล้มเหลวก่อนหน้านี้: ไม่มีโปรแกรมจองพอร์ต ผู้ใช้อยู่ใน `dialout` และ kernel
   ยังคงบันทึก control message timeout `-110` ดังนั้นคำเตือน udev rules ไม่ใช่หลักฐานว่า
   การเปิดพอร์ตครั้งนี้ล้มเหลวเพราะสิทธิ์ เตรียมไฟล์ rules จาก PlatformIO ที่ติดตั้งอยู่ไว้ให้ติดตั้งได้

2. **ใบงานและเฉลยใช้เวลา/ตัวอย่าง loop เก่า**
   ปรับตัวอย่างให้มี `ledOn`, `LED_OFF_MS` และข้อความ `LED = OFF` เหมือน `src/main.cpp`
   ระยะเริ่มสีถัดไปเท่ากับช่วงติด + ช่วงดับ: 2000 + 200 = 2200 ms
   แบบฝึกหัด 500 ms จึงเริ่มสีถัดไปทุก 700 ms เมื่อคงช่วงดับ 200 ms
   ปรับเฉลยข้อ 6 ให้กะพริบเฉพาะในช่วงสีและคงช่วงดับระหว่างสีไว้

3. **คำอธิบาย SPI ของ template ไม่ตรงกับฮาร์ดแวร์**
   แก้คำว่า microSD ใช้ “SPI ชุดที่ 3” เป็น VSPI และอธิบายว่า ESP32 รุ่นนี้มี HSPI/VSPI
   สำหรับอุปกรณ์ทั่วไปสองชุด Touch ใน template ใช้ VSPI เช่นเดียวกับ SD แต่เดินสายคนละขา
   จึงต้องจัดการสลับขา/บัส หรือใช้ไลบรารี Touch ที่รองรับ software SPI เมื่อต้องใช้พร้อมกัน
   การแก้ครั้งนี้เป็นคำอธิบายใน `include/board_config.h`; Lab 1 ใช้เฉพาะ LED และ Serial

ไฟล์ที่แก้: `README.md`, `WORKSHEET.md`, `WORKSHEET_ANSWERS.md`, `include/board_config.h`
และรายงานฉบับนี้

## ผลตรวจ PIO Home ที่แสดงหน้าว่าง / Reconnecting

- ผู้ใช้ยืนยันว่า Ports มีพอร์ต 8008; ภาพล่าสุดแสดง Forwarded Address เป็น `127.0.0.1:8008`
- เซิร์ฟเวอร์ Home ฝั่ง Ubuntu ทำงานที่ `127.0.0.1:8008`; หน้าเซสชัน, favicon และ JavaScript ทั้งสองไฟล์ตอบ HTTP 200
- WebSocket เชื่อมต่อได้ และคำขออ่านข้อมูล `app.get_state` ได้ผลลัพธ์ JSON-RPC สำเร็จ
- ราก URL `/` ของเซิร์ฟเวอร์นี้มีการป้องกันเซสชัน ตรวจการเปิดเฉพาะ `http://127.0.0.1:8008/` ได้ HTTP 403
- ส่งคำสั่งเปิดหน้า Home ด้วยเซสชันเดิมผ่าน VS Code Remote CLI ไปยังเบราว์เซอร์; ผู้ใช้ยืนยันว่าเห็นหน้า PlatformIO Home พร้อมปุ่ม New Project / Open Project
- ยืนยันว่าเซิร์ฟเวอร์และเส้นทางส่งต่อพอร์ตใช้แสดง Home ในเบราว์เซอร์ได้แล้ว ต้องคงการเชื่อมต่อ SSH และพอร์ตที่ส่งต่อระหว่างใช้งาน
- อาการหน้าว่างยังเหลือในแท็บ PIO Home ของ VS Code; ยังไม่มีข้อความจาก Console ที่ระบุสาเหตุ แนะนำลอง `Developer: Reload Webview` ตามเอกสาร VS Code โดยยังไม่ได้ยืนยันผลหลังรีโหลด

### การแก้ Reconnecting เมื่อเปิด URL ราก

- ผู้ใช้เปิดเฉพาะ `127.0.0.1:8008` แล้วเห็น Reconnecting พร้อม Core / รายการโปรเจ็กต์ค้างที่ Loading
- ตรวจ `/wsrpc` ที่ URL รากได้ HTTP 403; WebSocket ที่อยู่ใต้ `/session/.../` เชื่อมได้ และ `app.get_state` ตอบสำเร็จพร้อม Core 6.2.0
- เพิ่ม [scripts/open_pio_home.py](scripts/open_pio_home.py) เพื่อหา Home เซสชันที่กำลังทำงาน ตรวจ HTTP และเปิด URL เต็มผ่าน VS Code ไปยังเบราว์เซอร์บน Windows
- เพิ่ม task **PlatformIO: Open Home in Browser (SSH)** ใน [.vscode/tasks.json](.vscode/tasks.json) และอัปเดตคู่มือกับ README โดยใช้พอร์ต 8008
- ทดสอบค้นหา URL จากเซิร์ฟเวอร์จริงและส่งคำสั่งเปิดเบราว์เซอร์ผ่านแล้ว; ผู้ใช้ยืนยันว่าหน้าใหม่เห็น Core 6.2.0 และไม่มี Reconnecting
- เมื่อกลับมาใช้งาน ให้เชื่อม SSH เปิด PlatformIO Home ตรวจพอร์ต 8008 แล้วเรียก task นี้ รหัสเซสชันจะถูกอ่านใหม่ทุกครั้ง
- เพิ่มขั้นตอนเปิดใช้งานประจำวันไว้ต้นคู่มือ และใช้เมนู **Terminal → Run Task...** เพื่อเปิดงานได้โดยไม่ต้องจำ URL
- ปรับสคริปต์ให้แสดงข้อความสั้นๆ เมื่อส่งคำสั่งเปิดเบราว์เซอร์สำเร็จ; แสดง URL เต็มเมื่อใช้ `--url-only` หรือเปิดอัตโนมัติไม่สำเร็จ
- ทดสอบสคริปต์หลังปรับ: อ่านเซสชันจาก Home จริงและส่งคำสั่งเปิดผ่าน VS Code สำเร็จ (`exit code 0`); ข้อความเมื่อสำเร็จไม่มีรหัสเซสชัน

### การแก้เพื่อเปิด Home ในแท็บ VS Code

- ตรวจพบว่า extension host เริ่มใหม่ แต่ Home เก่ายังทำงานบนพอร์ต 8008 ด้วยรหัสเซสชันเดิม; helper ของส่วนขยายสร้างรหัสใหม่ทุกครั้งที่โหลด
- เพิ่ม `.vscode/settings.json` ให้ `pioHomeServerHttpPort` เป็น `0` เพื่อเลือกพอร์ตว่างและไม่ชนเซิร์ฟเวอร์ที่เหลือจากเซสชันก่อน
- แก้ส่วนขยาย PlatformIO IDE 3.3.4 ที่ติดตั้งอยู่ให้ URL ของ iframe ผ่าน `vscode.env.asExternalUri` ตาม [เอกสาร Remote Extensions ของ VS Code](https://code.visualstudio.com/api/advanced-topics/remote-extensions#option-1---use-asexternaluri)
- สำรองต้นฉบับข้างไฟล์ส่วนขยายในชื่อ `extension.js.before-pio-home-ssh-fix`; สคริปต์ `scripts/fix_pio_home_ssh.py` ใช้ซ้ำหรือคืนต้นฉบับด้วย `--restore` ได้
- ตรวจ JavaScript syntax ผ่าน และทดสอบเมธอดสร้างแท็บด้วย VS Code API จำลอง: เริ่มเซิร์ฟเวอร์ก่อนแปลง URL, ใช้พอร์ตที่ tunnel ส่งกลับ และรักษาเส้นทาง session ครบ
- ปรับสคริปต์เปิดเบราว์เซอร์เดิมให้อ่านพอร์ตอัตโนมัติและเลือกเซิร์ฟเวอร์ล่าสุด เพื่อรองรับการเปลี่ยนพอร์ต
- ยังต้อง Reload Window แล้วเปิด PlatformIO Home เพื่อโหลดโค้ดใหม่; ยังไม่ได้ยืนยันภาพแท็บจากฝั่ง Windows หลังการแก้

## หลักฐานและวิธีตรวจซ้ำ

- [Upload ครั้งแรก](.pio/verification/upload-initial.log)
- [Upload หลังเชื่อม USB ใหม่](.pio/verification/upload-after-reconnect.log)
- [Build หลังแก้](.pio/verification/build-after-fixes.log)
- [Build เฉลยข้อ 6](.pio/verification/lab6-build.log)
- [ไฟล์ udev rules จาก PlatformIO ที่ติดตั้งอยู่](.pio/verification/99-platformio-udev.rules)
- [ผลตรวจ HTTP / WebSocket ของ PIO Home](.pio/verification/pio-home-health.json)

ไฟล์หลักฐานอยู่ใน `.pio/verification` ซึ่งถูก `.gitignore` และอาจหายเมื่อทำความสะอาดโฟลเดอร์ `.pio`

```bash
"$HOME/.platformio/penv/bin/platformio" run -e esp32dev -j 1 -t upload --upload-port /dev/ttyUSB0
"$HOME/.platformio/penv/bin/platformio" device monitor -b 115200 --port /dev/ttyUSB0 --filter time
```

เลือกพอร์ตให้ตรงกับ Device List ปิด Monitor ก่อน Upload และตรวจบรรทัด `Hash of data verified.`
กับ `[SUCCESS]` จากนั้นดู Serial และเปรียบเทียบสี LED จริงบนบอร์ด

ขั้นตอนระบบที่ยังต้องดำเนินการจาก Terminal ในโฟลเดอร์โปรเจกต์:

```bash
sudo install -m 644 .pio/verification/99-platformio-udev.rules /etc/udev/rules.d/99-platformio-udev.rules
sudo udevadm control --reload-rules
sudo usbreset 1a86:7523
```

การรีเซ็ตนี้เลือกเฉพาะ CH340 ที่ตรวจพบ ไม่ใช่ VMware USB hub หรือเมาส์ และยังไม่ได้รันในเซสชันนี้
หากพบ USB `Input/output error` อีกและ reset ไม่สำเร็จ ให้ถอดเสียบ USB และเชื่อม QinHeng/USB Serial กลับเข้า Ubuntu ใน VMware
ภาพ Upload ล่าสุดสำเร็จแล้ว จึงไม่ต้องรีเซ็ต USB เพียงเพราะแท็บ PIO Home ว่าง

อ้างอิง: [esptool Troubleshooting](https://docs.espressif.com/projects/esptool/en/latest/esp32/troubleshooting.html),
[Espressif SPI](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/spi.html),
[CYD Pins](https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display/blob/main/PINS.md),
[VS Code: Inspecting and debugging webviews](https://code.visualstudio.com/api/extension-guides/webview#inspecting-and-debugging-webviews)

## ตรวจเพิ่ม: โปรแกรม LDR + RGB LED + จอ TFT

วันที่ 11 ตุลาคม 2569 เพิ่มการแสดงตัวเลข LDR และรูปสถานะ LED บนจอ TFT แนวนอน 320×240
ใช้ `TFT_eSPI@2.5.43` และไดรเวอร์ ILI9341 ตามค่าของโปรเจกต์
อ่านค่าครั้งแรกทันที แล้วอัปเดตทุก 500 ms: > 1000 แดง, 500–1000 รวมขอบเขียว, < 500 น้ำเงิน
ตัวเลขและแถบใช้สีเดียวกับ LED ที่สั่งติด รูปหลอดแสดง `ON` หรือ `OFF`

Build ด้วย `platformio run -e esp32dev -j 1` ผ่าน `[SUCCESS]`:

- RAM: 21,956 / 327,680 bytes (6.7%)
- Flash: 306,825 / 1,310,720 bytes (23.4%)
- ไลบรารีแจ้งเตือนว่าไม่ได้กำหนด `TOUCH_CS`; โปรแกรมนี้ใช้จอแสดงผลและไม่ได้เรียกฟังก์ชัน touch

ยังไม่ได้ Upload เวอร์ชันนี้หรือยืนยันภาพจอ สี LED และการตอบสนองของ LDR บนบอร์ดจริง
ผล Upload/Serial ข้างต้นเป็นของโปรแกรมก่อนเพิ่ม LDR และ TFT

## ตรวจเพิ่ม: แสดงแรงดัน V และค่าประมาณ R ของ LDR

ใช้ข้อมูล ADC calibration แปลงค่าดิบเดียวกับที่เลือกสี LED เป็นแรงดัน GPIO34 เทียบกับ GND
แสดง V และ `R~` บน TFT และ Serial โดยอ้างอิง R15/R19 = 1 MOhm และ Vs = 3.3 V
ตามวงจรต้นฉบับ ค่า R คิดผลของ R19 ที่ต่อขนานกับ LDR ด้วย
แสดง `R: N/A` เมื่อแรงดันต่ำกว่าช่วงที่กำหนดหรือไม่สามารถหา R ที่เป็นบวกและมีค่าจำกัดได้

Build รอบสุดท้ายด้วย `platformio run -e esp32dev -j 1` ผ่าน `[SUCCESS]`:

- RAM: 22,004 / 327,680 bytes (6.7%)
- Flash: 309,365 / 1,310,720 bytes (23.6%)
- ไม่มีคำเตือน deprecated จากโค้ดแปลงแรงดัน; ยังมีคำเตือน `TOUCH_CS` จากไลบรารีที่ไม่ได้ใช้ touch
- ทดสอบฟังก์ชันจริงใน `include/ldr_measurement.h` ด้วย g++ ผ่าน 15 กรณี:
  คำนวณย้อนจาก LDR ที่ทราบค่า 100 kOhm, 1 MOhm, 5 MOhm, 20 MOhm;
  จุดแรงดันขั้นต่ำ 0.15 V; และ input ที่ใช้ไม่ได้ 10 กรณี (รวมแรงดันที่ทำให้กระแสเป็นศูนย์, NaN และ infinity)
  ไฟล์ตรวจชั่วคราวอยู่ที่ `/tmp/cyd-ldr-resistance-check.cpp`

ยังไม่ได้ Upload เวอร์ชันนี้ ตรวจภาพจอ หรือเทียบ V/R กับเครื่องมือวัดบนบอร์ดจริง
ค่า R เป็นค่าประมาณตามวงจรอ้างอิง ยังไม่ได้ยืนยันค่า R15/R19 และแรงดันจ่ายจริงของบอร์ดผู้ใช้

## ตรวจเพิ่ม: เชื่อมต่อ WiFi และแสดง IPv4

เพิ่ม WiFi Station mode รับ IPv4 จาก DHCP และแสดงสถานะ/IPv4 บน TFT และ Serial
อ่านชื่อเครือข่ายและรหัสผ่านจาก `include/secrets.h` ที่ถูก gitignore
หากไม่มีไฟล์นี้ ใช้แม่แบบว่าง และหาก SSID ว่างจะแสดง `SET SSID` โดยยังวัด LDR ได้
ตรวจสถานะทุก 250 ms และลองเชื่อมต่อใหม่ทุก 15 วินาทีเมื่อไม่มีการเชื่อมต่อพร้อม IPv4
ไม่มีวงวนรอ WiFi จึงยังอ่าน LDR และอัปเดต LED/V/R ทุก 500 ms
ลบ IPv4 เดิมเมื่อหลุด และอัปเดตเมื่อได้รับ IP ใหม่

Build ด้วย `platformio run -e esp32dev -j 1` ผ่าน `[SUCCESS]`:

- RAM: 43,988 / 327,680 bytes (13.4%)
- Flash: 766,273 / 1,310,720 bytes (58.5%)
- ใช้ `WiFi` จาก framework; ไม่ต้องเพิ่ม dependency
- มีคำเตือน `TOUCH_CS` เดิมจาก TFT_eSPI ซึ่งไม่เกี่ยวกับการเชื่อม WiFi
- ตรวจด้วย g++ (`-Wall -Wextra -Werror`) โดยนำฟังก์ชัน WiFi จริงจาก `src/main.cpp`
  มาใช้กับ WiFi/TFT จำลอง ผ่านกรณี SSID ว่าง, เริ่มเชื่อมต่อ, เชื่อมต่อสำเร็จ,
  ไม่วาดซ้ำเมื่อสถานะเดิม, เปลี่ยน IPv4, การเชื่อมต่อหลุดและลบ IP เก่า,
  ลองใหม่ตามเวลา, ไม่พบ SSID, เชื่อมต่อไม่สำเร็จ, รอ DHCP และ millis() วนกลับผ่านศูนย์
  สคริปต์ทดสอบชั่วคราวอยู่ที่ `/tmp/cyd-wifi-state-check.cpp`

ไฟล์ค่าจริงที่สร้างไว้ยังมี SSID/รหัสผ่านว่าง ไม่ได้ Upload หรือทดสอบการเชื่อมต่อกับเครือข่ายจริง
ก่อนใช้งานต้องกรอก `include/secrets.h` แล้ว Upload ใหม่
ตรวจ TFT/Serial แสดง `CONNECTED` และ IPv4 จริง จากนั้นปิด/เปิด WiFi ของเราเตอร์
เพื่อตรวจการลบ IP และการเชื่อมต่อกลับพร้อมกับการทำงานต่อเนื่องของ LDR

## ตรวจบนบอร์ดจริง: WiFi ขึ้น SSID NOT FOUND

ตรวจ Serial จาก `/dev/ttyUSB0` ที่ 115200 baud พบ `SSID NOT FOUND` ซ้ำ
เพิ่มการสแกนแบบ asynchronous หนึ่งครั้งตอนเปิดเครื่อง และพิมพ์ผลของเครือข่ายเป้าหมาย
พร้อม RSSI/channel/security หากพบ รวมทั้งรหัสเหตุผลจาก WiFi event
callback พิมพ์ Serial เท่านั้น ไม่วาด TFT หรือแก้ตัวแปรสถานะของ loop
ไม่ได้เปลี่ยนชื่อเครือข่ายหรือรหัสผ่านของผู้ใช้ และไม่บันทึกสองค่านี้ในรายงาน/หลักฐาน

การสแกนค่าเริ่มต้น 300 ms ต่อ channel หมดเวลาก่อนเสร็จ
ปรับเป็น 500 ms ต่อ channel (framework ให้ timeout 10 วินาที)
รอบสุดท้าย Build และ Upload ผ่าน `[SUCCESS]` / `Hash of data verified.`:

- RAM: 43,988 / 327,680 bytes (13.4%)
- Flash: 770,593 / 1,310,720 bytes (58.8%)
- ผลจากบอร์ดจริง: `scan done, status = 0, count = 25`
- `25 networks visible, 0 match configured SSID`
- ชื่อ SSID ที่เฟิร์มแวร์ใช้ตรงกับค่าปัจจุบันใน `include/secrets.h` (ตรวจในหน่วยความจำโดยไม่พิมพ์ชื่อ)
- ไดรเวอร์รายงาน `reason = 201 (NO_AP_FOUND)` และยังไม่มี IPv4
- อ่าน LDR ต่อเนื่อง 47 ครั้งในช่วงตรวจประมาณ 24 วินาที รวมช่วงสแกน/รอเชื่อมต่อ
- ทดสอบฟังก์ชันจริงด้วย WiFi/TFT จำลองผ่านกรณีสแกนรอผล, พบเป้าหมาย,
  ไม่พบเครือข่าย, สแกนเริ่มไม่ได้/ล้มเหลว, เชื่อมต่อ, หลุด, การลองใหม่ และ millis() วนกลับ
  ไฟล์ทดสอบอยู่ที่ `/tmp/cyd-wifi-scan-check.cpp`

หลักฐาน Serial ที่ปิดบังชื่อเครือข่ายอยู่ที่
[wifi-scan-diagnostic.log](.pio/verification/wifi-scan-diagnostic.log)
บันทึก fingerprint เฟิร์มแวร์หลัง Upload ใน `.pio/verification/uploaded-firmware.sha256`

ยังไม่เชื่อม WiFi สำเร็จ: ต้องตรวจชื่อที่เราเตอร์ปล่อยบนย่าน 2.4 GHz จริง
แม้โทรศัพท์เชื่อมต่อชื่อที่ตั้งไว้ได้ โทรศัพท์อาจเชื่อมย่าน 5 GHz
ยังไม่ได้ตรวจการตั้งค่าเราเตอร์หรือยืนยันสาเหตุด้านเราเตอร์/สัญญาณ
ขั้นแยกปัญหาคือเปิด Hotspot 2.4 GHz ใกล้บอร์ด ใส่ชื่อ/รหัสผ่านของ Hotspot แล้ว Upload ใหม่

## ตรวจเพิ่ม: Weather API, จอสัมผัส และหน้า LDR / RGB

เพิ่มหน้า WEATHER เลือกเชียงใหม่ กรุงเทพฯ และพื้นที่โดยประมาณจาก public IP ของ WiFi
แยกชุด Weather, Rain/Cloud, Wind และ PM2.5 พร้อมเวลาท้องถิ่นและเวลาของข้อมูล
ใช้ Open-Meteo/CAMS ผ่าน HTTPS ที่ตรวจ CA และประมวลผลเครือข่ายใน FreeRTOS worker
แยกหน้า LDR / RGB พร้อม START/STOP และบันทึกสถานะใน NVS
เพิ่ม REST JSON API บน ESP32 และระบบปรับเทียบสัมผัสสี่จุด
รายละเอียดใช้งานอยู่ใน [WEATHER_API_GUIDE.md](WEATHER_API_GUIDE.md)

Build และ Upload เฟิร์มแวร์ชุดนี้ลง `/dev/ttyUSB0` สำเร็จ:

- RAM: 49,956 / 327,680 bytes (15.2%; ยังไม่รวมการใช้ heap ระหว่างทำงาน)
- Flash: 1,031,509 / 1,310,720 bytes (78.7%)
- Upload รายงาน `Hash of data verified.` และ `[SUCCESS]`
- คำเตือน `TOUCH_CS` มาจาก helper ของ TFT_eSPI; โปรแกรมใช้ XPT2046_Touchscreen บน VSPI แยกจากจอ HSPI
- Root filesystem เต็ม จึงตั้ง build directory เป็น `/tmp/cyd-esp32-weather-build`
  และย้ายเฉพาะ download cache ที่สร้างใหม่ได้ไป `/tmp`; ไม่ได้ลบโค้ดหรือเปลี่ยน credentials

การตรวจอัตโนมัติ `python3 scripts/check_weather.py` ผ่านด้วย g++ และ
AddressSanitizer/UndefinedBehaviorSanitizer ใช้ฟังก์ชันจริงสำหรับตรวจ JSON,
ค่าขาดหาย/ช่วงค่าที่ไม่ถูกต้อง, ฝน rain + showers, PM2.5, พิกัด, ทิศลม,
การแปลงสัมผัส, ขอบเขต LDR 499/500/999/1000/1001, GPIO แบบ active LOW,
START/STOP และ API handlers รวมคำสั่ง JSON ที่ผิดชนิด
ตรวจ HTTPS จากเครื่องพัฒนาโดยใช้ CA roots ชุดเดียวกับเฟิร์มแวร์แล้ว
บริการ weather, air quality และ IP geolocation ส่ง HTTP 200

ผลบนบอร์ดจริงหลัง Upload (2026-10-11):

- สแกนพบเครือข่ายเป้าหมาย: 21 networks, 1 match, RSSI -58 dBm, channel 11
- ช่วงเริ่มต้นพบ `HANDSHAKE_TIMEOUT` และ `AUTH_FAIL`; ต่อมาเชื่อมต่อสำเร็จเอง
  ได้ IPv4 `10.137.69.121` โดยไม่ได้เปลี่ยน SSID/รหัสผ่านในโค้ด
- อ่าน LDR ต่อเนื่อง 54 ครั้งในช่วงประมาณ 28.7 วินาที ไม่พบข้อความ boot error ในช่วงตรวจ
- ผู้ใช้ยืนยันว่าปรับเทียบสัมผัสแล้ว และปุ่มเปลี่ยนหน้าทำงานได้
- GET `/api/status` ส่ง HTTP 200: WiFi connected, clock synchronized และ worker ready เป็น true
- GET `/api/locations`, weather ทั้งสามพื้นที่ และ `/api/rgb` ส่ง HTTP 200
- ทั้งสามพื้นที่มี weather และ PM2.5 พร้อมใช้งาน, stale เป็น false และไม่มี error
  จึงยืนยันว่า ESP32 เองเรียก HTTPS, รับข้อมูล และแปลงข้อมูลได้จริง
- เวลา API ณ ตรวจคือ 20:14:12 เขต Asia/Bangkok (UTC+7)
- ขณะตรวจ RGB อยู่สถานะ stopped; ไม่ได้เปลี่ยนสถานะผ่าน API ระหว่างตรวจนี้

ค่าตัวอย่างจาก API ของบอร์ด ณ เวลาตรวจ (ค่าจากแบบจำลอง ไม่ใช่เซนเซอร์บน CYD):

| พื้นที่ | อุณหภูมิ °C | RH % | ฝน mm / 15 min | เมฆ % | ลม km/h จากทิศ | PM2.5 µg/m³ |
|---|---:|---:|---:|---:|---|---:|
| เชียงใหม่ | 24.7 | 93 | 0.0 | 34 | 3.1, NNW (332°) | 13.3 |
| กรุงเทพฯ | 25.6 | 96 | 0.3 | 98 | 4.9, W (280°) | 7.8 |
| WiFi area | 25.3 | 97 | 0.8 | 97 | 4.6, WSW (249°) | 16.9 |

PM2.5 ชุดนี้มีช่วงข้อมูล 3,600 วินาที แยกจากฝน 900 วินาที
ตำแหน่ง WiFi area ที่บริการคืนมามีป้าย approximate และเมือง Bang Bua Thong
ยังไม่ได้ยืนยันว่าตรงกับพื้นที่บ้านจริง เพราะ public IP ระบุตำแหน่งได้เพียงโดยประมาณ
ยังไม่ได้ตรวจด้วยสายตาว่าทุกชุดข้อมูลบน TFT วาดตรงกับ API หรือยืนยันสี LED จริงขณะ START/STOP

หลักฐานอยู่ใน `.pio/verification/weather-upload.log`,
`.pio/verification/weather-serial.log`, `.pio/verification/weather-api-live.json`
และ `.pio/verification/uploaded-firmware.sha256`

## ตรวจเพิ่ม: ขยายตัวอักษรข้อมูลอากาศและย้ายวันเวลา

ลดความสูงของแถบ WEATHER / LDR / RGB จาก 28 เป็น 20 พิกเซล
ขยับจาก y=30 เป็น y=22 และย้ายแถวเลือกพื้นที่/ชุดข้อมูลขึ้นตามกัน
ใช้ค่าตำแหน่งแถวร่วมกันระหว่างการวาดปุ่มและตรวจสัมผัส
ย้ายวันเดือนปีและเวลาท้องถิ่นไปมุมล่างซ้าย โดยคงปุ่ม REFRESH ด้านขวา

ข้อมูลอุณหภูมิ ความชื้น สภาพอากาศ ฝน เมฆ ลม และ PM2.5 ใช้ Font 4
สูง 26 พิกเซลจากเดิม Font 2 สูง 16 พิกเซล
แยกอุณหภูมิกับความชื้นคนละบรรทัด และลดฟอนต์เฉพาะข้อความที่ยาวเกิน 304 พิกเซล
ตรวจจากตารางความกว้างฟอนต์ที่ติดตั้งแล้ว ข้อความสภาพอากาศที่ยาวที่สุด
`Thunderstorm with hail` กว้าง 256 พิกเซล จึงใส่ฟอนต์ใหญ่ได้ครบทุก weather code
ตรวจช่วงแนวตั้งของข้อมูลหลัก เมทาดาทา และวันเวลาแล้วแยกจากกัน

Build และ Upload ลง `/dev/ttyUSB0` ผ่าน `[SUCCESS]` และ `Hash of data verified.`:

- RAM: 49,956 / 327,680 bytes (15.2%)
- Flash: 1,031,809 / 1,310,720 bytes (78.7%)
- หลัง Upload GET `/api/status` และข้อมูลเชียงใหม่ส่ง HTTP 200
- WiFi, นาฬิกา และ weather worker พร้อมใช้งาน; weather/PM2.5 ไม่ stale
- Serial พบค่า LDR 15 ครั้งในช่วง 8.2 วินาที (ไม่นับข้อความเริ่มต้นสองบรรทัด) และไม่พบข้อความ firmware error ในช่วงตรวจ
- ยังไม่ได้รับการยืนยันภาพและปุ่มตำแหน่งใหม่จากผู้ใช้หลัง Upload รอบนี้

หลักฐานรอบนี้อยู่ใน `.pio/verification/weather-layout-upload.log`,
`.pio/verification/weather-layout-api-live.json` และ `.pio/verification/weather-layout-serial.log`
