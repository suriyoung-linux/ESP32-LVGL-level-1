#ifndef LV_CONF_H
#define LV_CONF_H

// =====================================================================
//  lv_conf.h -- ค่าตั้งของ LVGL 8.3 (Lab 1 ยังไม่ใช้ แต่เตรียมไว้ให้ Lab ที่ใช้จอ)
//  ถ้าไม่มีไฟล์นี้ LVGL จะ build ไม่ผ่าน จึงเก็บไว้ใน Template ตั้งแต่แรก
// =====================================================================

#include <stdint.h>

// สี 16 บิต ตรงกับจอ ILI9341
#define LV_COLOR_DEPTH 16
#define LV_COLOR_16_SWAP 0

// ใช้ heap ปกติของ ESP32 (malloc/free) แทน pool คงที่
// บทเรียนจากคอร์ส: pool คงที่ขนาดเล็กจะหมดกลางทางแล้วโปรแกรมค้างเงียบๆ
// ส่วน pool ใหญ่ก็ไม่พอดีกับหน่วยความจำ DRAM ของ ESP32
#define LV_MEM_CUSTOM 1
#define LV_MEM_CUSTOM_INCLUDE <stdlib.h>
#define LV_MEM_CUSTOM_ALLOC malloc
#define LV_MEM_CUSTOM_FREE free
#define LV_MEM_CUSTOM_REALLOC realloc

// เราเรียก lv_tick_inc() เองใน loop() (ดู Lab ที่ใช้ LVGL)
#define LV_TICK_CUSTOM 0
#define LV_DPI_DEF 130

// ข้อความเป็น UTF-8 (สัญลักษณ์องศา ° ต้องเขียน "\xC2\xB0" ไม่ใช่ %c ค่า 176)
#define LV_TXT_ENC LV_TXT_ENC_UTF8

#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF 1

#define LV_USE_ASSERT_NULL 1
#define LV_USE_ASSERT_MALLOC 1

#define LV_USE_PERF_MONITOR 0
#define LV_USE_MEM_MONITOR 0

// ฟอนต์ที่เปิดใช้ (ยิ่งเปิดมาก flash ยิ่งเต็ม)
#define LV_FONT_MONTSERRAT_10 1
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_28 1
#define LV_FONT_DEFAULT &lv_font_montserrat_14

// widget ที่ใช้ในคอร์ส
#define LV_USE_LABEL 1
#define LV_USE_BTN 1
#define LV_USE_SWITCH 1
#define LV_USE_BAR 1
#define LV_USE_LED 1
#define LV_USE_ARC 1
#define LV_USE_SPINNER 1
#define LV_USE_CHART 1
#define LV_USE_METER 1
#define LV_USE_TABLE 1
#define LV_USE_LINE 1
#define LV_USE_TEXTAREA 1
#define LV_USE_KEYBOARD 1

// ธีมเริ่มต้น (0 = สว่าง, 1 = มืด)
#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 0
#define LV_THEME_DEFAULT_GROW 1
#define LV_THEME_DEFAULT_TRANSITION_TIME 80

#endif
