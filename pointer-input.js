// Keep each tap on the virtual GPIO until the emulator has had time to sample it.
function createPointerButtons(buttons, setInput) {
  const queued = [];
  const pointers = new Map();
  let active = null;
  let releasedAt = null;

  function cancel(id) {
    const press = pointers.get(id);
    if (!press) return;
    pointers.delete(id);
    press.cancelled = true;
  }

  buttons.forEach((button, index) => {
    button.addEventListener("pointerdown", event => {
      if (event.button !== 0) return;
      const id = event.pointerId;
      cancel(id);
      button.setPointerCapture(id);
      const press = {index, source: Symbol(), released: false, cancelled: false};
      pointers.set(id, press);
      queued.push(press);
    });
    button.addEventListener("pointerup", event => {
      const press = pointers.get(event.pointerId);
      if (!press) return;
      pointers.delete(event.pointerId);
      press.released = true;
    });
    for (const type of ["pointercancel", "lostpointercapture"]) {
      button.addEventListener(type, event => cancel(event.pointerId));
    }
  });

  addEventListener("blur", () => {
    queued.length = 0;
    pointers.clear();
    if (active) setInput(active.index, active.source, false);
    active = null;
    releasedAt = null;
  });

  return function sample(ready) {
    const now = performance.now();
    if (active) {
      if (active.cancelled || (active.released && active.debounceSampled)) {
        setInput(active.index, active.source, false);
        active = null;
        releasedAt = now;
      } else {
        // Keep the pin down through a full poll after the debounce interval.
        if (now - active.sampledAt >= 40) active.debounceSampled = true;
      }
      return;
    }
    if (!ready) return;
    if (releasedAt !== null) {
      // Let the high level settle in a complete poll before the next down edge.
      if (now - releasedAt >= 40) releasedAt = null;
      return;
    }
    while (queued.length && queued[0].cancelled) queued.shift();
    if (!queued.length) return;
    active = queued.shift();
    active.sampledAt = now;
    setInput(active.index, active.source, true);
  };
}
