# Qt MinGW 远程调试 (gdbserver) 完整排坑指南

## 架构

```
VS Code (开发机 192.168.1.100)
  └─ gdb (MSYS2 MinGW 32-bit)
       └─ TCP :1234 ─── gdbserver (GNC 工控机 192.168.1.2)
                            └─ chipSetter.exe (GUI 进程)
```

- **开发机**: Windows 11, Qt 5.15.2 + MinGW 8.1.0 32-bit, MSYS2
- **目标机**: Windows 10, 网线直连同一网段
- **部署方式**: robocopy -> SMB 共享 `\\192.168.1.2\share\chipSetter`
- **远程管理**: WinRM + schtasks

---

## 踩过的坑 & 解决方案

### 1. robocopy 退出码被误判为失败

**现象**: VS Code 任务显示 `robocopy ... 退出代码: 1` 报错

**原因**: robocopy 退出码 1 = "成功复制了文件"。VS Code 把所有非零退出码当失败。

**解决**: 用 PowerShell 脚本包装 robocopy，只有 `$LASTEXITCODE >= 8` 才报错。

> robocopy 返回码: 0=无变化, 1=已复制, 2-7=有差异/额外文件, >=8=真正的错误

相关文件: `.vscode/deploy_to_target.ps1`

---

### 2. PowerShell -Command 引号嵌套冲突

**现象**: `无法将 "code: $LASTEXITCODE" 项识别为 cmdlet`

**原因**: VS Code 用单引号包裹 `-Command` 参数，命令内的单引号提前闭合，导致 PowerShell 解析错误。

**解决**: 用 `powershell.exe -File <脚本>` 代替内联 `-Command`，彻底避免 JSON/Shell/PowerShell 三层引号冲突。

---

### 3. cwd 路径：本地 vs 远程的误解

**现象**: `-environment-cd C:/Users/googol/Desktop/...: No such file or directory`

**原因**: launch.json 中 `cwd` 是发给**本地 gdb** 的，不是发给远程 gdbserver 的。`-environment-cd` 在本地机器上执行 chdir，远程进程的工作目录由 gdbserver 启动时所在目录决定。

**解决**: `"cwd": "${workspaceFolder}"` — 填本地项目路径即可。

---

### 4. Windows 防火墙拦截 1234 端口

**现象**: `TcpTestSucceeded: False`，但 ping 通

**解决**: GNC 上管理员 cmd 执行:
```cmd
netsh advfirewall firewall add rule name="gdbserver" dir=in action=allow protocol=tcp localport=1234
```

验证:
```powershell
Test-NetConnection -ComputerName 192.168.1.2 -Port 1234
```

---

### 5. gdbserver 只绑定 IPv6

**现象**: `netstat -ano` 显示 `[::]:1234 LISTENING`，但 IPv4 连接不上

**原因**: `gdbserver.exe :1234 chipSetter.exe` 默认只绑 IPv6。

**解决**: 显式指定 IPv4 地址 `0.0.0.0`:
```cmd
gdbserver.exe 0.0.0.0:1234 chipSetter.exe
```

---

### 6. GDB 16.2 栈溢出崩溃

**现象**: 连上 gdbserver 后 `*** stack smashing detected ***: terminated`

**原因**: GDB 16.2 (i686-w64-mingw32) 在从远程目标读取 exe 文件时触发栈溢出 bug。

**解决**: 在 setupCommands 最前面加 `set sysroot`（清空 sysroot），阻止 gdb 从远程下载文件:
```json
{
    "text": "set sysroot",
    "description": "阻止从远程下载文件 (避免 gdb 16.x 栈溢出崩溃)",
    "ignoreFailures": false
}
```

**正确的手动连接顺序**:
```
(gdb) set sysroot
(gdb) file D:/path/to/local/debug/chipSetter.exe   // 先加载本地符号
(gdb) target remote 192.168.1.2:1234                // 再连接远程
```

---

### 7. sourceFileMap 导致断点 PENDING

**现象**: VS Code 在 main.cpp 设的断点全部显示 `<PENDING>`，永远不命中

**原因**: `sourceFileMap` 把断点路径翻译成 MSYS2 格式 (`/d/...`)，但 debug info 里存储的是标准 Windows 路径 (`D:\...\`)，两边完全不匹配。

**验证 debug info 中的路径**:
```powershell
gdb -q -batch -ex "file debug/chipSetter.exe" -ex "info sources" 2>&1 | findstr main.cpp
```

**解决**: **删掉 sourceFileMap**。当 debug info 的路径和本地文件系统一致时，不需要映射。

---

### 8. stopAtEntry 停在 CRT 入口而非 main()

**现象**: F5 后停在 `crtexewin.c:23` 而不是用户的 `main()`

**原因**: `stopAtEntry: true` 停的是 PE 入口点 (`__mingw_CRTStartup`)，不是 C++ 的 `main()`。

**解决**: `"stopAtEntry": false`，改为在编辑器中手动设断点。VS Code UI 断点在 gdb 连上后才下发，这时符号已加载不会 PENDING。

---

### 9. WinRM 远程启动 gdbserver

**一次性配置 — GNC 端（管理员 cmd）**:
```cmd
winrm quickconfig -force
Enable-PSRemoting -Force
Set-Item WSMan:\localhost\Client\TrustedHosts -Value "192.168.1.100" -Force
netsh advfirewall firewall add rule name="WinRM" dir=in action=allow protocol=tcp localport=5985-5986
net localgroup "Remote Management Users" googol /add
```

**一次性配置 — 开发机端**:
```powershell
winrm quickconfig
Set-Item WSMan:\localhost\Client\TrustedHosts -Value "192.168.1.2" -Force
# 首次运行 start_gdbserver.ps1 会提示输入 GNC 密码
# 密码加密保存到 ~\.chipsetter_cred.xml，后续无需重复输入
```

---

### 10. Session 0 隔离：WinRM 启动的进程无 GUI

**现象**: gdbserver 启动成功、调试正常，但 chipSetter 窗口不显示

**原因**: WinRM/WMI 创建进程默认跑在 Session 0（非交互会话），Windows Vista+ 禁止 Session 0 显示 GUI。

**解决**: 用 `schtasks /it` 把进程投递到交互式桌面会话:
```powershell
schtasks /create /tn "chipSetter-gdbserver" /tr "<exe路径>" /sc once /st 00:00 /it /f
schtasks /run /tn "chipSetter-gdbserver"
```

**前提**: GNC 必须有人登录（RDP 或本地），否则无交互式会话。

> **关键细节**: schtasks 的 `/st` 时间不能早于当前时间，否则会触发警告。脚本中动态计算 `(Get-Date).AddMinutes(1).ToString("HH:mm")` 来解决。

---

### 11. 停止调试时远程进程不退出

**现象**: Shift+F5 后 chipSetter.exe 继续运行，UI 不消失

**解决**: 
a) gdbserver 加 `--once` 参数 — gdb 断开时 gdbserver 自动退出
b) launch.json 加 `postDebugTask` — VS Code 停止调试后自动远端杀进程

```json
"postDebugTask": "远端结束 gdbserver"
```

---

### 12. SMB 共享认证失败（错误 1326）

**现象**: robocopy 报 `错误 1326 (0x0000052E) 用户名或密码不正确`

**原因**: 资源管理器能打开 SMB 共享（GUI 缓存了凭证），但命令行 robocopy 跑在 VS Code 任务进程里，凭证上下文不同。

**解决**: 部署脚本 (`deploy_to_target.ps1`) 在 robocopy 前先用 `net use` 建立 SMB 连接:
```powershell
$cred = Import-Clixml "$env:USERPROFILE\.chipsetter_cred.xml"
$password = $cred.GetNetworkCredential().Password
net use "\\$TargetIp\share" /user:$RemoteUser $password /persistent:no 2>$null
```

或手动一次性:
```cmd
net use \\192.168.1.2\share /user:googol * /persistent:yes
```

---

### 13. 部署遗漏 DLL（Qt + googol/gts）

**现象**: GNC 上 chipSetter.exe 闪退，报告 `Qt5Core.dll`, `Qt5Gui.dll`, `gts.dll` 找不到

**原因**: 打包脚本 `package_debug.ps1` 只复制了 MinGW runtime DLL，遗漏了:
- Qt DLL（Qt5Core/Gui/Widgets 等）
- Qt 平台插件 (`platforms/qwindows.dll`)
- googol GTS 运动控制库 (`gts.dll`, `gts.lib`)

**解决**: 打包脚本加入三个步骤:
1. **windeployqt** — 自动收集 Qt DLL 和平台插件
2. **googol/** 目录复制 — GTS 运动控制库
3. 部署脚本加 **`/E`** 标志 — robocopy 递归复制子目录（`platforms/`）

相关文件: `.vscode/package_debug.ps1`, `.vscode/deploy_to_target.ps1`

---

### 14. MSYS2 登录 shell 覆盖 MSYSTEM → 64 位编译

**现象**: 改代码后 F5，debug exe **从未更新**，始终显示旧 UI

**排查过程**:
1. 确认 `make` 手动跑能重新编译 → 编译命令本身没问题
2. 确认文件时间戳正确、依赖规则存在 → make 应该检测到改动
3. 检查 VS Code 任务的 bash 命令 → `bash -lc "export MSYSTEM=MINGW32 && ..."`
4. **关键发现**: 不带环境变量跑 `bash -lc 'echo $MSYSTEM'` 输出 `MINGW64`

**根因**: `bash -lc` 是 MSYS2 登录 shell。`/etc/profile` 先于 `-c` 命令执行，将 `MSYSTEM` 设为 `MINGW64`。命令里的 `export MSYSTEM=MINGW32` 虽然也执行了，但 PATH 中 `/mingw64/bin` 已经在前面 → g++ 实际用的是 **64 位版本** → 链接 Qt 32 位库时产生 `undefined reference` / `file format not recognized` 错误 → VS Code 问题匹配器只匹配 `file:line: error:` 格式，链接器错误格式不同 → **静默失败** → 旧 exe 没被替换。

**解决**: 在 VS Code task 的 `env` 块中**预设** `MSYSTEM=MINGW32`，让 bash 启动前环境变量就是正确的，登录脚本检测到已有值就不会覆盖:

```json
"options": {
    "cwd": "${workspaceFolder}",
    "env": {
        "MSYSTEM": "MINGW32"
    }
}
```

> 关键原则: MSYS2 登录 shell 的环境变量要在启动时传入，不能在 `-c` 命令里 `export`（已经晚了）。

---

### 15. 每次 F5 跑 qmake 破坏增量编译

**现象**: 即使 MSYSTEM 正确，代码改动后 make 也不重编

**原因**: F5 的任务链每次都先跑 `qmake` 重新生成 `Makefile.Debug`。qmake 更新 Makefile 的时间戳后，make 的依赖追踪状态被重置。

**解决**: 将 qmake 从自动构建链中移除。日常 F5 只跑 `make`（增量编译）。只有以下情况才手动运行 `qmake Debug`:
- 新增了 `.cpp` / `.h` 文件
- 修改了 `.pro` 工程文件
- 新增了 Qt 信号/槽（需要重新 moc）

```json
// 编译 Debug 版本：只依赖 make，不依赖 qmake
"dependsOn": ["make Debug (MSYS2)"]
```

---

## 最终配置参考

### launch.json 关键配置

```jsonc
{
    "type": "cppdbg",                          // 需要 ms-vscode.cpptools 扩展
    "request": "launch",
    "program": "${workspaceFolder}/debug/chipSetter.exe",   // 本地符号文件
    "cwd": "${workspaceFolder}",               // 本地路径, 不是远程路径
    "miDebuggerServerAddress": "192.168.1.2:1234",
    "miDebuggerPath": "D:/Code_Languages/C/msys64/mingw32/bin/gdb.exe",
    "MIMode": "gdb",
    "stopAtEntry": false,                      // 不停 CRT 入口, 用 UI 断点
    "setupCommands": [
        { "text": "set sysroot", "ignoreFailures": false },
        { "text": "-enable-pretty-printing", "ignoreFailures": true },
        { "text": "-gdb-set disassembly-flavor intel", "ignoreFailures": true },
        { "text": "set auto-solib-add off", "ignoreFailures": true }
    ],
    // 不要 sourceFileMap！debug info 已经是 Windows 路径
    "preLaunchTask": "编译并部署 Debug 到目标机",
    "postDebugTask": "远端结束 gdbserver"
}
```

### Tasks 链路

```
F5 → preLaunchTask: "编译并部署 Debug 到目标机"
       ├─ 远端结束 gdbserver    (stop_gdbserver.ps1 → WinRM 杀旧进程)
       ├─ make Debug (MSYS2)     (bash + MSYSTEM=MINGW32 env → 增量编译)
       ├─ 打包 Debug 部署包      (package_debug.ps1: exe+windeployqt+googol+gdbserver+DLL)
       ├─ 部署到目标机            (deploy_to_target.ps1: net use + robocopy /E)
       └─ 远端启动 gdbserver      (start_gdbserver.ps1 → WinRM → schtasks /it)
  → gdb 连接调试
  → Shift+F5
  → postDebugTask: "远端结束 gdbserver" (stop_gdbserver.ps1 → WinRM)
```

> **qmake 不自动跑**。新增文件或改 .pro 后手动 Ctrl+Shift+P → Tasks: Run Task → qmake Debug。

### 脚本文件

| 文件 | 用途 |
|------|------|
| `.vscode/deploy_to_target.ps1` | net use SMB + robocopy /E 部署 |
| `.vscode/package_debug.ps1` | windeployqt + googol + MinGW DLL + gdbserver 打包 |
| `.vscode/start_gdbserver.ps1` | WinRM → schtasks /it 远端启动 gdbserver |
| `.vscode/stop_gdbserver.ps1` | WinRM 远端杀 gdbserver + chipSetter |

### GNC 目标机检查清单

- [ ] GNC 已登录（RDP 或本地桌面）
- [ ] 防火墙: 1234 + 5985-5986 端口已放行
- [ ] gdbserver 使用 `0.0.0.0:1234`（不是 `:1234`）
- [ ] gdbserver 带 `--once` 参数
- [ ] 开发机已保存 WinRM 凭证 (`~\.chipsetter_cred.xml`)
- [ ] GNC 上有完整 DLL: Qt5*.dll, gts.dll, MinGW runtime, platforms/qwindows.dll

### 开发机检查清单

- [ ] VS Code task 中 `MSYSTEM=MINGW32` 写在 `env` 块（不是 bash 命令里）
- [ ] 日常改代码直接 F5（增量编译），不加新文件不跑 qmake
- [ ] `sourceFileMap` 已删除（debug info 是 Windows 路径时不需要）

### gdbserver 启动参考命令

```cmd
cd C:\Users\googol\Desktop\share\chipSetter
gdbserver.exe --once 0.0.0.0:1234 chipSetter.exe
```
