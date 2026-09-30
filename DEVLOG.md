# 开发日志 DEVLOG

> 每个开发日一条。格式：**今日目标 / 完成情况 / 遇到的问题 / 怎么解决 / 学到的 / 明日计划**。
> 目的：①留下真实的开发轨迹 ②面试时能讲清"我遇到过什么、怎么解决的"。

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
