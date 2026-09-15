# SemiEqMonitor

> 半导体设备数据采集与监控上位机 ｜ C++11 / Qt 5.14 / SQLite / Modbus-TCP

面向半导体/泛半导体制造设备的**上位机监控软件**：多线程采集设备参数、实时监控与报警、
SPC 过程能力分析、良率与 Wafer Map 可视化、数据落库与 MES 上报。

配套一个**独立的设备模拟器**（`SemiEqSimulator`），因此**无需任何真实硬件**即可完整演示，
也便于他人复现与自动化测试。

---

## 1. 项目背景与定位

半导体设备（AOI/SPI/AXI 检测机、测试机等）的软件通常分两层：

- **设备侧**：控制运动、采集传感器、执行测试
- **上位机（HMI）**：连接设备、展示数据、判定与报警、统计分析、对接 MES

本项目实现**上位机侧**的完整能力，并用模拟器扮演设备侧。

> 为什么做这个题：目标岗位集中在半导体设备与精密装备行业（设备软件/上位机/机器视觉方向），
> 本项目刻意覆盖这些岗位 JD 的公共技术点 —— 多线程、串口/网络通信、关系型数据库、
> 模型视图、统计过程控制。

---

## 2. 系统架构

```
┌──────────────────────────────────────────────────────────┐
│  表现层 UI（主线程）                                       │
│  主窗口布局 │ 实时曲线 │ 表格 Model │ 报警 · Wafer Map      │
└──────────────▲──────────────────────────┬─────────────────┘
               │ 信号槽（Queued）          │ 用户操作
┌──────────────┴──────────────────────────▼─────────────────┐
│  业务层 Service（工作线程）                                │
│  数据解析 │ 报警引擎 │ SPC · 良率 │ 报表 · MES              │
└──────────────▲──────────────────────────┬─────────────────┘
               │ 生产者-消费者队列          │ SQL
┌──────────────┴───────────────┐  ┌───────▼──────────────────┐
│  采集层 Acquisition（子线程） │  │  持久层 SQLite           │
│  IDeviceClient（抽象接口）    │  │  参数 / 批次 / 晶圆 / Die │
│  ├ ModbusTcpClient          │  │  报警 / 测试结果          │
│  ├ ModbusRtuClient（串口）   │  └──────────────────────────┘
│  └ SimulatorClient          │
└──────────────▲───────────────┘
               │ Modbus-TCP / 串口 RS485
┌──────────────┴───────────────────────────────────────────┐
│  设备模拟器 SemiEqSimulator（独立可执行程序）               │
│  周期性上报 8 路参数 · 可注入故障（超限 / 断线）            │
└──────────────────────────────────────────────────────────┘
```

**两条关键设计**

| 设计 | 说明 |
|---|---|
| **`IDeviceClient` 抽象接口** | 让"数据从哪来"与"业务怎么用"解耦。TCP / 串口 / 模拟器三种来源可互换（策略模式）。业务层只依赖接口，不依赖 `QTcpSocket` |
| **设备模拟器独立成程序** | 扮演设备侧、不依赖上位机任何代码。既解决"没有真实硬件"，也让**任何人都能复现完整演示** |

---

## 3. 当前进度

**v0.1.0（骨架）** —— 工程脚手架与通信底座已跑通：

- ✅ CMake 多目标构建（core 静态库 + 上位机 + 模拟器 + 单元测试）
- ✅ 分层目录结构与版本号统一管理（CMake `configure_file` 生成 `version.h`）
- ✅ 线程安全的分级日志器（`Logger` 单例）
- ✅ 半导体领域数据模型（Lot / Wafer / Die / Parameter / Alarm + 良率计算）
- ✅ **Modbus-TCP 协议实现**：请求帧组装 + 响应帧解析（大端、异常响应、帧长校验）
- ✅ **设备模拟器**：Modbus-TCP 服务端、8 路参数模拟、控制台故障注入
- ✅ SQLite 持久层：建表（幂等）+ 参数/报警写入 + 查询
- ✅ 主窗口外壳：菜单栏 / 状态栏 / 版本号；`关于` 对话框
- ✅ **单元测试 18 个用例全绿**（协议解析 + 良率计算）
- ✅ 端到端联调通过（模拟器 ↔ Modbus-TCP ↔ 客户端，25 字节响应校验）

**后续排期**（详见 `docs/需求说明.md`）

| 阶段 | 内容 |
|---|---|
| D2-D5 | Modbus-RTU 串口通道、`QThread` 多线程采集、生产者-消费者队列 |
| D6-D8 | 实时曲线（自绘 `QPainter`）、`QAbstractTableModel` 数据表格 |
| D9 | 报警引擎（阈值 + 持续时间）、日志查看器 |
| D10-D11 | **SPC（Cpk / X-bar 控制图）**、**Wafer Map**、良率报表与 CSV 导出 |
| D12 | MES mock 对接、配置管理、打包发布 |
| D13 | 文档补齐 + 演示录屏 + v1.0.0 |

---

## 4. 目录结构

```
SemiEqMonitor/
├── CMakeLists.txt              顶层构建脚本（4 个目标）
├── README.md / CHANGELOG.md / DEVLOG.md / BUGS.md
├── docs/
│   ├── 需求说明.md            功能清单与优先级（P0/P1/P2）
│   ├── 技术方案.md            架构分层与关键设计决策
│   ├── 协议说明.md            Modbus 寄存器映射表
│   ├── 数据库设计.md          表结构、索引、典型 SQL
│   └── 代码评审checklist.md   PR 自审清单
├── scripts/
│   ├── build.bat              一键配置 + 编译
│   └── run.bat                一键运行（monitor / sim / test）
├── src/
│   ├── main.cpp
│   ├── common/                版本、领域模型、日志
│   ├── acquisition/           通信层：IDeviceClient + ModbusTcpClient
│   ├── data/                  持久层：DatabaseManager
│   └── ui/                    界面：MainWindow
├── apps/simulator/            设备模拟器（独立程序）
└── tests/                     单元测试（Qt Test）
```

---

## 5. 构建与运行

### 环境要求

| 项 | 版本 |
|---|---|
| CMake | ≥ 3.16 |
| Qt | 5.14（需 Widgets / Network / Sql / SerialPort / Test） |
| 编译器 | MinGW 7.3（随 Qt 安装）或其他支持 C++11 的编译器 |
| 构建工具 | Ninja 或 Make |

> ⚠️ **路径必须全为 ASCII**：Qt 的 `moc.exe` 无法处理含中文/非 ASCII 的路径，
> 会报 `moc: Cannot create .../C++Qt??/...`。项目请放在纯英文路径下。

### 构建

```bat
scripts\build.bat
```

或手动：

```bat
set QT_ROOT=D:\OtherSoftwares\QT\QT\5.14.2\mingw73_64
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=%QT_ROOT%
cmake --build build
```

### 运行（两个程序，先起模拟器）

```bat
scripts\run.bat sim       :: 先启动设备模拟器
scripts\run.bat monitor   :: 再启动上位机
scripts\run.bat test      :: 跑单元测试
```

> 模拟器的控制台会打印操作说明；输入 `h` 可注入"腔体温度飙升"故障，
> 用来观察上位机的报警与曲线表现。

### 单元测试

```bat
scripts\run.bat test
:: 或
ctest --test-dir build --output-on-failure
```

---

## 6. 演示脚本（面试演示用）

1. 启动 `SemiEqSimulator` → 显示监听端口与命令行说明
2. 启动 `SemiEqMonitor` → 主窗口，状态栏显示"未连接"
3. 「文件 → 连接设备」→ 状态栏转为"已连接"，曲线开始滚动
4. 在模拟器控制台输入 `h` → 腔体温度飙升 → 上位机触发**上限报警**
5. 输入 `n` → 参数恢复 → 报警自动**恢复**（带恢复时间）
6. 输入 `d` → 强制断线 → 上位机检测到**断线并自动重连**
7. 「视图 → SPC 分析」→ 查看 **Cpk 与 X-bar 控制图**
8. 「视图 → Wafer Map」→ 查看晶圆良率分布图
9. 「文件 → 导出数据」→ 导出 CSV

---

## 7. 技术栈

| 分类 | 内容 |
|---|---|
| 语言 | C++11（`enum class`、智能指针、lambda、range-for） |
| 框架 | Qt 5.14 Widgets、`QThread`、`QTcpSocket`、`QSerialPort`、`QSqlDatabase` |
| 数据库 | SQLite（`parameter_log` / `alarm` / `wafer` / `die_result`） |
| 协议 | Modbus-TCP（功能码 0x03 读保持寄存器）、Modbus-RTU（排期中） |
| 统计 | SPC：均值 / 极差 / 标准差 / **Cpk**、X-bar 控制图 |
| 构建 | CMake + Ninja + MinGW |
| 测试 | Qt Test（18 用例） |
| 工程 | Git 分支策略、Conventional Commits、Issue / PR / CHANGELOG |

---

## 8. 开发规范

详见 `docs/代码评审checklist.md`。要点：

- **分支**：`main`（发布）← `dev`（日常开发）← `feat-*` / `fix-*`（单个功能）
  > ⚠️ **分支名不要用斜杠**（`feat/xxx`）——本机 Git for Windows 2.54 在 Git Bash 下
  > 会**静默失败**：命令退出码为 0，但分支根本没建出来；`git checkout -b feat/x` 更糟，
  > 会把 `HEAD` 指向一个不存在的 ref，仓库直接进入悬空状态。
  > 实测详情见 `BUGS.md`「环境坑」。
- **提交**：Conventional Commits（`feat:` / `fix:` / `docs:` / `refactor:` / `test:` / `chore:`）
- **日志**：每日更新 `DEVLOG.md`（目标 / 完成 / 问题 / 解决 / 明日计划）
- **版本**：语义化版本，`CHANGELOG.md` 随合并更新，发布打 tag
- **缺陷**：记录到 `BUGS.md`（现象 / 复现 / 根因 / 修复 / 回归验证）
- **参考资料**：`ref-skeleton` 分支保存了一份完整参考实现，可用
  `git diff ref-skeleton -- <文件>` 对答案（**写完再对**）

---

## 9. 许可

仅用于个人学习与求职展示。
