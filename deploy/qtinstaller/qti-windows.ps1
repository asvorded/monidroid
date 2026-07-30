$ErrorActionPreference = "Stop"
 
if (-not (Test-Path "deploy")) {
    Write-Error "Must be run from the project root"
    exit 1
}

# Config
$MD_VERSION     = "0.1.0"
$MD_DEPLOY_DATE = "2026-04-30"

$DEPLOY_DIR = Join-Path $PWD "deploy"

$SERVER_INSTALL_PREFIX          = Join-Path $DEPLOY_DIR "packages\com.monidroid.server\data"
# No need in SERVER_AUTOSTART_INSTALL_PREFIX
$DRIVER_INSTALL_PREFIX          = Join-Path $DEPLOY_DIR "packages\com.monidroid.driver\data"
$CONTROL_INSTALL_PREFIX         = Join-Path $DEPLOY_DIR "packages\com.monidroid.control\data"
$CONTROL_AUTOSTART_INSTALL_PREFIX = Join-Path $DEPLOY_DIR "packages\com.monidroid.control.autostart\data"

$DEPLOY_NAME = "monidroid-windows-$MD_VERSION-setup"

function Write-Message {
    param([string]$Text)
    Write-Host ""
    Write-Host $Text
    Write-Host ""
}
 
# Clean files
Write-Message "Cleaning files before deploy..."
 
Get-ChildItem -Path "$DEPLOY_DIR\packages\*\data\*" -Recurse -Force |
    Remove-Item -Recurse -Force -ErrorAction SilentlyContinue
 
# Set versions
Write-Message "Setting the version in .xml files..."
 
Get-ChildItem -Path "$DEPLOY_DIR\packages" -Recurse -Include "package.xml", "config.xml" -Exclude "build" | ForEach-Object {
    $content = Get-Content $_.FullName -Raw
    $content = $content -replace '(<Version>)[^<]*(</Version>)', "`${1}$MD_VERSION`${2}"
    $content = $content -replace '(<ReleaseDate>)[^<]*(</ReleaseDate>)', "`${1}$MD_DEPLOY_DATE`${2}"
    Set-Content -Path $_.FullName -Value $content -NoNewline
}

# Build and copy server
Write-Message "Building the server..."
 
cmake -DCMAKE_BUILD_TYPE=Release -DMD_DEPLOY=TRUE -DCMAKE_TOOLCHAIN_FILE="$env:VCPKG_ROOT\scripts\buildsystems\vcpkg.cmake" -B "$DEPLOY_DIR\build" --install-prefix $SERVER_INSTALL_PREFIX
cmake --build "$DEPLOY_DIR\build" --config Release --target monidroid-server
cmake --build "$DEPLOY_DIR\build" --config Release --target mdservice
cmake --build "$DEPLOY_DIR\build" --config Release --target mdterm
cmake --install "$DEPLOY_DIR\build" --config Release

$adbPath = (Get-Command adb -ErrorAction Stop).Source
Copy-Item $adbPath -Destination $SERVER_INSTALL_PREFIX

# Build the driver
Write-Message "Building the driver..."

& .\deploy\windows-makecert.ps1

msbuild "-p:Configuration=Release;Platform=x64;OutputPath=x64\Release;SignMode=Off"
New-Item -Type Directory -Path "$DRIVER_INSTALL_PREFIX\driver" -Force
Copy-Item -Path "x64\Release\MonidroidDriver\*" -Destination "$DRIVER_INSTALL_PREFIX\driver" -Recurse -Force
signtool sign /tr http://timestamp.digicert.com /td SHA256 /fd SHA256 /v /f $DEPLOY_DIR\MonidroidPfx.pfx /p 1234 $DRIVER_INSTALL_PREFIX\driver\monidroiddriver.cat

Copy-Item -Path $DEPLOY_DIR\MonidroidCert.cer -Destination $DRIVER_INSTALL_PREFIX

# Build and copy control panel
Write-Message "Building the control panel..."

Push-Location control
try {
    npm install
    npm run app-build
    $controlDest = Join-Path $CONTROL_INSTALL_PREFIX "control"
    New-Item -ItemType Directory -Force -Path $controlDest | Out-Null
    Copy-Item -Path "out\monidroid-control-win32-x64\*" -Destination $controlDest -Recurse -Force
} finally {
    Pop-Location
}

# TODO: Copy-Item "$DEPLOY_DIR\windows\monidroid.lnk" -Destination $CONTROL_INSTALL_PREFIX -ErrorAction SilentlyContinue
Copy-Item "control\static\logo.png"           -Destination $CONTROL_INSTALL_PREFIX

# Make installer
Write-Message "Making an installer..."
 
binarycreator -c "$DEPLOY_DIR\config\config.xml" -p "$DEPLOY_DIR\packages" "$DEPLOY_DIR\$DEPLOY_NAME"