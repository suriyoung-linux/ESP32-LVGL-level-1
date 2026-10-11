#pragma once

// =====================================================================
//  board_config.h -- ข้อเท็จจริงของฮาร์ดแวร์บอร์ด ESP32-2432S028 (CYD)
//
//  ไฟล์นี้รวม "ทุก pin" ไว้ที่เดียว ห้ามแก้ เว้นแต่ฮาร์ดแวร์จริงต่างไป
//  (ของที่ปรับได้ตามแต่ละ Lab ให้ไปแก้ใน app_config.h แทน)
//  การใช้งานและข้อจำกัดของระบบนี้: ดู README.md และ WORKSHEET.md
// =====================================================================

#include <Arduino.h>

namespace cyd {

// ---- จอ TFT (hardware SPI: HSPI) ----
constexpr uint8_t TFT_PIN_MISO = 12;
constexpr uint8_t TFT_PIN_MOSI = 13;
constexpr uint8_t TFT_PIN_SCLK = 14;
constexpr uint8_t TFT_PIN_CS = 15;
constexpr uint8_t TFT_PIN_DC = 2;
constexpr uint8_t TFT_PIN_BACKLIGHT = 21;  // ห้ามใช้ต่อเซนเซอร์ (ดูหมายเหตุด้านล่าง)

// ---- จอสัมผัส XPT2046 (ขา SPI คนละชุดกับจอ; ใช้ VSPI ใน template นี้) ----
constexpr uint8_t TOUCH_PIN_CLK = 25;
constexpr uint8_t TOUCH_PIN_CS = 33;
constexpr uint8_t TOUCH_PIN_MOSI = 32;
constexpr uint8_t TOUCH_PIN_MISO = 39;
constexpr uint8_t TOUCH_PIN_IRQ = 36;

// ---- RGB LED บนบอร์ด (สามสีในดวงเดียว) ----
//
//   สี          ขา GPIO     ประเภทสัญญาณ
//   แดง   (R)   GPIO4       เปิด/ปิดด้วย digitalWrite หรือ PWM (LEDC) ก็ได้
//   เขียว (G)   GPIO16      เปิด/ปิดด้วย digitalWrite หรือ PWM (LEDC) ก็ได้
//   น้ำเงิน (B) GPIO17      เปิด/ปิดด้วย digitalWrite หรือ PWM (LEDC) ก็ได้
//
// หมายเหตุ: ใช้การกำหนดสีจาก template ต้นทาง: แดง=4, เขียว=16, น้ำเงิน=17
//           ตรวจสีบนบอร์ดของคุณเทียบกับข้อความ Serial ก่อนใช้ใน Lab ถัดไป
//
// ทั้งสามสีเป็นแบบ active-LOW: ส่ง LOW = LED ติด, ส่ง HIGH = LED ดับ
//   -> ถ้าใช้ PWM ต้อง "กลับค่า duty" ก่อนเขียน คือเขียน (ค่าสูงสุด - ค่าที่อยากให้สว่าง)
//      ตัวอย่าง: Lab 5 ใช้ PWM ที่ LED เขียว (GPIO16), Lab 6 ใช้ PWM ที่ LED น้ำเงิน (GPIO17)
constexpr uint8_t LED_PIN_R = 4;   // LED แดง
constexpr uint8_t LED_PIN_G = 16;  // LED เขียว
constexpr uint8_t LED_PIN_B = 17;  // LED น้ำเงิน
// เปลี่ยนเป็น false เฉพาะเมื่อยืนยันว่าฮาร์ดแวร์ของคุณเป็น active-HIGH
constexpr bool LED_ACTIVE_LOW = true;

// ---- อื่นๆ บนบอร์ด ----
constexpr uint8_t AUDIO_PIN = 26;  // ลำ โพง (DAC)
constexpr uint8_t LDR_PIN = 34;    // ตัววัดแสง input-only, ADC1_CH6 ใช้ analogRead() ได้เลย
// ตามแผนผัง ESP32-2432S028 V0.1: R15 ต่อจาก 3.3 V มายัง ADC
// ส่วน R19 และ LDR (R21) ต่อขนานจาก ADC ลง GND
// https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display/blob/main/OriginalDocumentation/5-Schematic/ESP32-2432S028-MCU.jpg
constexpr float LDR_PULLUP_OHMS = 1000000.0f;    // R15 = 1 MOhm
constexpr float LDR_PULLDOWN_OHMS = 1000000.0f;  // R19 = 1 MOhm ขนานกับ LDR

// ---- microSD (ขา default ของ VSPI) ----
// ESP32 รุ่นนี้มี hardware SPI สำหรับอุปกรณ์ทั่วไป 2 ชุด: HSPI และ VSPI
// Touch ใช้ VSPI ด้วย แต่เดินสายคนละขากับ SD จึงเริ่ม SPIClass(VSPI) สองตัว
// ด้วย pin ต่างกันแล้วใช้พร้อมกันไม่ได้ ต้องจัดการสลับขา/บัสอย่างถูกต้อง
// หรือใช้ไลบรารี Touch ที่รองรับ software SPI เมื่อต้องใช้จอ + Touch + SD
constexpr uint8_t SD_PIN_CS = 5;
constexpr uint8_t SD_PIN_MOSI = 23;
constexpr uint8_t SD_PIN_CLK = 18;
constexpr uint8_t SD_PIN_MISO = 19;

// ---- ขา expansion ที่ปลอดภัยสำหรับต่อเซนเซอร์เพิ่ม ----
// ทดสอบกับฮาร์ดแวร์จริงแล้ว: GPIO27 (SDA) + GPIO22 (SCL) ใช้ I2C ได้ปกติ
// และ GPIO27 ใช้ต่อเซนเซอร์ตัวเดียว (เช่น DHT11) ได้โดยไม่รบกวนจอ
constexpr uint8_t I2C_PIN_SDA = 27;
constexpr uint8_t I2C_PIN_SCL = 22;
constexpr uint8_t SAFE_SENSOR_PIN = 27;

// !! ห้ามใช้ GPIO21 ต่อเซนเซอร์เด็ดขาด เพราะเป็นขา backlight ของจอ
// ทดสอบแล้วว่าทำให้จอกระพริบตามการสื่อสาร ทั้ง I2C และ DHT11
// !! GPIO34/35/36/39 เป็น input-only: อ่านค่าได้อย่างเดียว สั่ง output ไม่ได้

// ---- จอ ----
constexpr uint16_t DISPLAY_WIDTH = 320;
constexpr uint16_t DISPLAY_HEIGHT = 240;
constexpr uint8_t DISPLAY_ROTATION = 1;  // 1 = แนวนอน

// ---- Touch calibration ----
// ค่าเหล่านี้ได้จากโปรแกรม calibration ของ "บอร์ดแต่ละตัว" (ใช้ร่วมกันข้ามบอร์ดไม่ได้)
// ค่าเริ่มต้นด้านล่างเป็นค่าประมาณ และ TOUCH_CALIBRATED = false จนกว่าจะวัดจริง
constexpr bool TOUCH_CALIBRATED = false;
constexpr uint16_t TOUCH_RAW_X_MIN = 480;
constexpr uint16_t TOUCH_RAW_X_MAX = 3600;
constexpr uint16_t TOUCH_RAW_Y_MIN = 600;
constexpr uint16_t TOUCH_RAW_Y_MAX = 3530;
constexpr bool TOUCH_SWAP_XY = false;
constexpr bool TOUCH_INVERT_X = false;
constexpr bool TOUCH_INVERT_Y = false;
constexpr uint8_t TOUCH_SPI_BUS = VSPI;  // ใช้ hardware bus เดียวกับ SD; ดูหมายเหตุ microSD
constexpr uint32_t TOUCH_SPI_FREQUENCY = 2500000;

// ---------------------------------------------------------------------
// ฟังก์ชันช่วย (inline = วางในไฟล์ header ได้เลย)
// ---------------------------------------------------------------------

// เปิดไฟหลังจอ (ใช้ตอนเริ่มใช้จอ ใน Lab ถัดไป)
inline void enableBacklight()
{
    pinMode(TFT_PIN_BACKLIGHT, OUTPUT);
    digitalWrite(TFT_PIN_BACKLIGHT, HIGH);
}

// ตั้งค่า pin ของ RGB LED เป็น output และดับไฟทั้งหมดตั้งแต่เริ่ม
inline void ledInit()
{
    pinMode(LED_PIN_R, OUTPUT);
    pinMode(LED_PIN_G, OUTPUT);
    pinMode(LED_PIN_B, OUTPUT);
    digitalWrite(LED_PIN_R, LED_ACTIVE_LOW ? HIGH : LOW);
    digitalWrite(LED_PIN_G, LED_ACTIVE_LOW ? HIGH : LOW);
    digitalWrite(LED_PIN_B, LED_ACTIVE_LOW ? HIGH : LOW);
}

// สั่งสี RGB LED: true = ติด, false = ดับ (ไม่ต้องสนใจว่าบอร์ดเป็น active-LOW หรือ HIGH)
inline void ledSet(bool red, bool green, bool blue)
{
    digitalWrite(LED_PIN_R, (red == LED_ACTIVE_LOW) ? LOW : HIGH);
    digitalWrite(LED_PIN_G, (green == LED_ACTIVE_LOW) ? LOW : HIGH);
    digitalWrite(LED_PIN_B, (blue == LED_ACTIVE_LOW) ? LOW : HIGH);
}

}  // namespace cyd
