#include "SettingsPage.h"

// 保存成功才应用新的 RGB 状态。失败提示保留，用户可以重试。
bool SettingsPage::save(const Settings& next) {
  saveFailed = !store.save(next);
  if (saveFailed) {
    Serial.println("[SETTINGS] Flash save failed");
    return false;
  }
  ctx.led.configure(next.rgbEnabled(), next.brightnessLevel());
  ctx.led.play(StatusLed::Cue::Saved);
  Serial.printf("[SETTINGS] Saved: sleep=%u, after=%u min, RGB=%u, brightness=%u%%\n",
                static_cast<unsigned>(next.autoSleep()), static_cast<unsigned>(next.sleepMinutes()),
                static_cast<unsigned>(next.rgbEnabled()), static_cast<unsigned>(next.brightnessPercent()));
  return true;
}

// 数值先在副本中编辑，确认后写入闪存，返回键取消。
void SettingsPage::input(Key key, uint32_t us) {
  if (key == Key::Back) {
    if (!editing) { Page::input(key, us); return; }
    editing = saveFailed = false;
  } else if (key == Key::Action) {
    if (editing) {
      if (save(draft)) editing = false;
    } else {
      if (row == 2 && !store.current.rgbEnabled()) return;
      draft = store.current;
      editing = true;
      saveFailed = false;
    }
  } else if (vertical(key)) {
    const int step = key == Key::Up ? -1 : 1;
    if (!editing) row = (row + step + Settings::rowCount) % Settings::rowCount;
    else if (row == 0) draft.sleepIndex = constrain(draft.sleepIndex - step, 0, Settings::sleepChoices - 1);
    else if (row == 1) draft.brightness = constrain(draft.brightness - step, 0, Settings::rgbChoices - 1);
    else draft.screenRgb = static_cast<Settings::ScreenRgb>((static_cast<int>(draft.screenRgb) + step + 3) % 3);
  }
  draw();
}

void SettingsPage::tick(uint32_t) {
  // 侧键共用动画时间轴。每段结束后再检查最新的编辑状态和边界。
  if (ctx.ui.tabsAnimating()) return;
  const bool bounded = editing && row < 2;
  const int value = row == 0 ? draft.sleepIndex : draft.brightness;
  const int maximum = row == 0 ? Settings::sleepChoices - 1 : Settings::rgbChoices - 1;
  const bool up = !bounded || value < maximum;
  const bool down = !bounded || value > 0;
  if (saveShown != editing) {
    saveShown = editing;
    ctx.ui.replaceTab(1, editing ? "SAVE" : "EDIT");
  } else if (upShown != up) {
    upShown = up;
    ctx.ui.replaceTab(2, up ? "^" : nullptr);
  } else if (downShown != down) {
    downShown = down;
    ctx.ui.replaceTab(3, down ? "v" : nullptr);
  }
}
