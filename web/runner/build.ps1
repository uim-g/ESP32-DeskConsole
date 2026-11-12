param(
  [Parameter(Mandatory)] [string] $Compiler,
  [Parameter(Mandatory)] [string] $GfxLibrary,
  [Parameter(Mandatory)] [string] $Output,
  [string[]] $Versions = @('v1.1.0','v1.2.0','v1.3.0','v1.3.1','v1.4.0','v1.5.0')
)
$ErrorActionPreference = 'Stop'
$repo = (Resolve-Path "$PSScriptRoot/../..").Path
$GfxLibrary = (Resolve-Path $GfxLibrary).Path
$Output = [IO.Path]::GetFullPath($Output)
New-Item -ItemType Directory -Force $Output | Out-Null
foreach ($version in $Versions) {
  if ($version -notmatch '^v\d+\.\d+\.\d+$') { throw "Invalid version: $version" }
  $build = Join-Path $Output ".build-$version"
  $site = Join-Path $Output $version
  New-Item -ItemType Directory -Force $build,$site | Out-Null
  git -C $repo archive --format=tar "--output=$build/source.tar" $version DeskConsole
  if ($LASTEXITCODE) { throw "Could not export $version" }
  [System.Formats.Tar.TarFile]::ExtractToDirectory("$build/source.tar",$build,$true)
  $source = Join-Path $build 'DeskConsole'
  Copy-Item "$source/DeskConsole.ino" "$source/firmware.cpp"
  # Firmware SPI writes pace these loops on-device. Present each frame in the browser.
  $interfacePath = "$source/Interface.cpp"
  $interface = [IO.File]::ReadAllText($interfacePath)
  $launcherEnd = '(?m)^(    })(\r?\n  }\r?\n  lastSelection = selected;)'
  if (![regex]::IsMatch($interface,$launcherEnd)) { throw "Launcher animation not found in $version" }
  $interface = [regex]::Replace($interface,$launcherEnd,"      if (step < steps) delay(16);`n"+'$1$2')
  $settingsEnd = '(?m)^(    })(\r?\n  }\r?\n\r?\n  const bool rollingSleep)'
  $interface = [regex]::Replace($interface,$settingsEnd,"      if (frame < 6) delay(16);`n"+'$1$2')
  [IO.File]::WriteAllText($interfacePath,$interface)
  Write-Host "Building $version"
  & $Compiler -std=c++17 -O2 -DARDUINO=10819 "-I$PSScriptRoot" "-I$GfxLibrary" "-I$source" `
    "$source/firmware.cpp" "$source/Interface.cpp" "$PSScriptRoot/runner.cpp" "$GfxLibrary/Adafruit_GFX.cpp" `
    --pre-js "$PSScriptRoot/browser.js" -sASYNCIFY -sASYNCIFY_STACK_SIZE=65536 -sSTACK_SIZE=131072 `
    -sSINGLE_FILE -sENVIRONMENT=web -o "$site/firmware.js"
  if ($LASTEXITCODE) { throw "Could not compile $version" }
  $html = (Get-Content -Raw "$PSScriptRoot/index-template.html").Replace('VERSION',$version)
  [IO.File]::WriteAllText("$site/index.html",$html)
  Copy-Item "$PSScriptRoot/style.css" "$site/style.css"
  Copy-Item "$source/Nunito-OFL.txt" "$site/Nunito-OFL.txt"
  Copy-Item "$GfxLibrary/license.txt" "$site/Adafruit-GFX-license.txt"
  Copy-Item "$repo/LICENSE" "$site/LICENSE"
}
