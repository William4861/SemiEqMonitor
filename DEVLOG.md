# 开发日志 DEVLOG

> 每个开发日一条。格式：**今日目标 / 完成情况 / 遇到的问题 / 怎么解决 / 学到的 / 明日计划**。
> 目的：①留下真实的开发轨迹 ②面试时能讲清"我遇到过什么、怎么解决的"。

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
- 建立第一个完整 Git 工作流样例：开 Issue → feature 分支 → PR → 自审 → 合并
- 开始 `QThread` 采集线程设计（`moveToThread` + QueuedConnection）

---

<!-- 新条目加在最上方（时间倒序），并在标题里标注 Day 序号 -->
