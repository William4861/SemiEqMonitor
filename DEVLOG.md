# 开发日志 DEVLOG

> 每个开发日一条。格式：**今日目标 / 完成情况 / 遇到的问题 / 怎么解决 / 学到的 / 明日计划**。
> 目的：①留下真实的开发轨迹 ②面试时能讲清"我遇到过什么、怎么解决的"。

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
  > ⚠️ 分支名用扁平命名，不要写 `feat/xxx`（原因见 BUGS.md BUG-0004）
- 开始 `QThread` 采集线程设计（`moveToThread` + QueuedConnection）

> 📌 **本条目中的实现体已于当日复盘时拆回填空式**，见上方「D1 复盘」条目。

---

<!-- 新条目加在最上方（时间倒序），并在标题里标注 Day 序号 -->
