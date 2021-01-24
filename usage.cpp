#include <SPI.h>
#include <Arduino.h>
#include <FreeRTOS.h>

#include "VS1003Bmp3.h"
#include "mp3dat.h"

VS1003B::mp3 *m;

void setup(void) {
   Serial.begin(115200);
   m = new VS1003B::mp3(HSPI, 14, 12, 13);
}

void loop() {
  m->soundOut(dat, sizeof(dat));
  delay(1);
}
