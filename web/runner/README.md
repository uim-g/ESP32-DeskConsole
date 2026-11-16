# DeskConsole browser builds

The version selector and simulator downloads are published through GitHub Pages and the corresponding firmware releases. v1.0.0 uses the existing JavaScript port. Starting with v1.1.0, each build exports the selected Git tag and compiles its original sketch and display implementation to WebAssembly with Adafruit GFX.

The browser adapter supplies the 320×240 RGB565 framebuffer, four GPIO inputs, local storage for Preferences, Open-Meteo weather, and a visual sleep/LED simulation. Network and device diagnostics and BLE scan results use example values. Each version has its own browser storage namespace. Browser scheduling and display latency affect F1 times.

Builds with browser MPU6050 support show an optional panel below the simulated screen. Open **Apps → Motion** and rotate the virtual module with the X/Y/Z sliders. The simulator sends gravity and angular-rate samples through the same Motion code as the firmware. Uncheck **Sensor connected** to test the offline state. The sliders control a virtual pose and do not access the phone's sensors.

For historical tags v3.1.0 through v3.3.1, the build script adds the sensor adapter to the exported browser-only source. The tagged firmware and original published ZIPs remain available.

To build the WebAssembly versions locally, use PowerShell 7.4+, Git, Emscripten 4.0.19, and the Adafruit GFX 1.13.0 source directory. Run the following with the Emscripten environment loaded.

```powershell
./web/runner/build.ps1 -Compiler em++ -GfxLibrary <Adafruit-GFX-directory> -Output <output-directory>
```

Use `-Versions v1.5.0` for a single tag. Use `-WorkingTree` to build the current firmware into the output directory under `dev/`. The current firmware shares its page and animation code across both builds. Each output version directory can be served as static files or zipped for a release. The selector is `web/runner/versions.html` and is published as the site's root `index.html`.
