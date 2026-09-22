# 开发日志 DEVLOG

> 每个开发日一条。格式：**今日目标 / 完成情况 / 遇到的问题 / 怎么解决 / 学到的 / 明日计划**。
> 目的：①留下真实的开发轨迹 ②面试时能讲清"我遇到过什么、怎么解决的"。

---

## 2026-09-22（D2）· Modbus 协议解析：三个纯函数 + 18 用例全绿

### 今日目标
- 实现 `modbustcpclient.cpp` 的三个纯函数：组读请求帧 / 算响应帧长度 / 解析读响应帧
- 目标：`TestModbus` 从 12 红 → 全绿

### 完成情况
- ✅ **`Totals: 18 passed, 0 failed`**，编译 **0 error / 0 warning**
- ✅ 三个函数：
  - `buildReadHoldingRegistersRequest()` —— 12 字节请求帧，7 个字段全部按**大端**逐字节 append
  - `expectedResponseLength(registerCount)` —— `7 + 1 + 1 + 2N`
  - `parseReadHoldingRegistersResponse()` —— 六步顺序校验 + 数据段大端还原
- ✅ 迭代 4 轮：编译错 8 个 → 11 passed/7 failed → 12/6 → **18/0**
- ✅ 仓库首次推送到 GitHub（`William4861/SemiEqMonitor`），建立每日提交习惯

### 遇到的问题

**问题 1：写完不编译就往下走（同一个错犯了三轮）**

`static_cast` 前后拼错了三次（`transationId` / `staitc_cast` / `tatic_cast`），
另有 4 处 `return fail(QStringLiteral("...")` **少一个右括号**、
`res` 声明在 `else` 块内却在块外 `return`。这 8 个编译错**没有一个是逻辑问题**，全是"没验证"。

**怎么解决**：改完一个函数立刻编译，别攒着。
```bash
scripts\build.bat
```
**学到的**：编译器是最便宜的检查工具。攒到最后再编译，等于把 8 个错误一次性摊在面前。

**问题 2：一个 `!` 让 6 个用例变红**

判断异常响应时写成了
```cpp
else if (!(static_cast<quint8>(frame.at(7)) & 0x80))   // ❌ 多了个 !
```
`0x03 & 0x80 = 0` → 取反成 `true` → **正常帧被判成异常响应**；
而 `0x83 & 0x80 = 0x80` → 取反成 `false` → **异常帧反而漏过了异常分支**。

**怎么解决**：去掉 `!`，并确认**异常响应的判断排在"功能码 ≠ 0x03"之前** ——
否则 `0x83` 会先被 `0x83 != 0x03` 截胡，异常码那个字节永远读不到。

**学到的**：位掩码判断写完，用两个**极值**各代一遍（`0x03` 和 `0x83`）。
一个 `!` 的代价是 6 条用例。

**问题 3：成功路径漏了 `*ok = true`，而报错信息是空的**

测试报 `'ok' returned FALSE. ()` —— **括号里是空的**。
空 `error` 说明**根本没走失败分支**，是成功路径返回的，只是忘了把传出参数 `ok` 置 true。

**怎么解决**：在 `else` 分支里补 `*ok = true;`。

**学到的**：**报错信息的"缺失"本身就是线索**。`ok = false` 但 `error` 为空，
逻辑上只有一种可能（没经过失败分支）。以后遇到"断言失败但错误信息空白"，先往这个方向想。

**问题 4：两个下标空间混用导致越界**

数据从 `frame.at(9)` 开始取，却拿 `9 + i` 去索引自己攒的临时数组（它只有 `0 ~ byteCount-1`）。

**怎么解决**：临时数组的下标从 0 起 —— `temp.at(i - 1)` / `temp.at(i)`。

**学到的**：**`frame` 的下标是"整帧坐标"，临时数组的下标是"数据段坐标"，两者不能混用**。
写循环前先问一句"我手里这个变量是哪个坐标系里的"。

**问题 5：直接跑 exe 弹「无法定位程序输入点」**

编译成功，但运行 `build\bin\semieq_tests.exe` 报
`无法定位程序输入点 ?compareStrings@QPrivate@@… 于 …\5.14.2\msvc2017_64\bin\Qt5Test.dll`。

**根因**：本机装了**两套 Qt 5.14.2**（`mingw73_64` + `msvc2017_64`），
而用户 PATH 里 **msvc 排在 mingw 前面** → exe 启动时先找到了 MSVC 版的 Qt DLL。
**MSVC 编译的 DLL 与 MinGW 编译的 exe 二进制不兼容** —— 两者的 C++ 符号名编码方式不同
（`?xxx@@` 是 MSVC 修饰名，`_Zxx` 是 GCC 修饰名），所以"找不到入口点"。

**怎么解决**：走项目脚本（它会把 `mingw73_64\bin` 前置到 PATH）
```bash
scripts\build.bat && scripts\run.bat test
```
**学到的**：**"能编译"和"能运行"是两件事** —— 编译期和加载期的依赖解析路径不同。
混装多套 Qt 的机器上，这个坑很容易踩到。

**问题 6：`git push` 连不上 GitHub —— 代理配置与实际端口不符**

`git config` 里写的是 `http://127.0.0.1:7897`，但代理软件实际监听 **7890**
（而且值被写成了 `127.0.0.7890`，少了一个 `1:`）→ git 把它当主机名解析，
报 `Could not resolve proxy: 127.0.0.7890`。

**怎么解决**：
```bash
git config --global http.proxy  http://127.0.0.1:7890
git config --global https.proxy http://127.0.0.1:7890
git config --get http.proxy      # 回读验证
```
**学到的**：**改完配置必须回读验证**。`git config --get` 只要一秒，
但少一个字符能让人查半天。"退出码 0 ≠ 操作成功"这条，对配置类操作同样成立。

### 今日新增知识点
- **MBAP 报文头里的「长度」字段**数的是**它自己后面的字节数**（单元标识 1 + PDU 5 = 6），
  **不含 MBAP 自己那 7 字节**，也不是整帧的 12
- **字节序 ≠ 移位**：字节序是"多字节数值的**字节之间**的位置"，移位是"一个字节**内部**的 bit 移动"。
  `0x0031`(49) 字节序写反 → `0x3100`(=12544)；而 `0x80 → 0x08` 是移位，与字节序无关
- **`char` 在 x86 上有符号** → 字节 ≥ `0x80` 会变负数（`0x80` → `-128`），
  必须先 `static_cast<quint8>` 再参与运算，否则**符号扩展**会污染结果
- `QStringLiteral` 只能包字符串字面量，不能在它内部做 `+` 拼接；拼数字用 `QString::number()`
- **输出参数的语义**：函数有义务在**成功路径也**把 `*ok` 置 true，不能指望调用方预先初始化
- **`git add` 多文件时，只要有一个 pathspec 不匹配就整体失败**（`fatal: pathspec`），一个都不会暂存

### 明日计划（D2 第 2 步）
- 看 `BV1XW411x7NU` **P55 / P59 / P60 / P73**（Qt TCP + 粘包，约 51 分钟）
- 实现连接部分：`connectToDevice()` / `disconnectFromDevice()` / `onConnected` / `onDisconnected` / `onSocketError`
- 手工联调：`scripts\run.bat sim`（模拟器）+ `scripts\run.bat monitor`（上位机）

---

## 2026-09-15（D1 复盘）· 把实现体交回给自己

### 今日目标（追加）
D1 的骨架搭完之后复盘了一遍，发现一个问题：**脚手架里把三个核心模块的实现体也一起写完了**
（Modbus 协议、SQLite 持久层、主窗口）。这些恰恰是这个项目面试时最会被追问的部分 ——
如果实现不是我写的，我就只能背代码，讲不出"为什么这么写"。

### 完成情况
- ✅ 重新划清分工：**脚手架与规格由工具生成，实现体我自己写**
  - 保留（不属于学习重点）：构建系统、目录结构、文档、脚本、PR/Issue 模板、
    全部 `.h` 头文件、单元测试、设备模拟器、`src/common/`（日志器与数据模型）
  - **改为填空**：`modbustcpclient.cpp`（协议）、`databasemanager.cpp`（SQL）、
    `mainwindow.cpp`（界面）—— 三份文件里留了 `TODO(D2/D4/D6-x)` 说明目标与验收点
- ✅ 完整实现保存在 **`ref-skeleton` 分支**，用 `git diff ref-skeleton -- <文件>` 对答案
- ✅ 新增 `docs/tasks/` 目录：每个模块一份任务卡（含前置知识小课 + 分步路线 + 自检清单）
- ✅ 构建复验：改完后仍**零警告**编译通过，测试处于预期"红"状态（未实现用例失败）

### 遇到的问题

**问题 3：分支名带斜杠时 Git 静默失败，还把 HEAD 弄悬空了**

`git branch ref/skeleton` 返回 0、无输出，但分支根本不存在。更糟的是
`git checkout -b feat/x 684a10b` 打印了 `Switched to a new branch 'feat/x'`，
实际却把 `HEAD` 写成 `ref: refs/heads/feat/x` —— 一个**从未被创建的 ref**。
结果 `git status` / `git rev-parse HEAD` 全部报
`fatal: ambiguous argument 'HEAD'`，而且索引被污染，两个源文件被回退成旧版本。

排查：先怀疑文件系统不支持嵌套目录，于是手工
`mkdir -p .git/refs/heads/zz && echo <sha> > .git/refs/heads/zz/one`
—— **磁盘完全支持，Git 也认这个 ref**。所以问题出在 Git 自己写 ref 的路径上。

**怎么解决**：
```bash
printf "ref: refs/heads/main\n" > .git/HEAD   # ① 把 HEAD 指回真实分支
git reset --hard HEAD                          # ② 索引被污染，一并修正
git branch ref-skeleton 684a10b                # ③ 改用扁平命名，一次成功
```
并定下约定：**分支名一律扁平命名**（`feat-modbus` / `fix-crc` / `ref-skeleton`），不用斜杠。

> 📌 **2026-09-22 更正：这条约定已作废。** 当天定位到——嵌套 ref 写不进去**只发生在沙箱化的执行环境**里，
> 普通本机终端没有这个限制（同一台机器上对比验证过）。**斜杠分支名（`feat/xxx`）可以正常使用**，
> 详见 `BUGS.md` BUG-0004 的更新。

**学到的**：🔴 **退出码为 0 ≠ 操作成功。**
凡是会改动状态的操作（建分支、改文件、装包），执行完必须**再验证一次结果**
（`git branch -a` / `ls` / 读回内容），否则会带着错误状态继续往下走 ——
这次差一点就在"HEAD 已经悬空"的仓库上继续提交。
详见 `BUGS.md` BUG-0004。

### 明日计划（D2）
- **第 1 步**：实现 Modbus 三个纯函数（组帧 / 期望长度 / 解析）→ 目标：**10 个用例变绿**
- **第 2 步**：实现 QTcpSocket 连接部分 → 手工联调通
- 走一遍完整 Git 工作流：`feat-*` 分支 → 提交 → 合并 `dev` → 更新本日志
- 详见 `docs/tasks/D2_Modbus协议实现.md`

---

## 2026-09-15（D1）· 工程骨架 + 通信底座

### 今日目标
- 确定技术选型与工程规范
- 搭出可编译运行的四目标 CMake 工程
- 打通 Modbus-TCP 通信链路（含设备模拟器）

### 完成情况
- ✅ 技术选型定案：**CMake + Qt 5.14.2 + C++11 + Ninja**；图表用**自绘 QPainter**（不依赖 Qt Charts）
- ✅ 仓库结构、`CMakeLists.txt`（4 个目标）、版本号统一由 CMake `configure_file` 生成
- ✅ **`semieq_core` 静态库**：`Logger`（线程安全单例）、领域数据模型、`ModbusTcpClient`、`DatabaseManager`
- ✅ **`SemiEqMonitor`** 上位机外壳：菜单栏 / 状态栏 / 版本号 / 关于对话框
- ✅ **`SemiEqSimulator`** 设备模拟器：Modbus-TCP 服务端、8 路参数模拟、控制台故障注入（h/n/d/l/q）
- ✅ **`semieq_tests`** 单元测试：**18 个用例全部通过**
- ✅ **端到端联调通过**：模拟器 ↔ Modbus-TCP ↔ 客户端，响应 25 字节，8 个寄存器值正确

### 遇到的问题

**问题 1：`moc.exe` 报 `Cannot create .../C++Qt??/...` —— 中文路径**

一开始项目放在 `C++Qt项目/SemiEqMonitor`。`cmake` 配置成功、`g++` 编译也正常，
但 Qt 的 `moc.exe` 全部失败：

```
Warning: Failed to resolve include ".../C++Qt??/SemiEqMonitor/build/semieq_core_autogen/moc_predefs.h"
moc: Cannot create .../C++Qt??/SemiEqMonitor/build/semieq_core_autogen/.../moc_logger.cpp
```

注意路径里的 `C++Qt??` —— 中文被替换成了 `??`，说明 moc 内部按本地 8 位编码处理路径，
非 ASCII 字符直接丢失，于是文件创建失败。

**怎么解决**：把项目移到**纯 ASCII 路径**（`CppQtProject/SemiEqMonitor`）后一次通过。
并在 `README.md` 里把这条写进了环境要求，避免他人踩同样的坑。

**学到的**：**Qt 工具链（moc/uic/rcc）对非 ASCII 路径支持很差**，而 `g++` 反而能忍。
所以"能编译"不代表"工具链都没问题" —— 出现莫名其妙的 moc/uic 报错时，先看路径。

**问题 2：`common/version.h: No such file or directory`**

`configure_file` 的输出写成了 `generated/semieq/version.h`，但代码里 include 的是 `common/version.h`，
两者目录层级不一致，导致只能找到 `generated/` 却拼不出正确的相对路径。

**怎么解决**：把输出路径改为 `generated/common/version.h`，与 `src/common/` 的层级保持一致。

**学到的**：生成文件要与源文件**保持相同的目录层级**，否则 include 路径会很别扭；
或者干脆用 `#include "semieq/version.h"` 这种带命名空间的写法，但要全项目统一。

### 今日新增知识点
- CMake `configure_file()` 生成版本头文件（版本号单一来源）
- CMake `AUTOMOC` 对 `.cpp` 内 `Q_OBJECT` 的处理：需要 `#include "xxx.moc"`
- **Modbus-TCP 帧结构**：MBAP 头（事务标识 / 协议标识 / 长度 / 单元标识）+ PDU；**所有多字节字段为大端**
- `Q_DECLARE_METATYPE` + `qRegisterMetaType` 的配合：前者让类型"可注册"，后者才让 QueuedConnection 真正可用
- `QMutex` 默认非递归 → 持有锁时不能再调用同样加锁的函数（写 `Logger::open` 时差点踩到）

### 明日计划（D2）
- `IDeviceClient` 接出模拟器实现，跑通"采集 → 解析 → 日志"闭环
- 建立第一个完整 Git 工作流样例：开 Issue → `feat-*` 分支 → PR → 自审 → 合并
  > 📌 分支名用 `feat/xxx` 斜杠命名是**可以**的（`BUG-0004` 已于 2026-09-22 更正：
  > 那是沙箱化执行环境的限制，普通本机终端无此问题）
- 开始 `QThread` 采集线程设计（`moveToThread` + QueuedConnection）

> 📌 **本条目中的实现体已于当日复盘时拆回填空式**，见上方「D1 复盘」条目。

---

<!-- 新条目加在最上方（时间倒序），并在标题里标注 Day 序号 -->
