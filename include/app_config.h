#pragma once

// =====================================================================
//  app_config.h -- ค่าที่ "ปรับได้" ของแต่ละ Lab
//
//  board_config.h = ข้อเท็จจริงของฮาร์ดแวร์ (ไม่แก้)
//  app_config.h   = ค่าที่เราอยากลองปรับ เช่น ความเร็ว ข้อความ (แก้ได้เลย)
// =====================================================================

#include <Arduino.h>

namespace app {

constexpr uint32_t SERIAL_BAUD = 115200;  // ต้องตรงกับ monitor_speed ใน platformio.ini

// ค่าของแบบฝึกหัดกะพริบ LED เดิม (โปรแกรมควบคุมสีด้วย LDR ไม่ได้ใช้สองค่านี้)
// เวลาที่ LED แต่ละสีค้างอยู่ (มิลลิวินาที)
constexpr uint32_t LED_STEP_MS = 2000;
// ช่วงดับระหว่างสี เพื่อให้เห็นการกระพริบชัดเจน
constexpr uint32_t LED_OFF_MS = 200;

// อ่านค่า LDR ทุก 500 มิลลิวินาที (0.5 วินาที)
constexpr uint32_t LDR_READ_INTERVAL_MS = 500;

// ตรวจสถานะ WiFi โดยไม่หยุดอ่าน LDR และลองเชื่อมต่อใหม่ทุก 15 วินาทีเมื่อหลุด
constexpr uint32_t WIFI_CHECK_INTERVAL_MS = 250;
constexpr uint32_t WIFI_RETRY_INTERVAL_MS = 15000;

// เกณฑ์ค่าดิบ ADC สำหรับเลือกสี LED
// < 500 = น้ำเงิน, 500 ถึง 1000 (รวมขอบ) = เขียว, > 1000 = แดง
constexpr uint16_t LDR_LOW_THRESHOLD = 500;
constexpr uint16_t LDR_HIGH_THRESHOLD = 1000;

// แรงดันจ่ายวงจร LDR: ปรับตามแรงดัน 3.3 V ที่วัดจริงเพื่อปรับปรุงค่าประมาณ R
constexpr float LDR_SUPPLY_VOLTAGE = 3.3f;
// ADC ที่ attenuation 11 dB: ไม่ประมาณ R จากแรงดันต่ำกว่า 0.15 V
constexpr float LDR_MIN_VALID_VOLTAGE = 0.15f;
// แรงดันอ้างอิงภายใน ADC ใช้สำรองเมื่อชิปไม่มีค่า calibration ใน eFuse
// เป็นคนละค่ากับแรงดันจ่ายวงจร 3.3 V ด้านบน
constexpr uint32_t ADC_DEFAULT_VREF_MV = 1100;

constexpr char HELLO_TEXT[] = "Hello World";

}  // namespace app
