# 开发日志 DEVLOG

> 每个开发日一条。格式：**今日目标 / 完成情况 / 遇到的问题 / 怎么解决 / 学到的 / 明日计划**。
> 目的：①留下真实的开发轨迹 ②面试时能讲清"我遇到过什么、怎么解决的"。

---

## 2026-10-28（D7 前半）· 报警引擎：前置修复 + `clearAlarm()`（**只完成 2/5，未验收**）

### 今日目标
- 开 D7 报警引擎：把"阈值判定 + 防抖 + 落 `alarm` 表"做出来
- 预计 3–4 h 完成，任务卡 `docs/tasks/D7_报警引擎.md`

### 完成情况（🔴 **只做了 2 块，今天没收尾**）

| # | 项 | 状态 |
|---|---|---|
| **D7-0** | `insertAlarm()` 时间字段改 `toString(Qt::ISODate)` | ✅ 完成 |
| **D7-b** | `clearAlarm(paramCode, clearedAt)` 实现（UPDATE + 事务 + 错误处理） | 🟡 **已写，还差一行**（`return true` → `numRowsAffected() > 0`）；`isNull()` → `isEmpty()` 已改 |
| **D7-0.5** | `schemaStatements()` 加部分唯一索引 `idx_alarm_one_active` | ⏳ 未做 |
| **D7-d** | `setupStatusBar()` 加 `m_alarmStatus` 标签 | ⏳ 未做 |
| **D7-e** | `checkAlarms()` 四态状态机 | ⏳ 未做 |
| **D7-f** | `onParametersRead()` 末尾调一行 | ⏳ 未做 |

**编译状态**：`.h` 骨架（`clearAlarm` 声明 / `checkAlarms` / `m_alarmStatus` / 三个 `QHash`）已加，`build 0 warning`、`dbtest 18/0`、`tests 18/0`。

### 遇到的问题（三个都是"实测才发现的"）

**① `QString::isNull()` 判不住空串**
`clearAlarm` 里写 `if (paramCode.isNull())` —— 本机实测：
```
QString a;                  → isNull=true   isEmpty=true
QString b = "";             → isNull=false  isEmpty=true   ← 空字面量不是 null
QStringLiteral("")          → isNull=false  isEmpty=true
d = "x"; d.clear();         → isNull=true   isEmpty=true
```
→ 判空必须用 `isEmpty()`。

**② `UPDATE ... WHERE` 会改**所有**匹配行，不是 1 行**
实测（SQLite 3.31.1，项目实际驱动）：插 3 条同 `param_code` 且 `cleared_at IS NULL` → 跑那条 UPDATE → **影响 3 行**。
想用 `UPDATE ... ORDER BY id LIMIT 1` 限一条 → ❌ **`near "ORDER": syntax error`**（该构建没开 `SQLITE_ENABLE_UPDATE_DELETE_LIMIT`，**UPDATE 不支持 LIMIT**）。
→ 解法：**部分唯一索引**（实测可建、可拦住第 2 条未恢复插入、已恢复的不受影响）。

**③ `checkAlarms` 放进 `private slots:` 会链接失败**
moc 会为槽生成 `qt_static_metacall` 里的调用，**没实现就是 `undefined reference to ...checkAlarms`**，`build.bat` 直接 `ld returned 1 exit status`。
→ 改成普通 `private:` 成员函数（它只被 `onParametersRead` 内部调用，不需要信号连接）。

### 学到的
- **`numRowsAffected()` 是 UPDATE/DELETE 的"真改到了吗"判据，但 `SELECT` 返回 0**（不是行数）；`size()` 在 SQLite 上返回 -1 → 数行数只能自己 `while(next()) ++n`
- **`alarm` 表是审计记录，只增不删**：产生（`raised_at`）→ 确认（`acknowledged`/`ack_by`）→ 恢复（`cleared_at`）。"断线"不等于"故障解除"，不能因为断线就把报警标成已恢复
- **`clearAlarm` 只改一行不是技术限制，是数据约束的体现**："一个采集点同时有多条未恢复报警"本身就是 bug，应该让它在 `INSERT` 时被唯一索引拒掉，而不是在 `clear` 时被悄悄改掉
- **`alarm` 表缺 `ack_at`**（只有 `acknowledged` + `ack_by`）→ 算不出响应时长 → 记 `BUGS.md` **TODO-0009**，推迟到 D9

### 明日计划
1. **补完 D7 剩余 3 块**（唯一索引 → `m_alarmStatus` → `checkAlarms` 状态机），按卡里 §7 那 7 条验收
2. **11/01 毕设必须开工**（只剩 3 天）
3. 10/29 埃芯追问邮件（10/26 投，第 3 个工作日）

---

## 2026-10-27（D6-3）· 采集数据落库：`parameter_log` 第一次有数据

### 今日目标
- 把 `onParametersRead()` 收到的 8 条采样批量写进 SQLite 的 `parameter_log`
- 补掉代码审查发现的硬伤：`main.cpp` 里 `DatabaseManager db;` 建完表就没人用了，`MainWindow` 拿不到它

### 完成情况
- ✅ **`DatabaseManager::insertParameters()`**：**空列表早退** → `transaction()` → **`QSqlQuery` + `prepare` 提到循环外**（解析一次、执行 N 次）→ `for (const Parameter &p : list)` 只 bind + exec → `commit()`；失败路径 `rollback()` 都在 `return` 之前
- ✅ **`main.cpp`**：`window.setDatabase(&db);`（在 `window.show()` 之前，`db` 仍是 `main()` 的局部对象，`MainWindow` 只借用）
- ✅ **`onParametersRead()`**：喂完曲线后 `if (m_db != nullptr && m_db->isOpen()) m_db->insertParameters(params);`
- ✅ **验收（实测查库）**：
  - **1248 行**，时间范围 `16:58:21 → 16:59:39`（**78 秒**）
  - **8 个采集点**，`recorded_at` 全部 **ISO 格式**（`2026-10-27T16:59:39`）
  - 数值合理：真空度 `0.396 kPa`（不是 0 → 证明 D6-1 的 per-channel scale 生效）
  - 🔎 **数据自洽校验**：`78 s ÷ 0.5 s = 156 轮 × 8 通道 = 1248` —— **一条不差**

### 遇到的问题（第一版写完被逐条打回）
1. 🔴 **SQL 两个关键字拼错**：`INSET`（应 `INSERT`）、`VALUE`（应 `VALUES`）→ **`prepare()` 直接失败**，而我没检查它的返回值，继续 `exec()` 也失败 → 每次插入都失败
2. 🔴 **函数末尾没有 `return true;`** → 编译器警告 `control reaches end of non-void function [-Wreturn-type]`，且是未定义行为
3. 🔴 **`param.timestamp` 直接绑**，没转 ISO 字符串 → 写进去的格式由驱动决定，而读库那边用 `fromString(..., Qt::ISODate)` 解析 → **写读格式不配对**
4. 🟠 **`QSqlQuery` + `prepare` 写在循环内** → 每轮重新构造、重新解析，正好丢掉了 `prepare` 唯一的价值
5. 🟠 `for (Parameter param : list)` 按值拷贝（3 个 QString + 1 个 QDateTime）

### 怎么解决
- 拼写与返回值：改成 `INSERT ... VALUES`；补 `return true;`；`prepare` 的返回值虽然仍没判，但 `exec` 会失败并走 rollback，不会静默成功
- 时间：写库 `p.timestamp.toString(Qt::ISODate)`，读库 `QDateTime::fromString(str, Qt::ISODate)` —— **两个方向都显式 ISO**
- 性能：`QSqlQuery query(m_db); query.prepare(...)` 移到循环外，循环里只 `addBindValue` + `exec`
- 补 `if (list.isEmpty()) return true;`、`const Parameter &`

### 学到的
- 🔴 **「Build succeeded」什么都不证明**：`INSET` + `VALUE` 两个关键字全错，还少一个 `return`，编译照样过。**只信运行结果**
- **编译器的 `-Wreturn-type` 是硬警告**：`bool` 函数漏 return 不是"风格问题"，是未定义行为
- **`QDateTime::toString()` 无参 = `Qt::TextDate`**（`qdatetime.h:113` 的默认参数），实测输出 **`周二 10月 27 16:22:45 2026`** —— 这种格式按字符串排序是乱的，`ORDER BY recorded_at` 直接失效
- **`fromString` 是 `toString` 的逆操作，两边格式必须配对**：实测 `fromString(toString(), ISODate)` → `isValid() = false`（绑进去成 NULL → 撞 `recorded_at NOT NULL` → INSERT 被拒）
- **事务的第二个用途是性能**：8 条逐条插 = 每秒 16 次自动提交 = 16 次 fsync；一个事务写 8 条，落盘降到 2 次。验收就靠"拖窗口跟不跟手"
- **数据能自洽校验**：`行数 ÷ 8 × 0.5 s` 应该 ≈ 实际运行秒数 —— 对不上就说明有丢帧或重复

### 明日计划
- **D7 报警引擎**：`lowerLimit / upperLimit` 字段早就有 → 阈值判定 + 持续时间 + 状态栏提示 + 落 `alarm` 表（`cleared_at` 用真 NULL 表达"未恢复"）
- 顺手修 `BUG-0012`（三条集成测试共用同一个 `QTcpServer`，导致 `disconnectEmitsDisconnected` 一直红）

---

## 2026-10-26（D6-2）· 采集线程化：`QTcpSocket` 搬进工作线程

### 今日目标
- 把 `ModbusTcpClient`（含 `QTcpSocket`）从 **UI 线程**搬进独立工作线程，UI 只发命令
- 目标：界面不卡、8 曲线照动、**关窗口不崩**

### 完成情况
- ✅ **建线程与搬家**（`setupAcquisition`）：`new ModbusTcpClient(cfg)`（**去掉了 parent**）→ `setParameters()` → `new QThread(this)` → `moveToThread()` → `start()`
- ✅ **跨线程命令通道**：`.h` 声明 `connectRequested` / `disconnectRequested` 两个信号；`.cpp` 里三条 connect，**接收者直接是 `m_client` 的槽**
- ✅ **`onConnectDevice()`**：`m_client->connectToDevice()` → `emit connectRequested()`
- ✅ **析构**：`if (m_acqThread) { quit(); wait(); }` —— 替换掉原来的 `= default`
- ✅ **`onDeviceDisconnected()` 补上 `m_pollTimer->stop()`**（D5 遗留的 TODO②，断开后定时器一直空转）
- ✅ **验收全绿**：`build.bat` **0 error**｜`semieq_tests` **18/0**｜`semieq_db_tests` **18/0**｜GUI 三条 —— 连接成功（状态栏「已连接 127.0.0.1:1502」）/ 控制台无 `QSocketNotifier` 报错 / **关窗口正常退出不 abort**

### 遇到的问题
1. **一开始想不通"为什么非要自己造两个信号"**：以为信号槽到处都能用
2. **中途差点写错**：想在 `MainWindow` 里加一个 `onConnectRequested()` 槽，在槽里再调 `m_client->connectToDevice()`
3. **审查发现 3 个边界**：`isConnected()/getEndpoint()` 仍从 UI 线程只读调用；`disconnectRequested` 目前没有 emit 点（菜单还没有「断开设备」项）；`onDeviceDisconnected` 少了 `stop()`

### 怎么解决
- **为什么必须自己造信号**：跨线程的调用需要一个「信号」当运输工具。`poll()` 有 `QTimer::timeout` 可搭，**不用加**；而「连接」的第二棒（`onConnectDevice` → 客户端）手上没有信号 —— 第一棒 `QAction::triggered` 已经在菜单接线那一步用掉了
- **为什么不能写 `MainWindow::onConnectRequested()`**：**槽在哪个对象里，就在那个对象所属的线程执行**。这个槽属于 `MainWindow`（UI 线程），排队终点还是 UI 线程，里面再调 socket 等于没搬 → 接收者必须是 `m_client`
- **只读查询不加锁**：`isConnected()` / `getEndpoint()` 调用极快且只读，**明确不加锁**（锁的代价大于问题），写进 README §8 已知限制

### 学到的
- **`moveToThread` 要求对象没有 parent**，否则会打印一行警告然后**静默失败**（程序照跑，只是还在原线程）—— 这种"不崩但没生效"的错误最难查
- **`terminate()` 不能用**：强杀可能让线程在持锁/写文件时被砍断 → 死锁或数据损坏。正确是 `quit()`（请求退出事件循环）+ `wait()`（阻塞等结束）
- **`QThread::finished → deleteLater`** 是客户端的正确回收方式：让对象在自己的线程里被删
- **`QTimer` 留在 UI 线程反而更好**：定时器只管"按节拍发命令"，真正耗时的 I/O 在工作线程，定时精度不受采集阻塞影响

### 明日计划
- 🔴 **先补数据库接线**（审查时发现）：`main.cpp` 建完库就把 `DatabaseManager` 丢了，`MainWindow` 拿不到它 → **曲线在跑、`parameter_log` 表是空的**。这是现在最大的"能演示但不能深问"的点
- 然后 **D7 报警引擎**：阈值判定 + 持续时间 + 状态栏/表格提示 + 落 `alarm` 表

---

## 2026-10-25（D6-1）· 每通道量化（scale）：把"真空度恒 0"真正修掉

### 今日目标
- 把 10/21 留下的「整数寄存器无缩放」从**已知限制**变成**已修复**：改为**每通道独立 scale**
- 顺带看完正点原子 P56（QThread 多线程），为 D6-2 线程化打底

### 完成情况
- ✅ **`types.h`**：`Parameter` 新增 `double scale = 1.0`（**工程量 = 原始寄存器值 × scale**）
- ✅ **模拟器**：新增 `registerScales()`（8 值）；量化从 `static_cast<quint16>(qBound(...))` 改为 **`qRound(工程量 / scale)`**，含 `sc > 0` 兜底
- ✅ **客户端**：`modbustcpclient.cpp` 解码处 `value = parseRes[i] * m_parameters[i].scale`
- ✅ **UI**：`defaultChannels()` 的 lambda 加第 6 参数，8 通道 scale 全部填对
- ✅ **文档**：`docs/协议说明.md` §3 映射表加 **scale 列 ＋ 寄存器满量程列**，新增 §3.1 量化规则与「**三处必须一致**」清单
- ✅ **验收**：温度显示 **45.xx**（原为整数 45）｜**真空度曲线有起伏**（原为贴地直线）｜`dbtest` **18 passed / 0 failed**
- ✅ **commit `04aa8c7`**；`BUGS.md` TODO-0003 标「已修」
- ✅ **学完 P56** ＋ 复述 ＋ 笔记 `笔记库/QT/09_Qt多线程.md` 落盘（第 4 条 `terminate()` 被判错 → 改 `quit()+wait()`）

### 遇到的问题
1. **README §8 写的「分辨率是全局 ×10」与代码不符** —— 代码里根本没有 ×10，是 `static_cast` **直接截断**
2. **量化因子该放在哪一层**：放 UI 层（`mainwindow`）会让"协议属性"漂到界面层；放数据模型（`Parameter`）才对
3. **验收靠眼睛，不靠编译**：编译过、测试过，但"真空度是不是一条直到底的线"只能看曲线 —— **这正好是 `编译过 ≠ 跑得对` 的又一次实例**
4. （过程记录）AI 侧一度把 `dbtest` 日志里的 `Driver not loaded` 当成回归，实际是 `insertParameterFailsWhenDatabaseClosed()` 的**预期输出**（QWARN + PASS）

### 怎么解决
- **量化属于协议，不属于 UI**：scale 随 `Parameter` 走，解码在客户端做，UI 只拿到已经是工程量的值
- **单一因子必然牺牲一端**：8 路量纲差 5 个数量级（真空度 0~1 kPa vs 转速 0~3000 rpm）→ 必须 per-channel
- **scale 选取原则**：让「量程上限 / scale」落在 **8k–16k counts** —— 既留足 16 位（65535）余量，又不浪费分辨率
- **截断 vs 四舍五入**：设备侧用 `qRound`，否则 0.6 kPa 会被算成 0（旧版就是这么丢的）

### 学到的
- **`static_cast<quint16>(double)` 是截断不是四舍五入** —— 小量程通道会被"量化吃光"；工业上靠 `scale`/`offset` 定标解决
- **量化因子是协议属性**：它必须和设备侧的量化因子**互为倒数**，所以它和寄存器映射表是同一层的东西
- **三处一致性靠人维护是隐患**：协议文档 / 模拟器 `registerScales()` / `defaultChannels()` 必须同步改（已在 BUGS 记 TODO-0004）
- **回归测试的真正用途**：这次改的是量化，`dbtest` 18/0 证明**持久层没被牵连** —— 测试不是"跑一遍求心安"，是"划定改动边界"

### 明日计划（D6-2 线程化）
- `m_client` 搬进工作线程：**`new ModbusTcpClient(cfg)`（不能带 parent）→ `setParameters()` → `moveToThread()` → `start()`**
- 跨线程只用「信号 → 槽」：`connectRequested/disconnectRequested`（`.h` 已声明）
- 三个必踩坑：带 parent 的 `moveToThread` **静默失败**／UI 线程直接调 `connectToDevice()` 报 `QSocketNotifier ... from another thread`／析构不 `quit()+wait()` 关窗口 abort
- 验收：UI 不卡 ＋ 8 曲线照动 ＋ 关窗口不崩；然后 README §8 划掉「采集在 UI 线程」

---

## 2026-10-21（D5 收尾）· 采集接线打通：8 条曲线第一次动起来

### 今日目标
- 完成 D5-10 的 6 处接线：`setupAcquisition` ＋ 5 个槽 ＋ 菜单改槽
- 目标：`run.bat sim` ＋ `run.bat monitor` → 看到实时曲线

### 完成情况
- ✅ **`setupAcquisition()`**：Config（127.0.0.1:1502 / slaveId 1 / registerCount 8）＋ `new ModbusTcpClient(cfg,this)` ＋ `setParameters(defaultChannels())` ＋ QTimer 500ms ＋ 5 条 connect
- ✅ **`onConnectDevice()`**：防抖用**新增的 `isConnecting()`**（不是 UI 层存标志）
- ✅ **`onDeviceConnected()`**：状态栏「已连接 127.0.0.1:1502」＋ `m_pollTimer->start()`
- ✅ **`onDeviceDisconnected()` / `onParametersRead()` / `onConnectionError()`**
- ✅ 菜单第 134 行 `showNotImplemented` → `onConnectDevice`
- ✅ **8 条曲线全部跑通**（温度 / 压力 / 流量 / 转速 / 功率 / 真空度 / 传送带 / 电流），网格、刻度、规格线、图例全部正常
- ⚠️ 真空度恒 0 → 定位为「整数寄存器无缩放」，**不是程序 bug**（见 BUG-0029）

### 遇到的问题（4 个，全部在"接口与数据表示"上）
1. `m_client->m_socket->state()` 编译失败：`m_socket` 是 **private**
2. `const QString& getEndpoint()` 返回 `QStringLiteral(...).arg()` 的临时对象 → **悬垂引用**，GCC `-Wreturn-local-addr` 警告 ×5
3. `QStringLiteral("正在连接%1:%2").arg(host, port)` → **第二参数被当成字段宽度**，日志出现 `127.0.0.1:%2` ＋ 1502 个空格
4. 真空度显示 `0.0 kPa`：模拟器 `static_cast<quint16>(double)` **截断小数**，0.6 → 0

### 怎么解决
- **封装**：`IDeviceClient` 加 `virtual bool isConnecting() const = 0`，实现读 `m_socket->state()` → UI 不碰内部成员
- **引用**：返回值类型去掉 `&`（按值返回，RVO 零拷贝）
- **arg**：改链式 `.arg(m_config.host).arg(m_config.port)`
- **量化**：模拟器 `value * 10.0`、客户端 `parseRes[i] / 10.0`（全局分辨率约定；待改 per-channel scale）

### 学到的
- **`const&` 只能返回成员 / 静态 / 入参引用**；现场算出来的值必须按值返回
- **`.arg(a, b)` 不是"插两个值"**：QString 版第二参是**字段宽度**，int 版是**进制**（10/18 讲过，今天又踩）
- **黑盒链路先加日志**：发送帧（十六进制 + 字节数）＋ 收到字节数 / 期望帧长 → 一次定位断点
- **单一真相（single source of truth）**：状态存在最了解它的地方（socket state 在客户端），UI 只查询
- **工程量 = 原始值 × 分辨率**：16 位寄存器传不了小数，工业上靠系数；量程小于 1 的通道（真空度）会被量化吃光

### 明日计划
- **10/23 14:00 招聘会**：A075 鼎匠 → A085 湖南航天捷诚 → A034 荣泰
- **D5 review**：`scale` 从全局 ×10 改成 per-channel
- 之后：QThread 线程化（采集搬出 UI 线程）→ D6

---

## 2026-10-19（D5 续）· 实时曲线绘图层：5 个绘制函数全部完成

### 今日目标
- 完成 `curvewidget.cpp` 剩下的 6 个函数：`plotArea` / `computeRange` / `drawGridAndAxes` / `drawCurves` / `drawLegend` / `paintEvent`
- 目标：屏幕上先看到网格与刻度，把"画"这一半做完，接线留给明天

### 完成情况
- ✅ **D5-4 `plotArea()`**：一行公式，四边留白 56 / 16 / 10 / 26（800×400 → `QRect(56,10,728,364)`）
- ✅ **D5-5 `computeRange()`**：判空 → 首值初始化 → 遍历求 min/max → 并入 LSL/USL → 防除零
- ✅ **D5-6 `drawGridAndAxes()`**：边框＋内部 3 横 3 竖＋左侧 5 个刻度；文字框高用 `fontMetrics().height()`，以 y 为中心
- ✅ **D5-7 `drawCurves()`**：深红虚线规格线（`(192,57,43)` · `Qt::DashLine` · 1px）＋ `QPolygonF` + `drawPolyline` 折线（通道色 2px · 抗锯齿）
- ✅ **D5-8 `drawLegend()`**：中文名＋最新值＋单位；半透明白底压网格＋通道色文字
- ✅ **D5-9 `paintEvent()`**：白底 → `plotArea()` → `computeRange()` → 调绘制函数（只装配，不画）
- ✅ 全过程 **build 0 error**（每改完一个函数编一次）
- ⏭️ **D5-10 接线（6 处）已梳理清楚**（Config / setParameters / 定时器 / 4 个基类信号 / 菜单改槽 / 状态栏），**待明日写**

### 遇到的问题（今天 7 个，全部出在"绘图"上）
1. 网格与刻度的 y 用了**数据公式** `maxValue - minValue` 算像素 → 换通道（如转速 0–3000）网格直接崩到画面外
2. 折线的 y 用了 `plot.top() + ratio * height` → **整张图上下颠倒**
3. 用 **`drawLines`** 画折线 → 点是**两两配对**的，画出来是一段一段的断线
4. **`i / m_capacity` 整数除法** → 前 299 个点 x 全相同，曲线缩成一条竖线
5. `Channel ch = m_channels[...]` **值拷贝**（同类错误第 3 次）
6. `values.last()` 未判空即调用 → 空容器越界；且用 `if (v)` 判断"有没有数据" —— **把"值 0"当成了"数据为空"**
7. `drawLegend` 没 `setPen` → 继承了上一个函数留下的 2px 通道色笔，字变粗

### 怎么解决
- **两套坐标分开**：画在哪用 `plot.height()/width()`；代表多少用 `maxValue - minValue`
- 屏幕 y 向下 → `y = plot.bottom() - ratio * height`
- 折线一律 **`drawPolyline`**（`Poly`＝多，连成一条）
- 除法前先 `(double)` 转换
- 遍历一律 `const T&`
- **判"有没有数据"看容器 `isEmpty()`，不看值**
- **每个绘制函数自己 `setPen`**，不依赖上一个函数的状态

### 学到的
- **`QRect` 是像素矩形**：`right() = left + width - 1`、`bottom() = top + height - 1`（对比 `QRectF` 无 -1）
- **`QFontMetrics`**：`horizontalAdvance(text)` 量文字宽（`width()` 已废弃）、`height()` 是行高
- **`arg` 的格式化重载**：`.arg(double, fieldWidth, fmt, prec)`；而 `int` 版第三参是**进制**——两版含义不同，极易错
- **`const` 成员函数是"承诺"**：声明 const → `this` 是 const 指针 → 成员只能拿 `const&`；要改成员的 `appendSamples` 就不能声明 const
- **正确 ≠ 好**：`y - height/8` 配 `height/4` 恰好对上中心，但把正确性寄托在"两个数成 2:1"上；改成 `y - textH/2` 才"显然正确"

### 明日计划
- **10:00 打大族校招专线 `0755-8663 2722`**（问领导签字节点）
- **D5-10 接线 6 处** → `run.bat sim` ＋ `run.bat monitor` → **曲线动起来**（今晚唯一没闭环的）
- 改 D5 review 意见 → QThread 线程化开工
- 求职：10/21 邦正答复 ｜ 10/22 向奥普特申请延期到 10/30 ｜ 10/23 招聘会（鼎匠优先）

---

## 2026-10-18（D5）· 实时曲线控件：任务卡落地 ＋ 数据层三函数

### 今日目标
- **先读懂任务卡再动手**：8 个通道到底是什么、数据从哪来、`QPolygonF/drawPolyline` 怎么命名、`connect` 的信号/槽是什么
- 实现 `curvewidget.cpp` 的数据层三函数：`setChannels()` / `appendSamples()` / `channelColor()`
- 目标：`scripts\build.bat` → 0 error

### 完成情况
- ✅ **任务卡补三处解释**（`docs/tasks/D5_实时曲线.md`）：§2.0 八个通道表＋数据形态＋uint16 截断坑；§3.1 `QPolygonF` / `drawPolyline` 命名拆解；§3.2 `connect` 四要素与信号/槽运行时序
- ✅ **D5-1 `setChannels`**：清空 → 遍历 `definitions` 建 `Channel`（5 个字段 ＋ `color = channelColor(m_channels.size())`，**下标必须在 `append` 之前取**）→ `update()`
- ✅ **D5-2 `appendSamples`**：外层样本／内层通道按 `code` 匹配 → `values.append` → `while (size > m_capacity) removeFirst()` 裁剪 → `isWrote` → **循环外**只 `update()` 一次
- ✅ **D5-3 `channelColor`**：`switch (index % 8)` 8 色 ＋ `default` 深灰
- ✅ **编译 `[OK] Build succeeded`**
- 🔍 附带发现并记录 **TODO-0003**：Modbus 保持寄存器是 **uint16**，模拟器 `static_cast<quint16>(qBound(0.0, value, 65535.0))` **直接截断小数** → 真空度 0.6 → 0，该通道曲线永远是平线

### 遇到的问题（3 个，每个都卡了一轮）
1. **`QVector::at(i)` 返回 `const T&`** → 拿返回值调 `append()` 直接编译失败：
   `error C++(1087) 没有与参数列表匹配的重载函数 … 对象包含阻止匹配的类型限定符`
2. **range-for 写成值拷贝** `for (Channel channel : m_channels)` → 数据 append 到**栈上临时副本**，循环结束即销毁。**能编过，但曲线永远没有数据**（比编译错更难查）
3. **裁剪对象写错**：`while (m_channels.capacity() > m_capacity) m_channels.removeFirst();` —— 拿"通道表的容量"当"点数"，删的是**通道**不是**数据**；实际条件恒为 false（8 > 300），裁剪一行都没生效

### 怎么解决
- `at(i)` / `first()` / `last()` 都是只读 → **要写就用 `m_channels[i]`**
- **要改容器元素，range-for 必须写引用**：`for (Channel &channel : ...)`
- 要裁的是"这个通道存了几个点"：`while (channel.values.size() > m_capacity) channel.values.removeFirst();`
- 顺手删掉流程里的 `LOG_INFO`（每秒 16 条会写爆日志），`update()` 从内层循环提到循环外

### 学到的
- **`QVector::capacity()` ≠ `size()`**：capacity 是预留的内存槽位数（与数据多少无关），size 才是元素个数
- **"能编译过" ≠ "跑得对"**：值拷贝这类错编译器不会报，只能靠"数据有没有真的进去"验证
- `QString::arg(i, value)` 的第二个参数是**最小字段宽度**，不是第二个值（要链式 `.arg(i).arg(v)`）
- 数据层的正确性**无法靠肉眼确认** —— 必须等 D5-7 画出来 / D5-10 接线后看曲线动没动

### 明日计划
- D5-4/5/6：`plotArea()` / `computeRange()`（**真空度通道 min == max，会真实触发除零**）/ `drawGridAndAxes()`
- D5-7/8/9：规格虚线＋折线（`QPolygonF` + `drawPolyline` + 抗锯齿）/ 图例 / `paintEvent` 组装
- D5-10 接线 → D5-11 联调（`sim` + `monitor` → **曲线在动**）
- 求职线：**周一 10:30 催大族**（本周唯一能改变结果的动作）

---

## 2026-10-11 · 数据库初始化接线（commit `0551756`）

> 📌 本条按提交记录回补（10/11 当日未写）

### 今日目标
- 把持久层接进 `main.cpp`：启动时 `open(defaultDatabasePath())` → `initSchema()`，让程序**首次真实建库**

### 完成情况
- ✅ `src/main.cpp` 新增 10 行接线（commit `0551756`）
- ✅ 首跑 `run.bat monitor` 后 `build/bin/data/semieq.db` 生成，实测 **6 表 3 索引**
- ✅ D4 完整链路闭环（建库 → 读写 → 测试）

### 学到的
- **"接线"本身没几行，但它证明整条链路是通的**——之前 dbtest 全绿只是"函数对"，建库成功才是"系统对"

### 明日计划
- 进入 D5：实时曲线（P36–P38 绘图）

---

## 2026-10-10 · D4-6~9 四个持久层函数（commit `fcbbaee`）

> 📌 本条按提交记录回补

### 今日目标
- 实现 `insertParameter` / `insertAlarm` / `recentParameters` / `activeAlarms` 四个函数
- 目标：`dbtest` 18/18 全绿

### 完成情况
- ✅ `databasemanager.cpp` +116/-9（commit `fcbbaee`）
- ✅ **`dbtest` 18 passed / 0 failed**
- ✅ 关键点：**`QSqlQuery` 预编译 + `addBindValue` 按列顺序**；**NULL 值用 `QVariant(QVariant::String)` 显式绑**；时间字段走 **`QDateTime::fromString(串, Qt::ISODate)`**

### 遇到的问题
1. **NULL 绑定**：空字符串 ≠ NULL——想写"没有值"，必须绑**空的 `QVariant`**，否则列里存的是 `""`
2. **时间格式**：字符串直接塞会被 SQLite 当普通文本比较；统一 **ISODate** 才能正确排序/筛选

### 学到的
- 持久层"能存进去"和"查得对"是两件事——**测试要覆盖 NULL / 时间边界**

### 明日计划
- `main.cpp` 接线（次日 `0551756`）

---

## 2026-10-09 · SQLite 持久层单元测试 18 用例（commit `b742951`）

> 📌 本条按提交记录回补

### 今日目标
- 给持久层补测试：**18 个用例** + CMake 的 `dbtest` 目标 + `run.bat dbtest` 子命令

### 完成情况
- ✅ 新增 `tests/test_database.cpp`（388 行，commit `b742951`）
- ✅ `CMakeLists.txt` +7（dbtest 目标）；`scripts/run.bat` +9/-1（新增 dbtest 分支）
- ✅ 18 用例覆盖：建表/幂等 initSchema/参数插入/报警插入/最近 N 条/活动报警筛选

### 学到的
- **测试文件比实现文件还长是正常的**——用例要覆盖"正常/边界/空数据"
- 用**独立测试库**跑用例（不碰业务库），跑完清理——避免污染

### 明日计划
- 补齐四个业务函数（次日 `fcbbaee`）

---

## 2026-10-03 · SQLite 持久层落地 + .gitignore 事故（commit `b27fb95`）

> 📌 本条按提交记录回补

### 今日目标
- 实现 `DatabaseManager`：建表 / 打开 / 关闭 / `initSchema`（含事务）
- 修掉 `.gitignore` 误忽略 `src/data/` 的事故

### 完成情况
- ✅ `src/data/databasemanager.cpp`（328 行）+ `.h`（73 行）新建（commit `b27fb95`）
- ✅ `initSchema` 用**事务**包裹六张表 + 索引，失败回滚
- ✅ 修复 `.gitignore`（7 行改动）

### 遇到的问题
1. 🔴 **`.gitignore` 写 `data/` 会吞掉 `src/data/`**——源码目录被忽略，`git add` 根本加不进去
   → 改成 **`/data/`**（只忽略根目录），改完 `git check-ignore -v <文件>` 验证
2. **建表要幂等**：`CREATE TABLE IF NOT EXISTS` + 事务，重复启动不报错

### 学到的
- **`.gitignore` 的 `dir/` 是"任意层级匹配"**——只忽略根目录必须写 `/dir/`（`log/`、`bin/` 同理）
- 建库/建表这类"一次性动作"要能**反复执行不炸**（幂等）

### 明日计划
- 补单元测试（10/9 `b742951`）

---

## 2026-09-30（D3）· 轮询采集：`poll()` ＋ `onReadyRead()` 粘包拆包

### 今日目标
- 实现 `modbustcpclient.cpp` 的采集部分 2 个函数：`poll()`（组帧发送）＋ `onReadyRead()`（收包拆包解析）
- 目标：编译 0 error，数据链路"上位机问 → 模拟器答 → 解析成寄存器值"能跑通

### 完成情况
- ✅ 2 个函数实现（`modbustcpclient.cpp` **+52 行**），编译 **0 error / 0 warning**
- ✅ `poll()` 4 步齐：判连接 → `m_transactionId` 自增 → 组帧 → `write()`；**并加上了"上一帧未回就跳过"的保护**
- ✅ `onReadyRead()` 5 步齐：收 → 判长度 → **`left()` 切帧 + `remove()` 删原件** → 解析 → 按序配对 `m_parameters` + 时间戳 → `emit parametersRead()`
- ✅ **两轮批改**（首轮 5 个问题，第 2 轮全部改对）
- 📌 求职线今日另跑**第 29 轮流水线**（六步全完成，详见 `.workbuddy/memory/2026-09-30.md`）

### 遇到的问题

**问题 1：拆包只做了一半 —— `left()` 取了帧，却没把原件从缓冲里删掉** ⭐

```cpp
temp = m_rxBuffer.left(length);   // ❌ 只"复制"出前 length 字节，原缓冲一动不动
```

**后果**（这是 TCP 新手最典型的坑）：
```
第 1 次 readyRead：缓冲=[帧A]        → left(13) 取到帧A ✅
第 2 次 readyRead：缓冲=[帧A 帧B]    → left(13) 又取到【帧A】❌
第 3 次 readyRead：缓冲=[帧A 帧B 帧C] → left(13) 还是【帧A】❌
```
→ **永远在解析第一帧**，新数据全堆在后面解析不到；而且 `size()` 永远 ≥ `length`，那句 `return` 也永远不触发。

**怎么解决**：`left()` 后面紧跟 `m_rxBuffer.remove(0, length);`

**学到的**：**拆包 ＝ 两个动作**：「**切一份出来**」＋「**把原件删掉**」，少一个都不行。
`left()` 是**只读**的（返回副本），`remove()` 才是**改动**缓冲的那个。

---

**问题 2：用 `new` 造传出参数 —— 内存泄漏 ＋ 未初始化** ⭐

```cpp
bool *ok = new bool;              // ❌ ① 没有 delete → 每收一帧泄漏一次
QString *error = new QString;     //    ② new bool 不做初始化 → *ok 是随机值!
```

**后果**：第 ② 条比泄漏更危险 —— `*ok` 如果是随机非零值，**解析失败会被当成成功**，错误数据一路流下去。

**怎么解决**：传出参数要的是「**变量的地址**」，不是堆对象：

```cpp
bool    ok = false;            // 栈变量，初始化好
QString error;
QVector<quint16> parseRes = parseReadHoldingRegistersResponse(temp, &ok, &error);
```

**学到的**：**看到 `T *传出参数`，就给它一个栈变量的地址**（`&ok`）——
既不泄漏、又天然初始化、还不用手动释放。**`new` 只在"对象需要活过当前作用域"时才用。**

---

**问题 3：提前 `return` 时忘了清状态 → 一次脏数据让采集永久停摆** ⭐⭐（本日最有价值）

```cpp
if (!ok) {
    LOG_ERROR("ModbusTcpClient", error);
    return;                       // ❌ m_waitingResponse 还停在 true
}
```

**后果**：
```
设备回一帧脏数据 → 解析失败 → m_waitingResponse 停在 true
   ↓
下一次 poll()：if (m_waitingResponse) return;   ← 被挡住
   ↓
再下一次：还是被挡住…… → 【采集永久停摆，界面上毫无提示】
```

**怎么解决**：`return` 前补 `m_waitingResponse = false;`

**学到的**：
- **这是"状态残留"的第二例**（D2 那次是"断开时不清"）—— **同一类问题，一个月内踩了两次**
- 📌 **通用规律**：函数里**每一条 `return` 路径**都要问一句「**我进来时改过的状态，清理干净了吗？**」
- 📌 更狠的写法是用 **RAII/守卫**（构造时置位、析构时恢复），但现阶段**逐个 return 检查**就够

---

**问题 4：`m_waitingResponse` 从没被设为 `true` —— 标志位形同虚设**

`poll()` 里加了 `if (m_waitingResponse) return;`，但**忘了在 `write()` 前 `m_waitingResponse = true;`** →
这个标志永远是 `false`，保护逻辑**完全不生效**（而且 `onDisconnected()` 里那句清理也失去意义）。

**怎么解决**：`poll()` 里"发出请求"和"置位"要**成对出现**。

**学到的**：**"加了一个标志位" ≠ "保护生效了"** —— 置位 / 判断 / 清除，**三处都要有**，漏一处整条逻辑就是装饰。

---

**问题 5：`emit parametersRead(...)` 漏了**

TODO ⑤ 明确要求把填好值的参数列表发出去，但只写了循环里的 `qDebug`，**没发信号** → 上层（界面/数据库）永远收不到数据。

**学到的**：**写完一个"数据出口"函数，回读一遍它的 TODO 清单**——`poll`/`onReadyRead` 这种"骨架里列了 5 步"的地方，**逐条对勾**比通读一遍更可靠。

### 明日计划（10/1）
- **D4**：`databasemanager.cpp` 9 个函数（已看完两个短课 `BV1mw411M7nw` / `BV1gk4y1w7zy`）
- 项目收尾：README ＋ 截图 ＋ 联调截图 → **项目进简历**
- ⚠️ **10/1–10/2 必须投 #11 联讯仪器**（10/03 截止）

---

## 2026-09-29（D2 第 2 步）· TCP 连接：5 个函数 ＋ 连接超时

### 今日目标
- 实现 `modbustcpclient.cpp` 的**连接部分 5 个函数**：`connectToDevice` / `disconnectFromDevice` / `onConnected` / `onDisconnected` / `onSocketError`
- 目标：`scripts\run.bat tcptest` 尽量全绿（起点 2 passed / 3 failed）

### 完成情况
- ✅ 5 个函数全部实现（`modbustcpclient.cpp` **+49 行**、`.h` **+2 行**），编译 **0 error / 0 warning**
- ✅ **`tcptest`：4 passed / 1 failed**（起点 2/3）
  - `connectsSuccessfully` ✅ 能连上
  - `emitsErrorWhenPortClosed` ✅ 连不上能报出原因
  - `disconnectEmitsDisconnected` ❌ 失败原因＝**测试自身问题**（见"问题 4"）
- ✅ 自己实现了**连接超时**：`QTimer` ＋ `setSingleShot(true)` ＋ `m_config.timeoutMs`
- ✅ 摸清了 **TCP 客户端的三层**：`QTcpSocket`(网络层) → `ModbusTcpClient`(采集层，**转发信号**) → `MainWindow`(界面层)

### 遇到的问题

**问题 1：`#include` 刚学完，`QTimer` 又踩同一类坑**

`m_connectTimer` 在 `.h`/`.cpp` 都没提前声明 → 编译报 5 个"未声明"：

```
error: 'm_connectTimer' was not declared in this scope
error: expected type-specifier before 'QTimer'
error: 'QTimer' has not been declared
```

**怎么解决**：`.h` 加 `#include <QTimer>`（或前置声明 `class QTimer;`）＋ 成员声明 `QTimer *m_connectTimer = nullptr;`；`.cpp` 也补 `#include <QTimer>`。

**学到的**（一条规律管两种情形）：
- **只当"指针"存着** → 前置声明就够（写在 `.h`）
- **要 `new` / 调成员 / 取 `&类::成员函数`** → 必须 include **完整定义**
- **谁真正用到，谁自己 include** —— 别依赖"传递包含"（别人的头文件帮你带进来）。哪天上游头文件删了那行，你的文件会莫名编译不过，且报错点在下游。
- 📌 这和 D6 的 `QMenuBar` 是**同一个知识点的两个方向**：那次是"该 include 没 include"，这次是"该声明没声明"。

---

**问题 2：`abort()` 不触发 `error` 信号 —— 超时报错链路是断的**

超时后调 `m_socket->abort()`，以为它会像"连接失败"那样触发 `error` → `onSocketError()` → `emit connectionError()`。
实测：**`abort()` 只会触发 `disconnected()`，不会触发 `error()`** → 上层**永远收不到"连接超时"的提示**。

**怎么解决**：超时的 lambda 里**自己发**通知：
```cpp
m_socket->abort();
const QString reason = QStringLiteral("连接超时（%1 ms）").arg(m_config.timeoutMs);
LOG_ERROR("ModbusTcpClient", reason);
emit connectionError(reason);        // ← 别指望 abort 帮你触发 error
```

**学到的**：**`abort()` / `disconnectFromHost()` 的语义是"关连接"，不是"报错"** ——
"出错了"这个语义**必须由你自己 `emit`**。改完后 `emitsErrorWhenPortClosed` 立刻变绿。

---

**问题 3：`QTimer` 默认是"循环触发"，不是"只触发一次"**

一开始以为 `start(1000)` = "1 秒后触发一次"。写了个最小程序实测：

```
timer.start(300)   →  2 秒内触发了 6 次
```

→ **默认是周期性**的。要"只触发一次"必须 `setSingleShot(true)`。

**学到的**：Qt 里"定时"的**默认语义是周期**（模拟器刷新用的就是周期）；
**"超时"属于一次性的**，必须显式声明 —— 否则 1 秒后它会**每隔 1 秒都 abort 一次**。

---

**问题 4：`disconnectEmitsDisconnected` 测试失败（**测试自身的问题**）**

现象：`disconnectFromDevice()` 之后 3 秒内没等到 `disconnected` 信号。

**排查过程**（关键的一步：**先怀疑测试，而不是先怀疑代码**）：
1. 日志里明明有 `[modbusTcpClient] 连接已断开` → **说明 `onDisconnected()` 执行了，`emit disconnected()` 也走了**
2. 于是写了个**独立小程序**（不经过 QtTest）复现"连 → 断"：
   ```
   connected    收到？ 是
   disconnected 收到？ 是        ← 都正常!
   ```
3. → **同一份代码，单独跑就过、在测试里跑就失败** → 结论：**测试之间的相互干扰**

**根因**：三条测试**共用同一个 `QTcpServer`**（`m_server` 是测试类的成员，只在 `initTestCase` 里 listen 一次），
前序测试留下的连接/残留状态影响了后面这条。

**怎么解决**：**改成每条测试自己起一个 server**（或用完彻底清理）—— 属"测试完善"，放到下一天。

**学到的**：
- **"单独能跑、连起来跑就挂" 是典型的"测试间共享状态"信号**
- **先证伪自己的代码**（独立最小复现）**再改测试** —— 顺序反了就会去改本来正确的代码

### 明日计划（9/30）
- 修 `disconnectEmitsDisconnected`：测试改为**每条独立 server**
- 跑 `run.bat test` 确认 D2 第 1 步的 **18 passed** 没被破坏
- **D3**：`poll()` 组帧发送 ＋ `onReadyRead()` 粘包拆包（三步法）
- 之后：D4 数据库 ＋ README/截图

---

## 2026-09-28（D6）· 主窗口界面：菜单栏 / 中央区域 / 状态栏 / 关于框

### 今日目标
- 实现 `src/ui/mainwindow.cpp` 的 5 个函数：`setupMenuBar` / `setupCentralArea` / `setupStatusBar` / `showAbout` / `showNotImplemented`
- 目标：编译 0 error，窗口能跑出"**可运行的骨架**"

### 完成情况
- ✅ 5 个函数全部实现（文件 **106 → 171 行**），编译 **0 error / 0 warning**
- ✅ 手工联调通过：
  - 菜单栏三个菜单（**文件/视图/帮助**）+ 助记符（`Alt+F` / `Alt+V` / `Alt+H`）+ 分隔线
  - 快捷键 **Ctrl+K**（连接设备）/ **Ctrl+Q**（退出，实测可关窗）
  - 状态栏**左**「未连接」（`addWidget`）+ **右**版本号 `v0.1.0`（`addPermanentWidget`）
  - 中央区域占位标签；「帮助 → 关于」**弹框**；「关于 Qt」调 Qt 自带；未实现项 → **状态栏临时提示**
- ✅ 顺带为 D2 第 2 步铺好自动化验收口：新增 `tests/test_tcp_connect.cpp` + `scripts\run.bat tcptest`

### 遇到的问题

**问题 1：`invalid use of incomplete type 'class QMenuBar'`**

`mainwindow.h` 只 include 了 `<QMainWindow>`，而 `qmainwindow.h:55` 对 `QMenuBar` 只写了一句
`class QMenuBar;`（**前置声明**）→ 所以 `mBar->addMenu(...)` 编不过：

```
error: invalid use of incomplete type 'class QMenuBar'
note: forward declaration of 'class QMenuBar'
    class QMenuBar;
```

**怎么解决**：在 `mainwindow.cpp` 里补 `#include <QMenuBar>`。

**学到的**：**前置声明只够"当指针用"（`QMenuBar *p;`）；一旦要 `p->成员`，就必须 include 完整定义。**
报错里出现 `incomplete type` / `forward declaration`，第一反应就应该是"缺 include"。

---

**问题 2：三个成员变量没 `new` 就加进了布局 / 状态栏**

`m_placeholder` / `m_connStatus` / `m_versionLabel` 在头文件里是 `= nullptr` 初始化，
直接 `vLayout->addWidget(m_placeholder)` → **编译能过、运行也不崩**，
但界面里**中央一片空白、状态栏什么都没有**。

**怎么解决**：先 `new QLabel(...)` 再 `addWidget` / `addPermanentWidget`。

**学到的**：Qt 对 `addWidget(nullptr)` 是**静默忽略**（本机实测：不崩、不报警告、不写日志）。
→ **"不崩" ≠ "对"**。这类错**编译和单测都抓不到**，只能"跑起来看界面"发现。

---

**问题 3：`QKeySequence::Quit` 在 Windows 上解析为空**

原以为用 Qt 预定义的 `QKeySequence::Quit` 就能给"退出"设快捷键。本机实测（Qt 5.14.2）：

```
Quit          -> （空）        ← 等于什么都没设
Cancel        -> Esc
Save          -> Ctrl+S
New / Open    -> Ctrl+N / Ctrl+O
Close         -> Ctrl+F4
HelpContents  -> F1
```

`Quit` 是 **macOS 的 `Cmd+Q`**，Windows/Linux 上没有对应标准键 → 返回空串。

**怎么解决**：写死 `QKeySequence(QStringLiteral("Ctrl+Q"))`。

**学到的**：**"编译通过" ≠ "功能有效"** —— 枚举存在所以编译期毫无提示，只有跑起来才知道没生效。
**键位 / 路径 / 配置这类东西必须实跑验证**，不能只看编译过。

---

**问题 4：`connect` 把 action 连错了槽**

`actQuit`（退出）和 `actAboutSemi`（关于）一开始**都连到了 `showNotImplemented()`** →
结果：点「退出」不退出、点「关于」不弹框。

**怎么解决**：分别改成 `&QWidget::close`、`&MainWindow::showAbout`。

**学到的**：**connect 的目标槽要逐个对一遍**。10 个 action 里连错 2 个，
**编译期完全看不出来**（信号槽是运行期绑定）—— 只能一个一个点。

### 明日计划（9/29）
- **D2 第 2 步**：读官方示例 `fortuneclient` + 正点原子 P59 + 阿西拜 P83 →
  写连接部分 5 个函数（`connectToDevice` / `disconnectFromDevice` / `onConnected` / `onDisconnected` / `onSocketError`）
  → `scripts\run.bat tcptest` 三条全绿
- 然后 **D3 采集**（`QTimer` 轮询 + `onReadyRead()` 粘包拆包）

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
