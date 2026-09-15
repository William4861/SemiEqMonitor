# 更新日志 CHANGELOG

本文件记录所有值得注意的变更。
格式参考 [Keep a Changelog](https://keepachangelog.com/zh-CN/1.1.0/)，
版本号遵循[语义化版本](https://semver.org/lang/zh-CN/)。

分类：`新增` / `变更` / `修复` / `移除` / `安全`

---

## [0.1.0] - 2026-09-15

首个骨架版本：工程脚手架与通信底座跑通。

### 新增
- CMake 多目标构建：`semieq_core`（静态库）、`SemiEqMonitor`（上位机）、`SemiEqSimulator`（设备模拟器）、`semieq_tests`（单元测试）
- 版本号单一来源：顶层 `CMakeLists.txt` 的 `project(... VERSION)` → `configure_file` 生成 `common/version.h`
- **公共设施**：线程安全的分级日志器 `Logger`（单例 / 互斥保护 / 每条 flush）
- **领域数据模型**：`Parameter`、`Alarm`、`Lot`、`Wafer`、`Die`，含 `enum class` 状态枚举与良率计算
- **采集层**：`IDeviceClient` 抽象接口；`ModbusTcpClient` 实现请求帧组装与响应帧解析（大端、异常响应、帧长与字节数校验）
- **持久层**：`DatabaseManager` 实现 SQLite 建表（幂等）、参数与报警写入、按采集点查询、未恢复报警查询
- **界面**：`MainWindow` 外壳（菜单栏 / 状态栏 / 版本显示 / 关于对话框）
- **设备模拟器**：Modbus-TCP 服务端、8 路参数正弦+噪声模拟、控制台命令注入故障（`h` 温度飙升 / `n` 恢复 / `d` 强制断线 / `l` 打印值 / `q` 退出）
- **单元测试**：18 个用例，覆盖请求帧组装、响应帧解析（含 5 类非法输入）、期望帧长、良率计算
- 工程规范文件：`README.md`、`DEVLOG.md`、`BUGS.md`、`docs/`（需求 / 技术方案 / 协议 / 数据库 / 评审清单）

### 修复
- 修正 `configure_file` 生成头文件的目录层级，使 `#include "common/version.h"` 能正确解析
- 修正设备模拟器中三目运算符与加法混写导致的优先级错误（`45.0 + flag ? a : b` 被解析为 `(45.0 + flag) ? a : b`）

### 变更
- 项目路径从含中文的目录迁至纯 ASCII 路径，以规避 Qt `moc.exe` 对非 ASCII 路径的支持缺陷

---

## 未发布

### 计划中（对应排期 D2-D13）
- 新增：`ModbusRtuClient`（串口 RS485 / Modbus-RTU）、`SimulatorClient`
- 新增：`QThread` 采集线程 + 生产者-消费者队列
- 新增：实时曲线（自绘 `QPainter`）与 `QAbstractTableModel` 数据表格
- 新增：报警引擎（阈值 + 持续时间判定、确认与恢复）
- 新增：SPC 统计（均值 / 极差 / 标准差 / Cpk）与 X-bar 控制图
- 新增：Wafer Map 良率可视化、良率报表与 CSV 导出
- 新增：MES 上报（mock 服务）
- 新增：配置管理（JSON / QSettings）、`windeployqt` 打包发布
