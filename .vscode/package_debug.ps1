# ============================================
#  chipSetter debug 部署包打包脚本
#  将 debug exe + gdbserver + 依赖 DLL 打包到 debug_deploy/
# ============================================

$ErrorActionPreference = "Stop"
Set-Location $PSScriptRoot\..

$MSYS2_MINGW32 = "D:\Code_Languages\C\msys64\mingw32"
$PROJECT_DIR  = "D:\my_information\qtProject\chipSetter"
$DEPLOY_DIR   = "$PROJECT_DIR\debug_deploy"

Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  chipSetter Debug Package Tool" -ForegroundColor Cyan
Write-Host "========================================" -ForegroundColor Cyan
Write-Host ""

# ---- Step 1: 确认 debug exe 存在 ----
$DEBUG_EXE = "$PROJECT_DIR\debug\chipSetter.exe"
if (-not (Test-Path $DEBUG_EXE)) {
    Write-Host "[ERROR] Debug exe not found: $DEBUG_EXE" -ForegroundColor Red
    Write-Host "Please build debug version first." -ForegroundColor Yellow
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
Write-Host "[1/3] Copying debug chipSetter.exe..."
Copy-Item $DEBUG_EXE $DEPLOY_DIR\
Write-Host "      Done." -ForegroundColor Green

# ---- Step 4: 复制 gdbserver 和 MinGW 依赖 DLL ----
Write-Host "[2/3] Copying gdbserver + MinGW runtime DLLs..."
$DLL_LIST = @(
    "gdbserver.exe",
    "libgcc_s_dw2-1.dll",
    "libstdc++-6.dll",
    "libwinpthread-1.dll",
    "libintl-8.dll",
    "libiconv-2.dll",
    "zlib1.dll"
)

foreach ($dll in $DLL_LIST) {
    $src = Join-Path $MSYS2_MINGW32 "bin\$dll"
    if (Test-Path $src) {
        Copy-Item $src $DEPLOY_DIR\
        Write-Host "      Copied: $dll" -ForegroundColor Gray
    } else {
        Write-Host "      SKIP (not found): $dll" -ForegroundColor Yellow
    }
}
Write-Host "      Done." -ForegroundColor Green

# ---- Step 5: 打包统计 ----
Write-Host "[3/3] Package summary..." -ForegroundColor Cyan
$fileCount = (Get-ChildItem $DEPLOY_DIR).Count
$totalSize = [math]::Round((Get-ChildItem $DEPLOY_DIR | Measure-Object -Property Length -Sum).Sum / 1MB, 1)
Write-Host "      Files: $fileCount, Total: $totalSize MB" -ForegroundColor Green
Write-Host "      Output: $DEPLOY_DIR" -ForegroundColor Green

Write-Host ""
Write-Host "========================================" -ForegroundColor Cyan
Write-Host "  Package complete!" -ForegroundColor Green
Write-Host "  Next: start gdbserver on target, then F5 in VSCode"
Write-Host "========================================" -ForegroundColor Cyan
