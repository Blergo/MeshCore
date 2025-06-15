#include "WaveshareBoard.h"

#include <Arduino.h>
#include <Wire.h>

void WaveshareBoard::begin() {
  // for future use, sub-classes SHOULD call this from their begin()
  startup_reason = BD_STARTUP_NORMAL;

#ifdef PIN_LED_BUILTIN
  pinMode(PIN_LED_BUILTIN, OUTPUT);
#endif

#ifdef PIN_VBAT_READ
  pinMode(PIN_VBAT_READ, INPUT);
#endif

#if defined(PIN_BOARD_SDA) && defined(PIN_BOARD_SCL)
  Wire.setPins(PIN_BOARD_SDA, PIN_BOARD_SCL);
#endif

  Wire.begin();

  delay(10); // give sx1262 some time to power up
}

bool WaveshareBoard::startOTAUpdate(const char *id, char reply[]) {
  return false;
}
