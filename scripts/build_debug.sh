#!/bin/bash
# ============================================================
# chipSetter — Debug 编译 (MSYS2 MINGW32)
#
# 用法:  ./scripts/build_debug.sh [clean]
#
# 环境:
#   - Qt 5.15.2 MinGW 32-bit (D:/tool/qt/5.15.2/mingw81_32)
#   - MSYS2 MINGW32 (D:/Code_Languages/C/msys64)
# ============================================================
set -e

# ---- 路径配置 (按需修改) ----
QT_PATH="${QT_PATH:-/d/tool/qt/5.15.2/mingw81_32}"
MINGW_PATH="${MINGW_PATH:-/mingw32}"

# ---- 切换到项目根目录 ----
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
cd "$PROJECT_DIR"

echo "============================================"
echo " chipSetter — 编译 Debug 版本"
echo " 项目目录: $PROJECT_DIR"
echo " Qt 路径:  $QT_PATH"
echo "============================================"

# ---- 清理旧文件 (可选) ----
if [ "$1" = "clean" ]; then
    echo "[1/4] 清理旧的编译产物..."
    rm -rf debug/*.o debug/*.exe debug/moc_*.cpp debug/qrc_*.cpp
    echo "      清理完成"
fi

# ---- QMake ----
echo "[2/4] 运行 qmake..."
"$QT_PATH/bin/qmake" chipSetter.pro CONFIG+=debug CONFIG+=console
echo "      qmake 完成"

# ---- Make ----
echo "[3/4] 编译 (make -j4)..."
make -f Makefile.Debug -j4
echo "      编译完成"

# ---- 输出 ----
echo "[4/4] 编译产物:"
ls -lh debug/chipSetter.exe
echo ""
echo "============================================"
echo " 编译成功!"
echo " 可执行文件: debug/chipSetter.exe"
echo "============================================"
