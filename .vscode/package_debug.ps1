# ============================================
#  chipSetter debug 部署包打包脚本
#  将 debug exe + gdbserver + Qt DLL + MinGW DLL 打包到 debug_deploy/
# ============================================

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot\..

$QT_DIR         = "D:\tool\qt\5.15.2\mingw81_32"
$MSYS2_MINGW32  = "D:\Code_Languages\C\msys64\mingw32"
$PROJECT_DIR    = "D:\my_information\qtProject\chipSetter"
$DEPLOY_DIR     = "$PROJECT_DIR\debug_deploy"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  chipSetter Debug Package Tool" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# ---- Step 1: 确认 debug exe 存在 ----
$DEBUG_EXE = "$PROJECT_DIR\debug\chipSetter.exe"
if (-not (Test-Path $DEBUG_EXE)) {
    Write-Host "[ERROR] Debug exe not found: $DEBUG_EXE" -ForegroundColor Red
    exit 1
}
Write-Host "[OK] Found debug exe: $DEBUG_EXE" -ForegroundColor Green
$exeSize = [math]::Round((Get-Item $DEBUG_EXE).Length / 1MB, 1)
Write-Host "     Size: $exeSize MB" -ForegroundColor Gray

# ---- Step 2: 创建部署目录 ----
if (-not (Test-Path $DEPLOY_DIR)) {
    New-Item -ItemType Directory -Path $DEPLOY_DIR -Force | Out-Null
}
Remove-Item "$DEPLOY_DIR\*" -Recurse -Force -ErrorAction SilentlyContinue

# ---- Step 3: 复制 debug exe ----
Write-Host ""
Write-Host "[1/5] Copying debug chipSetter.exe..."
Copy-Item $DEBUG_EXE $DEPLOY_DIR\
Write-Host "      Done." -ForegroundColor Green

# ---- Step 4: windeployqt (Qt DLLs) ----
Write-Host "[2/5] Running windeployqt..."
$windeployqt = "$QT_DIR\bin\windeployqt.exe"
if (Test-Path $windeployqt) {
    $env:PATH = "$QT_DIR\bin;$env:PATH"
    & $windeployqt "$DEPLOY_DIR\chipSetter.exe" --no-translations --no-compiler-runtime 2>&1 | Out-Null
    Write-Host "      Qt DLLs deployed." -ForegroundColor Green
} else {
    Write-Host "      [WARN] windeployqt not found at $windeployqt" -ForegroundColor Yellow
}

# ---- Step 5: 复制 gdbserver 和 MinGW 依赖 DLL ----
Write-Host "[3/5] Copying gdbserver + MinGW runtime DLLs..."
$DLL_LIST = @(
    "gdbserver.exe",
    "libgcc_s_dw2-1.dll",
    "libstdc++-6.dll",
    "libwinpthread-1.dll"
)

foreach ($dll in $DLL_LIST) {
    $src = Join-Path $MSYS2_MINGW32 "bin\$dll"
    if (Test-Path $src) {
        Copy-Item $src $DEPLOY_DIR\
        Write-Host "      Copied: $dll" -ForegroundColor Gray
    } else {
        # Try Qt dir as fallback
        $src2 = Join-Path $QT_DIR "bin\$dll"
        if (Test-Path $src2) {
            Copy-Item $src2 $DEPLOY_DIR\
            Write-Host "      Copied (Qt): $dll" -ForegroundColor Gray
        } else {
            Write-Host "      SKIP: $dll" -ForegroundColor Yellow
        }
    }
}
Write-Host "      Done." -ForegroundColor Green

# ---- Step 6: 复制 googol GTS 运动控制库 ----
Write-Host "[4/6] Copying googol GTS library..."
$GOOGOL_DIR = "$PROJECT_DIR\googol"
if (Test-Path $GOOGOL_DIR) {
    Copy-Item "$GOOGOL_DIR\*" $DEPLOY_DIR\ -Force
    Write-Host "      gts.dll, gts.lib copied." -ForegroundColor Green
} else {
    Write-Host "      [WARN] googol dir not found" -ForegroundColor Yellow
}

# ---- Step 7: Qt 平台插件 (platforms/qwindows.dll) ----
Write-Host "[5/7] Copying Qt platform plugin..."
$srcPlugins = "$QT_DIR\plugins"
$dstPlugins = "$DEPLOY_DIR"
if (Test-Path $srcPlugins) {
    Copy-Item "$srcPlugins\*" $dstPlugins -Recurse -Force
    Write-Host "      Plugins copied." -ForegroundColor Green
}

# ---- Step 7: 打包统计 ----
Write-Host "[6/7] Package summary..." -ForegroundColor Cyan
$fileCount = (Get-ChildItem -Recurse $DEPLOY_DIR | Where-Object { -not $_.PSIsContainer }).Count
$totalSize = [math]::Round((Get-ChildItem -Recurse $DEPLOY_DIR | Measure-Object -Property Length -Sum).Sum / 1MB, 1)
Write-Host "      Files: $fileCount, Total: $totalSize MB" -ForegroundColor Green
Write-Host "      Output: $DEPLOY_DIR"

# List Qt DLLs for verification
Write-Host ""
$qtDlls = Get-ChildItem "$DEPLOY_DIR\*.dll" | Select -ExpandProperty Name
Write-Host "      Qt DLLs: $($qtDlls -join ', ')" -ForegroundColor Gray

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Package complete!" -ForegroundColor Green
Write-Host "========================================" -ForegroundColor Cyan
