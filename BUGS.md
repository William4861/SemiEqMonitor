# 缺陷记录 BUGS

> 记录开发过程中发现并修复的缺陷。
> 格式：**编号 / 发现日期 / 现象 / 复现步骤 / 根因 / 修复 / 回归验证**。
> 目的：一方面防止同一问题复发，另一方面为面试提供"调试过程"的真实素材。

| 编号 | 发现 | 严重度 | 状态 | 一句话 |
|---|---|---|---|---|
| BUG-0001 | 2026-09-15 | 阻塞 | 已修复 | `moc.exe` 无法处理含中文的项目路径，导致构建失败 |
| BUG-0002 | 2026-09-15 | 阻塞 | 已修复 | `configure_file` 生成头文件目录层级错误，`#include "common/version.h"` 找不到 |
| BUG-0003 | 2026-09-15 | 高 | 已修复 | 设备模拟器三目运算符优先级写错，温度始终为故障值 |

---

## BUG-0001 · moc 无法处理非 ASCII 路径

- **现象**：`cmake` 配置成功、`g++` 能编译，但 `moc` 全部报错：
  ```
  Warning: Failed to resolve include ".../C++Qt??/SemiEqMonitor/build/semieq_core_autogen/moc_predefs.h"
  moc: Cannot create .../C++Qt??/SemiEqMonitor/build/semieq_core_autogen/.../moc_logger.cpp
  ```
  路径中的中文被替换为 `??`。
- **复现**：把项目放在任意含中文的目录下，执行 `cmake --build build`。
- **根因**：`moc` 内部按本地 8 位编码处理文件路径，非 ASCII 字符在转换中丢失，
  于是既解析不到 `moc_predefs.h`，也创建不了输出文件。
  （`g++` 对 UTF-8 路径的容忍度更高，所以会出现"编译器没问题、moc 有问题"的错觉。）
- **修复**：项目迁至**纯 ASCII 路径**；并在 `README.md` 的环境要求里写明该限制。
- **回归验证**：迁移后 `cmake --build build` 一次通过，4 个目标全部产出。

---

## BUG-0002 · 生成头文件目录层级不匹配

- **现象**：编译报 `fatal error: common/version.h: No such file or directory`。
- **根因**：`configure_file` 把文件生成到 `${BINARY_DIR}/generated/semieq/version.h`，
  而代码 include 的是 `common/version.h`；`-I${BINARY_DIR}/generated` 只能拼出
  `generated/common/version.h`，与之不符。
- **修复**：输出路径改为 `${CMAKE_CURRENT_BINARY_DIR}/generated/common/version.h`，
  与源文件目录 `src/common/` 保持同样的层级。
- **回归验证**：重新配置 + 编译通过。

---

## BUG-0003 · 三目运算符与加法混写导致优先级错误

- **现象**：设备模拟器的"腔体温度"无论是否注入故障，都恒为故障值 95。
- **复现步骤**（修复前）：
  ```cpp
  m_values[0] = 45.0 + m_injectHighTemp ? 95.0 : 45.0 + 8.0 * qSin(t / 6.0) + noise(1.2);
  ```
- **根因**：`?:` 的优先级**低于** `+`，表达式被解析为
  `(45.0 + m_injectHighTemp) ? 95.0 : (...) `；条件恒为非零 → 永远取 95.0。
- **修复**：把基础值先算好，条件表达式单独一行；
  同时加注释提醒不要混写：
  ```cpp
  m_values[0] = m_injectHighTemp ? 95.0
                                 : 45.0 + 8.0 * qSin(t / 6.0) + noise(1.2);
  ```
- **回归验证**：端到端联调读回温度 = 49（正常范围内波动），注入故障后再读 = 95。

---

## 待观察（低优先级）

| 编号 | 说明 | 计划 |
|---|---|---|
| TODO-0001 | `ModbusTcpClient::poll()` 未处理"上一帧未响应"的情况，慢设备下可能堆积请求 | D5 采集线程一并处理：加 `m_waitingResponse` 超时判断 |
| TODO-0002 | 模拟器 `onReadyRead()` 用函数内 `static QByteArray` 缓冲，只支持单客户端 | 当前够用；如需多客户端改为按 socket 存缓冲 |
