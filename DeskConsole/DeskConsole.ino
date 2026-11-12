// Arduino 入口。setup 只执行一次；loop 把工作交给应用对象。末尾的 1 ms 延时让系统任务有机会运行，页面动画由自己的计时器推进。

#include "DeskConsoleApp.h"

namespace {
DeskConsoleApp app;
}

void setup() { app.begin(); }

void loop() {
  app.tick();
  delay(1);
}
