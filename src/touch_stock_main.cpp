/**
 * Gold-standard CST816S acceptance test for T-Display C5.
 *
 * Mirrors LilyGO examples/touch with hardened init:
 *   single Wire.begin, real TP_RST, FALLING IRQ, sleep-disable 0xFE=0x07, int_edges.
 *
 * Flash:  pio run -e bringup_touch -t upload
 * Monitor: pio device monitor -e bringup_touch
 *
 * Pass: Serial prints gesture / X/Y while touching, and int_edges increases.
 * Fail: INT never moves / int_edges stays 0 on ≥2 boards → CTP missing → RMA.
 */

#include <Arduino.h>
#include <CST816S.h>
#include <Wire.h>

#include "board_config.h"

#ifndef TP_INT
#define TP_INT 27
#endif
#ifndef TP_RST
#define TP_RST 24
#endif

CST816S touch(IIC_SDA_PIN, IIC_SCL_PIN, TP_RST, TP_INT);

volatile uint32_t g_int_edges = 0;
volatile bool g_touch_flag = false;

void IRAM_ATTR onTouchInterrupt() {
  uint32_t n = g_int_edges;
  g_int_edges = n + 1;
  g_touch_flag = true;
}

static void write_sleep_disable(void) {
  // Disable auto-sleep while chip is awake after RST (Espressif / plan: 0xFE=0x07).
  Wire.beginTransmission(CST816S_ADDRESS);
  Wire.write(0xFE);
  Wire.write(0x07);
  Wire.endTransmission();
}

static void i2c_scan(const char *tag) {
  Serial.printf("I2C scan (%s):", tag);
  int n = 0;
  for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
      Serial.printf(" 0x%02X", addr);
      n++;
    }
  }
  if (n == 0) Serial.print(" (none)");
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println();
  Serial.println("=== bringup_touch (CST816S gold test) ===");
  Serial.printf("Pins: SDA=%d SCL=%d INT=%d RST=%d\n", IIC_SDA_PIN, IIC_SCL_PIN, TP_INT,
                TP_RST);

  Wire.begin(IIC_SDA_PIN, IIC_SCL_PIN);
  Wire.setClock(400000);
  i2c_scan("pre-RST");

  pinMode(TP_INT, INPUT_PULLUP);
  Serial.printf("INT level pre-begin: %d\n", digitalRead(TP_INT));

  // Library begin: RST pulse + attachInterrupt(FALLING). Wire already begun.
  touch.begin(FALLING);
  Wire.setClock(400000);
  write_sleep_disable();
  touch.attachUserInterrupt(onTouchInterrupt);

  i2c_scan("post-begin");
  Serial.printf("version=%u info=%u-%u-%u INT=%d\n", (unsigned)touch.data.version,
                (unsigned)touch.data.versionInfo[0], (unsigned)touch.data.versionInfo[1],
                (unsigned)touch.data.versionInfo[2], digitalRead(TP_INT));
  Serial.println("Touch/swipe for 30s — expect gestures and rising int_edges");
}

void loop() {
  static uint32_t last_hb = 0;
  const uint32_t now = millis();

  if (g_touch_flag) {
    g_touch_flag = false;
    if (touch.available()) {
      Serial.printf("%s\t pts=%u evt=%u x=%d y=%d edges=%lu\n", touch.gesture().c_str(),
                    (unsigned)touch.data.points, (unsigned)touch.data.event, touch.data.x,
                    touch.data.y, (unsigned long)g_int_edges);
      write_sleep_disable();
    }
  }

  if ((now - last_hb) >= 2000) {
    last_hb = now;
    Wire.beginTransmission(CST816S_ADDRESS);
    const bool ack = (Wire.endTransmission() == 0);
    Serial.printf("hb INT=%d probe0x15=%d int_edges=%lu\n", digitalRead(TP_INT), (int)ack,
                  (unsigned long)g_int_edges);
  }
}
