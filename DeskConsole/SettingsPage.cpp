#include "SettingsPage.h"

bool SettingsPage::save(const Settings& next) {
  saveFailed = !store.save(next);
  if (saveFailed) {
    Serial.println("[SETTINGS] Flash save failed");
    return false;
  }
  ctx.led.configure(next.rgbEnabled, next.brightness);
  ctx.led.play(StatusLed::Cue::Saved);
  Serial.printf("[SETTINGS] Saved: sleep=%u, after=%u min, RGB=%u, brightness=%u\n",
                static_cast<unsigned>(next.autoSleep), static_cast<unsigned>(next.sleepMinutes()),
                static_cast<unsigned>(next.rgbEnabled), static_cast<unsigned>(next.brightness));
  return true;
}

void SettingsPage::input(Key key, uint32_t us) {
  if (key == Key::Back) {
    if (!editing) { Page::input(key, us); return; }
    editing = saveFailed = false;
  } else if (key == Key::Action) {
    if (editing) {
      if (save(draft)) editing = false;
    } else {
      Settings next = store.current;
      if (row == 0 || row == 2) {
        if (row == 0) next.autoSleep = !next.autoSleep;
        else next.rgbEnabled = !next.rgbEnabled;
        save(next);
      } else {
        if ((row == 1 && !next.autoSleep) || (row == 3 && !next.rgbEnabled)) return;
        draft = next;
        editing = true;
        saveFailed = false;
      }
    }
  } else if (vertical(key)) {
    const int step = key == Key::Up ? -1 : 1;
    if (!editing) row = (row + step + 4) % 4;
    else if (row == 1) draft.sleepIndex = constrain(draft.sleepIndex - step, 0, 3);
    else draft.brightness = constrain(draft.brightness - step, 1, 8);
  }
  draw();
}
