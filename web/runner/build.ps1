param(
  [Parameter(Mandatory)] [string] $Compiler,
  [Parameter(Mandatory)] [string] $GfxLibrary,
  [Parameter(Mandatory)] [string] $Output,
  [string[]] $Versions = @('v1.1.0','v1.2.0','v1.3.0','v1.3.1','v1.4.0','v1.5.0','v2.0.0','v2.1.0','v2.2.0','v2.2.1','v3.0.0',
                          'v3.1.0','v3.1.1','v3.1.2','v3.1.3','v3.2.0','v3.2.1','v3.3.0','v3.3.1'),
  [switch] $WorkingTree,
  [string] $PreviewVersion = "dev"
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path "$PSScriptRoot/../..").Path
$GfxLibrary = (Resolve-Path $GfxLibrary).Path
$Output = [IO.Path]::GetFullPath($Output)
New-Item -ItemType Directory -Force $Output | Out-Null
if ($WorkingTree) {
  if ($PreviewVersion -notmatch '^(dev|v\d+\.\d+\.\d+)$') { throw 'Invalid preview version.' }
  if ($PSBoundParameters.ContainsKey('Versions')) { throw 'Select either -WorkingTree or -Versions.' }
  $Versions = @($PreviewVersion)
}
foreach ($version in $Versions) {
  if (!$WorkingTree -and $version -notmatch '^v\d+\.\d+\.\d+$') { throw "Invalid version: $version" }
  $build = Join-Path $Output ".build-$version"
  $site = Join-Path $Output $version
  New-Item -ItemType Directory -Force $build,$site | Out-Null
  if ($WorkingTree) {
    $source = Join-Path $repo 'DeskConsole'
  } else {
    git -C $repo archive --format=tar "--output=$build/source.tar" $version DeskConsole
    if ($LASTEXITCODE) { throw "Could not export $version" }
    [System.Formats.Tar.TarFile]::ExtractToDirectory("$build/source.tar",$build,$true)
    $source = Join-Path $build 'DeskConsole'
  }
  $firmware = Join-Path $build 'firmware.cpp'
  Copy-Item "$source/DeskConsole.ino" $firmware
  if (!(Test-Path (Join-Path $source 'DeskConsoleApp.cpp'))) {
    $sketch = [IO.File]::ReadAllText($firmware)
    $marker = '  uint32_t pressedUs = 0;'
    if (!$sketch.Contains($marker)) { throw "Input polling point not found in $version" }
    $hook = "  extern void setBrowserInputReady(bool);`n  setBrowserInputReady(true);`n"
    $sketch = $sketch.Replace($marker,$hook+$marker)
    $sketch = $sketch.Replace('  if (discardWakePress) {', "  if (discardWakePress) {`n    extern void setBrowserInputReady(bool);`n    setBrowserInputReady(false);")
    [IO.File]::WriteAllText($firmware,$sketch)
  }
  $interfacePath = "$source/Interface.cpp"
  $interface = [IO.File]::ReadAllText($interfacePath)
  if (!$WorkingTree -and $interface.Contains('void Interface::animateTabs(')) {
    # 旧版依赖 SPI 传输耗时，浏览器构建时给这些历史循环增加帧间隔。
    $launcherEnd = '(?m)^(    })(\r?\n  }\r?\n  lastSelection = selected;)'
    if (![regex]::IsMatch($interface,$launcherEnd)) { throw "Launcher animation not found in $version" }
    $interface = [regex]::Replace($interface,$launcherEnd,"      if (step < steps) delay(16);`n"+'$1$2')
    $settingsEnd = '(?m)^(    })(\r?\n  }\r?\n\r?\n  const bool rollingSleep)'
    $interface = [regex]::Replace($interface,$settingsEnd,"      if (frame < 6) delay(16);`n"+'$1$2')
    $interfacePath = Join-Path $build 'Interface.cpp'
    [IO.File]::WriteAllText($interfacePath,$interface)
  }
  $appPath = Join-Path $source 'DeskConsoleApp.cpp'
  $browserApp = $null
  if (!$WorkingTree -and (Test-Path $appPath)) {
    $app = [IO.File]::ReadAllText($appPath)
    if (!$app.Contains('Platform::setInputReady(')) {
      # Historical v2 tags do not report transition readiness to the browser.
      $marker = '  uint32_t pressedUs = 0;'
      if (!$app.Contains($marker)) { throw "Input polling point not found in $version" }
      $browserApp = Join-Path $build 'DeskConsoleApp.cpp'
      $hook = "  extern void setBrowserInputReady(bool);`n  setBrowserInputReady(transition == Transition::None);`n"
      $app = $app.Replace('  if (discardWakePress) {', "  if (discardWakePress) {`n    extern void setBrowserInputReady(bool);`n    setBrowserInputReady(false);")
      [IO.File]::WriteAllText($browserApp,$app.Replace($marker,$hook+$marker))
    }
  }
  $motionSource = Join-Path $source 'MotionPage.cpp'
  $hasBrowserMotion = $false
  if (Test-Path $motionSource) {
    $motion = [IO.File]::ReadAllText($motionSource)
    $hasBrowserMotion = $motion.Contains('readBrowserMotion(')
    if (!$WorkingTree -and !$hasBrowserMotion) {
      # 只在导出的历史网页源码里替换空传感器，保留 Git 标签与硬件代码。
      $offline = '(?m)(^#ifdef __EMSCRIPTEN__\r?\n)  return false;[^\r\n]*(\r?\n#else)'
      if (![regex]::IsMatch($motion, $offline)) { throw "Motion simulator hook not found in $version" }
      $motion = [regex]::Replace($motion, $offline,
        ('$1' + "  const bool connected = readBrowserMotion(reading);`n  address = connected ? 0x68 : 0;`n  return connected;" + '$2'))
      [IO.File]::WriteAllText($motionSource,
        "#include <stdint.h>`nextern `"C`" bool readBrowserMotion(int16_t* reading);`n" + $motion)
      $hasBrowserMotion = $true
    }
  }
  $modules = Get-ChildItem -LiteralPath $source -Filter '*.cpp' | Where-Object {
    $_.Name -notin @('Interface.cpp','Weather.cpp','Bluetooth.cpp','firmware.cpp') -and
      (!$browserApp -or $_.Name -ne 'DeskConsoleApp.cpp')
  } | ForEach-Object FullName
  Write-Host "Building $version"
  & $Compiler -std=c++17 -O2 -DARDUINO=10819 "-I$PSScriptRoot" "-I$GfxLibrary" "-I$source" `
    $firmware $interfacePath $browserApp $modules "$PSScriptRoot/runner.cpp" "$GfxLibrary/Adafruit_GFX.cpp" `
    --pre-js "$PSScriptRoot/../pointer-input.js" --pre-js "$PSScriptRoot/browser.js" -sASYNCIFY -sASYNCIFY_STACK_SIZE=65536 -sSTACK_SIZE=131072 `
    -sSINGLE_FILE -sENVIRONMENT=web -o "$site/firmware.js"
  if ($LASTEXITCODE) { throw "Could not compile $version" }
  $html = (Get-Content -Raw "$PSScriptRoot/index-template.html").Replace('VERSION',$version)
  if (!$hasBrowserMotion) {
    $html = $html -replace '(?s)<!-- MOTION_START -->.*?<!-- MOTION_END -->\r?\n',''
  }
  [IO.File]::WriteAllText("$site/index.html",$html)
  Copy-Item "$PSScriptRoot/style.css" "$site/style.css"
  Copy-Item "$source/Nunito-OFL.txt" "$site/Nunito-OFL.txt"
  Copy-Item "$GfxLibrary/license.txt" "$site/Adafruit-GFX-license.txt"
  Copy-Item "$repo/LICENSE" "$site/LICENSE"
}
