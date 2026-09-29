# 缺陷记录 BUGS

> 记录开发过程中发现并修复的缺陷。
> 格式：**编号 / 发现日期 / 现象 / 复现步骤 / 根因 / 修复 / 回归验证**。
> 目的：一方面防止同一问题复发，另一方面为面试提供"调试过程"的真实素材。

| 编号 | 发现 | 严重度 | 状态 | 一句话 |
|---|---|---|---|---|
| BUG-0001 | 2026-09-15 | 阻塞 | 已修复 | `moc.exe` 无法处理含中文的项目路径，导致构建失败 |
| BUG-0002 | 2026-09-15 | 阻塞 | 已修复 | `configure_file` 生成头文件目录层级错误，`#include "common/version.h"` 找不到 |
| BUG-0003 | 2026-09-15 | 高 | 已修复 | 设备模拟器三目运算符优先级写错，温度始终为故障值 |
| BUG-0004 | 2026-09-15 | 阻塞 | 已验证规避 | **分支名带斜杠时 Git 静默失败**，`checkout -b` 还会把 HEAD 弄成悬空 |
| BUG-0005 | 2026-09-28 | 高 | 已修复 | `mainwindow.cpp` 少 include `<QMenuBar>`，只拿到前置声明 → `incomplete type` 编译失败 |
| BUG-0006 | 2026-09-28 | 中 | 已修复 | 三个 `QLabel*` 成员未 `new` 就加进布局/状态栏 → Qt **静默忽略**，界面空白（不崩不报错） |
| BUG-0007 | 2026-09-28 | 高 | 已修复 | `QKeySequence::Quit` 在 **Windows 上解析为空** → 「退出」快捷键等于没设 |
| BUG-0008 | 2026-09-28 | 阻塞 | 已修复 | `CMakeLists.txt` 首行被误粘贴文本 → `Parse error. Expected a command name` |
| BUG-0009 | 2026-09-29 | 高 | 已修复 | `QTimer *m_connectTimer` 未提前声明 ＋ 缺 `#include <QTimer>` → 5 个"未声明"编译错 |
| BUG-0010 | 2026-09-29 | 高 | 已修复 | **`abort()` 不触发 `error` 信号** → 连接超时后上层收不到任何通知（报错链路断） |
| BUG-0011 | 2026-09-29 | 中 | 已修复 | 测试假设"连不上＝连接被拒"，**Windows 实际是超时（~21 秒）** → 测试必然超时失败 |
| BUG-0012 | 2026-09-29 | 低 | 待修 | 三条集成测试**共用同一个 `QTcpServer`**，前序测试的残留状态干扰后续测试 |

---

## 环境坑（非本项目代码缺陷，但会拦住你）

### BUG-0004 · 分支名含 `/` 时 Git 静默失败（**2026-09-22 已定位：仅发生在沙箱化执行环境**）

- **现象**：`git branch ref/skeleton` 与 `git checkout -b feat/x` **退出码均为 0、无任何输出**，
  但 `git branch -a` 里根本没有这个分支。
- **最危险的一幕**：`git checkout -b feat/x 684a10b` 打印了
  `Switched to a new branch 'feat/x'`，然后把 `HEAD` 写成了 `ref: refs/heads/feat/x`
  —— 而那个 ref 从未被创建。结果是：
  ```
  $ git status        → fatal: ambiguous argument 'HEAD': unknown revision
  $ git rev-parse HEAD → HEAD （解析不出来）
  ```
  仓库进入**悬空 HEAD** 状态，且索引被那次 checkout 污染（两个源文件被回退到旧版本，
  `git status` 却显示为已暂存修改）。
- **复现**（本机 Git for Windows 2.54.0 + Git Bash）：
  ```bash
  git branch tb_plain   684a10b   # ✅ 建出来了
  git branch tb_slash/x 684a10b   # ❌ 建不出来（rc=0 但不存在）
  git update-ref refs/heads/feat/one 684a10b   # ❌ 同样静默失败
  ```
- **排查过程**：一度怀疑是权限或文件系统不支持嵌套目录，于是手工验证
  `mkdir -p .git/refs/heads/zz && echo <sha> > .git/refs/heads/zz/one`
  —— **文件系统完全支持，`git branch -a` 也能识别 `zz/one`**。
  所以问题出在 Git 自己创建 ref 的写入路径上。
- ✅ **2026-09-22 定位结论**：根因**不是本机、也不是 Git 本身**，而是**沙箱化的执行环境会
  静默丢弃 `.git/refs/` 下 ≥4 层深路径的写入**。同一台机器上的对比判据：
  - 普通终端：`mkdir -p .git/refs/remotes/origin` → 目录**持久存在**，`Test-Path` 为 True
  - 沙箱环境：`mkdir -p` 与 `git update-ref refs/remotes/origin/main <sha>` **退出码全为 0**，
    但目录 / 文件**根本不存在**；`Test-Path` 当场为 True，**下一次执行即消失**
  - 同源现象：`git fetch` 打印 `* [new branch] main -> origin/main`，却永远建不出 `origin/main`
  → **普通本机终端没有这个限制，斜杠分支名（`feat/xxx`）可以正常使用。**
- **修复 / 规避**：
  1. **回读验证**：任何"建分支 / 建 ref / fetch"之后，用 `git branch -a` / `git show-ref`
     确认结果**真的存在**（沙箱环境里会"假成功"）。
  2. **需要写 `refs/remotes/**` 的命令（`git fetch` / `git push -u`）在普通终端执行**；
     沙箱环境只用来读 —— 看远端用 `git ls-remote`，取文件用 `FETCH_HEAD`。
  3. 万一又把 HEAD 弄悬空，一行修复：
     ```bash
     printf "ref: refs/heads/main\n" > .git/HEAD     # 把 HEAD 指回真实分支
     git reset --hard HEAD                            # 索引被污染时一并修正
     ```
- **回归验证**：2026-09-22 在普通终端执行 `git push -u origin main`，成功创建了
  `refs/remotes/origin/main`（同样是 4 层嵌套）→ 反证"本机不支持嵌套 ref"的结论不成立。
- 📌 **教训（两条）**：
  1. **退出码为 0 ≠ 操作成功** —— 命令返回成功时必须再验证一次结果
     （`git branch -a`、`ls`、读回内容），否则会带着错误状态继续往下走。
  2. **结论必须标注它的前提环境** —— 这次的误判，就是把"沙箱环境的限制"当成了
     "本机 / Git 的限制"，差点让项目长期放弃斜杠分支名这种完全正常的写法。

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

## BUG-0005 · 只有前置声明 → `incomplete type`（D6）

- **现象**：`mainwindow.cpp` 写完菜单栏第一行后编译失败：
  ```
  error: invalid use of incomplete type 'class QMenuBar'
       QMenu *fileMenu = mBar->addMenu("文件");
  note: forward declaration of 'class QMenuBar'
   class QMenuBar;
  ```
  同时编辑器（VS Code / IntelliSense）**也不给 `mBar->` 的成员提示**，只有红色波浪线。
- **复现**：只 `#include "ui/mainwindow.h"`（它只 include `<QMainWindow>`），然后 `mBar->addMenu(...)`。
- **根因**：`QMainWindow` 里 `QMenuBar` 是**当指针用**的（`QMenuBar *menuBar() const;`），
  所以 `qmainwindow.h:55` 只写了一句**前置声明** `class QMenuBar;` 就够它自己用 ——
  但**调用方要 `->成员`，必须看到完整定义**。
- **修复**：在 `mainwindow.cpp` 顶部补 `#include <QMenuBar>`（它同时带来 `QMenu`/`QAction`/`QKeySequence`/`QWidget`）。
- **回归验证**：补 include 后编译通过；**编辑器的成员提示也同时恢复了**
  —— 因为 IntelliSense 也只是"看到完整定义才能列成员"。
- 📌 **教训**：报错出现 `incomplete type` / `forward declaration` → **先想"缺 include"**。
  这类报错的"没有代码提示"和"编译失败"是**同一个原因**，不是编辑器的问题。

---

## BUG-0006 · `nullptr` 加进布局/状态栏被静默忽略（D6）

- **现象**：`addWidget(nullptr)` 后界面**中央空白、状态栏什么都没有**，
  但**编译 0 error、运行不崩、不报警告**，日志里也没有任何线索。
- **复现步骤**（修复前）：
  ```cpp
  // 头文件里：QLabel *m_placeholder = nullptr;  ...
  vLayout->addWidget(m_placeholder);        // m_placeholder 从未 new
  sBar->addWidget(m_connStatus);            // 同上
  sBar->addPermanentWidget(m_versionLabel); // 同上
  ```
- **根因**：三个成员变量**只在头文件里声明并初始化为 `nullptr`，从未创建实例**。
  而 Qt 对"空指针"的输入是**静默忽略**（本机实测 `QLayout::addWidget(nullptr)`
  和 `QStatusBar::addWidget(nullptr)` 都不崩、不报错、不写警告）。
- **修复**：先 `new QLabel(...)` 再 add：
  ```cpp
  m_placeholder = new QLabel(QStringLiteral("下一步：…"), widget);
  vLayout->addWidget(m_placeholder);
  ```
- **回归验证**：重新编译 + `run.bat monitor` → 中央出现占位文字、状态栏左「未连接」右版本号。
- 📌 **教训**：**"不崩" ≠ "对"**。Qt 很多 API 对 `nullptr` 是"宽容"的（悄悄跳过），
  所以 **编译和单元测试都抓不到**，只能"跑起来看界面"。写完带 UI 的代码**必须实际看一眼**。

---

## BUG-0007 · `QKeySequence::Quit` 在 Windows 上是空键（D6）

- **现象**：给「退出」菜单项设了 `actQuit->setShortcut(QKeySequence::Quit);`，
  编译通过、程序正常，但**按什么键都触发不了退出**（菜单项右侧也不显示快捷键）。
- **复现**：任何平台下打印
  ```cpp
  qDebug() << QKeySequence(QKeySequence::Quit).toString();
  ```
- **根因**：`QKeySequence::StandardKey` 是**跨平台预定义**，而 `Quit` 只有 **macOS 有定义（`Cmd+Q`）**；
  在 Windows / Linux 上**解析结果为空字符串**。
  本机（Qt 5.14.2 / Windows）实测对照：
  ```
  Quit          -> （空）         ← 等于没设
  Cancel        -> Esc            ← Esc 是这个，不是 Quit
  Save / New / Open -> Ctrl+S / Ctrl+N / Ctrl+O
  Close         -> Ctrl+F4
  HelpContents  -> F1
  ```
- **修复**：改用**显式键位字符串**：`actQuit->setShortcut(QKeySequence(QStringLiteral("Ctrl+Q")));`
- **回归验证**：设完后按 Ctrl+Q 能关窗；菜单项右侧显示 `Ctrl+Q`。
- 📌 **教训（两条）**：
  1. **"编译通过" ≠ "功能有效"** —— 枚举存在所以编译期毫无提示，只有实跑才知道没生效。
     **键位 / 路径 / 配置这类东西必须实跑验证。**
  2. **Qt 预定义键不能凭常识猜**（"Quit 应该就是 Esc 吧" —— 错）。要用就直接打印
     `QKeySequence(标准键).toString()` 看它在本平台解析成什么。

---

## BUG-0008 · `CMakeLists.txt` 被误粘贴文本导致 CMake 解析失败（D6）

- **现象**：配置阶段直接失败：
  ```
  CMake Error at CMakeLists.txt:1: Parse error.
  Expected a command name, got unquoted argument with text "CMake:".
  ```
- **复现**：查看文件首行 →
  ```
  CMake: Select a Kit# ================================================
  ```
  文件第 1 行**被插入了一段本应输入到 VS Code 命令面板的文本**，和原来的注释行黏在了一起。
- **根因**：**编辑器有焦点时把命令文本粘进了文件**。
  在 VS Code 里 `Ctrl+Shift+P` 打开的是**命令面板**（一个独立的输入框），
  如果焦点还在编辑器里，`Ctrl+Shift+P` 不生效，**复制的内容会直接落到光标处**。
- **修复**：删掉首行误植的 `CMake: Select a Kit`（保留原注释行）。
- **回归验证**：`scripts\build.bat` 重新配置 + 编译通过。
- 📌 **教训**：**执行命令前先确认焦点在命令面板**；
  以及 **"文件被改坏"时先看 diff 首几行** —— 这类误操作的特征就是"多出与代码无关的自然语言文本"。

---

## BUG-0009 · `QTimer` 成员未声明 ＋ 缺 include（D2 第 2 步）

- **现象**：加"连接超时"后编译失败，一次报 5 个错：
  ```
  error: 'm_connectTimer' was not declared in this scope        (220 / 235 / 249 行)
  error: expected type-specifier before 'QTimer'                (222 行)
  error: 'QTimer' has not been declared                         (225 行)
  ```
- **复现**：`.cpp` 里写 `m_connectTimer = new QTimer(this);`，但 `.h` 没有该成员声明、也没有 `QTimer` 的类型信息。
- **根因**：C++ 编译**从上往下读**，遇到 `QTimer *m_connectTimer` 必须先知道 `QTimer` **是个类型**（这靠"声明"），
  而要 `new QTimer(...)` / 取 `&QTimer::timeout` 还必须知道**完整定义**（这靠 `#include`）。两者都缺 → 全报"未声明"。
- **修复**：`.h` 加 `#include <QTimer>`（或前置声明 `class QTimer;`）＋ 成员 `QTimer *m_connectTimer = nullptr;`；
  `.cpp` 也补 `#include <QTimer>`（不依赖"传递包含"）。
- **回归验证**：补完后 `scripts\build.bat` 通过（0 error / 0 warning）。
- 📌 **教训（一条规律管两种情形）**：
  1. **只当"指针"存着** → 前置声明就够（写在 `.h`，省编译时间）
  2. **要 `new` / 调成员 / 取 `&类::成员函数`** → 必须 include **完整定义**
  3. **谁真正用到，谁自己 include** —— 别靠"上游头文件帮我带进来"；上游一改，下游莫名编译不过
  → 这与 **BUG-0005（`QMenuBar` 少了 include）是同一知识点的两个方向**：那次"该 include 没 include"，这次"该声明没声明"。

---

## BUG-0010 · `abort()` 不触发 `error` 信号（D2 第 2 步）⭐

- **现象**：给"连接超时"实现后，超时确实中止了连接（界面不再卡），但**上层收不到任何"连接失败"的通知** ——
  集成测试 `emitsErrorWhenPortClosed` 一直 `errorSpy.wait(3000)` 超时失败。
- **复现**（修复前）：
  ```cpp
  connect(m_connectTimer, &QTimer::timeout, this, [this]() {
      if (m_socket->state() != QAbstractSocket::ConnectedState) {
          m_socket->abort();          // ❌ 以为这样就会触发 error → onSocketError → connectionError
      }
  });
  ```
- **根因**：**`abort()` 只保证触发 `disconnected()`，不会触发 `error()`**。
  它和 `disconnectFromHost()` 的语义都是"**关闭连接**"，**不是"报告错误"**。
  所以"超时"这件事，**没有任何人替你 emit 出去** → 报错链路断在半路。
- **修复**：超时的 lambda 里**自己 emit**：
  ```cpp
  m_socket->abort();
  const QString reason = QStringLiteral("连接超时（%1 ms）").arg(m_config.timeoutMs);
  LOG_ERROR("ModbusTcpClient", reason);
  emit connectionError(reason);        // ← 关键：自己通知上层
  ```
- **回归验证**：改完 `emitsErrorWhenPortClosed` **立即变绿**（`tcptest` 从 3 passed/2 failed → **4 passed/1 failed**）。
- 📌 **教训**：**"关闭连接"和"报告错误"是两件事**。
  凡是想让上层知道"出事了"，就必须**显式 `emit` 自己的错误信号**，不能指望底层 API 顺带帮你发。

---

## BUG-0011 · 测试假设"连不上＝连接被拒"，Windows 实际是"超时"（D2 第 2 步）

- **现象**：`emitsErrorWhenPortClosed` 失败 —— 连一个"没人监听的端口"，3 秒内没等到 `connectionError`。
- **复现**：写独立程序连 `127.0.0.1:1` 与 `127.0.0.1:1502`（都无人监听），打印 `errorString()`：
  ```
  127.0.0.1:1     → SocketTimeoutError: "Network operation timed out"
  127.0.0.1:1502  → SocketTimeoutError: "Network operation timed out"
  ```
- **根因**：**测试按 Unix 行为假设**"连不上会立刻返回 `ConnectionRefusedError`（对端回 RST）」。
  但本机（Windows）实测报的是 **`SocketTimeoutError`** —— 本机协议栈/防火墙把 RST 吞掉了，表现成**丢包超时**。
  而**系统 TCP 连接超时约 21 秒**（SYN 重试），测试只等 **3 秒** → 必然失败。
- **修复**：不在测试里硬等 —— 在 `connectToDevice()` 里用 `QTimer` 实现 `m_config.timeoutMs`（1 秒），
  超时后 `abort()` ＋ `emit connectionError(...)`（见 BUG-0010）→ 1 秒内就有结果，测试稳定通过。
- **回归验证**：`emitsErrorWhenPortClosed` PASS。
- 📌 **教训（两条）**：
  1. **"连不上"在不同平台/网络环境下表现不同**（立刻拒绝 vs 超时）—— 涉及网络的测试**不能假设时延**
  2. **别用"3 秒"这种魔法等待时间**；要等外部行为就给它一个**可配置的超时**（这正是 `m_config.timeoutMs` 存在的意义）

---

## BUG-0012 · 集成测试共用 `QTcpServer` 导致相互干扰（**待修**）

- **现象**：`disconnectEmitsDisconnected` 失败（`disconnectFromDevice()` 后 3 秒内没等到 `disconnected`），
  但**日志里明明有 `[modbusTcpClient] 连接已断开`** —— 说明 `onDisconnected()` 跑了、`emit disconnected()` 也发了。
- **排查过程**（关键：**先证伪自己的代码，再怀疑测试**）：
  1. 日志证明实现侧走通 → 不像实现的问题
  2. 写**独立小程序**（不经 QtTest）复现"连 → 断" → `connected` ✅ `disconnected` ✅ **都收到了**
  3. → **同一份代码，单独跑就过、在测试序列里跑就失败** → 判定为**测试间共享状态**
- **根因**：`tests/test_tcp_connect.cpp` 里 `m_server`（`QTcpServer`）是**测试类的成员**，
  只在 `initTestCase()` 里 `listen()` 一次 → **三条测试共用同一个 server**。
  前序测试（尤其"连不上"那条）留下的连接/残留状态干扰了后一条测试的连接与断开。
- **修复（待做）**：改成**每条测试自己起一个 `QTcpServer`**（用完即关），保证测试之间**完全隔离**。
- **验证（待做）**：修完后预期 `tcptest` **5 passed / 0 failed**。
- 📌 **教训**：
  1. **"单独能跑、连起来跑就挂"＝典型的"测试间共享状态"信号**
  2. **测试要做成"可单独运行、可任意顺序运行"** —— 共享 fixture（尤其共享网络端口/服务器）是常见污染源
  3. **先证伪自己的实现，再改测试** —— 顺序反了就会去改本来正确的代码

---

## 待观察（低优先级）

| 编号 | 说明 | 计划 |
|---|---|---|
| TODO-0001 | `ModbusTcpClient::poll()` 未处理"上一帧未响应"的情况，慢设备下可能堆积请求 | D5 采集线程一并处理：加 `m_waitingResponse` 超时判断 |
| TODO-0002 | 模拟器 `onReadyRead()` 用函数内 `static QByteArray` 缓冲，只支持单客户端 | 当前够用；如需多客户端改为按 socket 存缓冲 |
