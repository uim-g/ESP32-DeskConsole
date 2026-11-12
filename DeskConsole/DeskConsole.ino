#include "DeskConsoleApp.h"

namespace {
DeskConsoleApp app;
}

void setup() { app.begin(); }

void loop() {
  app.tick();
  delay(1);
}
