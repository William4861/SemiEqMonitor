# D2 任务卡 · 实现 Modbus-TCP 协议

> 面向：王远 ｜ 日期：2026-09-16 ｜ 预计：上午 3h + 下午 3h
> 目标文件：`src/acquisition/modbustcpclient.cpp`
> 验收标准：`build\bin\semieq_tests.exe` 里 **TestModbus 全部变绿**

---

## 0. 先看清"填空式"怎么运作

| 谁 | 负责什么 |
|---|---|
| **AI 已给** | 构建系统、头文件（接口+协议图）、单元测试（= 验收标准）、模拟器、文档 |
| **你写** | `src/acquisition/modbustcpclient.cpp` 里的实现体 |
| **AI 批改** | 你写完发我 → 指出错误 / 改正方案 / 新知识小课 / 记笔记 |

**对答案的规矩**：每个函数**写完再对**，别提前看。
```bash
git diff ref-skeleton -- src/acquisition/modbustcpclient.cpp     # 看差异
git show ref-skeleton:src/acquisition/modbustcpclient.cpp        # 看完整参考实现
```

---

## 1. 今天的路线图（两小步，别一次做完）

| 步 | 内容 | 需要的新知识 | 完成标志 |
|---|---|---|---|
| **第 1 步** | 三个**纯函数**：组帧 / 期望长度 / 解析 | QByteArray + 大端字节序 | **10 个测试用例变绿** |
| **第 2 步** | 连接部分：QTcpSocket + 4 个信号 | QTcpSocket 异步模型 | 手工联调：能连上模拟器并读到数据 |

> 💡 第 1 步**完全不碰网络** —— 只做字节搬运。这是刻意的：协议解析是这个项目
> 最容易错、面试最爱问的部分，先把它单独攻下来，再去碰网络。

---

## 2. 第 1 步 · 前置小课

### 小课 A：QByteArray —— 网络世界里没有 int，只有字节

TCP 上跑的是**一串字节**。QByteArray 就是 Qt 的字节数组（≈ `std::string` 但按字节处理）。

```cpp
QByteArray b;
b.append(char(0x01));          // 追加 1 个字节
b.append(otherBytes);          // 追加一整段（QByteArray）
b.size();                      // 长度
b.at(3);                       // 第 3 个字节，返回 char —— ⚠️ 越界会断言崩溃
b[3] = char(0x05);             // 改第 3 个字节
b.left(12);                    // 取前 12 字节（拷贝）
b.remove(0, 12);               // 删掉前 12 字节
```

🔴 **必须记住的一个转换**：

```cpp
char c = b.at(0);                 // char 在 x86 上【有符号】
quint8 u = c;                     // ❌ 若 c 是 0x80，u 会变成负数 / 溢出
quint8 u = static_cast<quint8>(c); // ✅ 正确：当作无符号字节读
```

> 为什么不写 `unsigned char`？Qt 提供了 `quint8`（= `unsigned char` 的 Qt 别名），
> 项目里统一用 Qt 的类型。

### 小课 B：大端字节序 —— 本项目第 1 号坑

一个 16 位整数占 **2 个字节**。这两个字节谁在前，有两种约定：

| 约定 | 0x0102 存成 | 谁在用 |
|---|---|---|
| **大端**（big-endian） | `01 02`（高位在前） | **Modbus / TCP 等网络协议** |
| 小端（little-endian） | `02 01`（低位在前） | x86 CPU 的内存里 |

**所以"把 int 直接 memcpy 进字节数组"在 x86 上是错的** —— 必须手工拆字节。

```cpp
// 拆（写出去）：把 quint16 拆成两字节，高位在前
out.append(static_cast<char>((value >> 8) & 0xFF));   // 高字节
out.append(static_cast<char>(value & 0xFF));          // 低字节

// 合（读进来）：两字节还原成 quint16
quint16 v = static_cast<quint16>((static_cast<quint8>(d.at(o)) << 8)
                               |  static_cast<quint8>(d.at(o + 1)));
```

- `value >> 8`：右移 8 位 → 原来的高字节落到低 8 位
- `& 0xFF`：只保留最低 8 位，其余清零

⚠️ **为什么这个坑极隐蔽**：写反了**不会报错**。
温度 49（`0x0031`）会变成 12544（`0x3100`）—— 你只会看到"读到天文数字"，
然后怀疑人生。**这正是我在 `tests/test_modbus.cpp` 里专门写了
`buildRequest_fieldsAreBigEndian` 和 `parse_readsBigEndianValues` 两组用例的原因。**

### 📖 要实现的三个函数（纯函数，看 `modbustcpclient.h` 顶部的帧结构图）

| 函数 | 一句话 |
|---|---|
| `buildReadHoldingRegistersRequest()` | 把 7 个字段拼成 12 字节请求帧 |
| `expectedResponseLength()` | 算出一个正常响应帧有多少字节 |
| `parseReadHoldingRegistersResponse()` | 把响应帧拆回寄存器值数组，**并识别四种非法情况** |

每个函数体上方我都写了 `TODO(D2-x)` 注释，注明**要做什么**和**验收哪个用例**。
`parse` 那个函数用到 `fail(...)` 这个 lambda 来处理失败路径。

### ▶️ 第 1 步怎么做

```bash
# ① 拉一个功能分支（⚠️ 名字不要用斜杠，原因见 BUGS.md BUG-0004）
git checkout dev
git checkout -b feat-modbus-frame

# ② 写代码……
#    每写完一个函数就编译跑测试，看变绿了几个

cd "D:/code_work/trae_project/PreparationForInterview/CppQtProject/SemiEqMonitor"
cmake --build build
build/bin/semieq_tests.exe
```

> 🔎 **第一次跑测试会看到 `FAIL! ... Received a fatal error` 然后中断** —— 这不是你弄坏了，
> 是因为测试里读了空数组的 `at(0)`，触发 Qt 断言，整个测试进程就停了。
> `buildReadHoldingRegistersRequest()` 返回 12 字节之后，测试就能完整跑完、逐个列结果了。

---

## 3. 第 2 步 · 前置小课

### 小课 C：QTcpSocket 是**异步**的

```cpp
QTcpSocket *s = new QTcpSocket(this);     // ① 创建（parent 给 this，跟着一起销毁）
s->connectToHost("127.0.0.1", 1502);      // ② 发起连接 —— 立即返回，不阻塞
s->write(request);                        // ③ 发数据
```

🔴 **`connectToHost()` 立刻返回，此时还没连上。** 结果通过**信号**告诉你：

| 信号 | 什么时候发 |
|---|---|
| `connected()` | 连接成功 |
| `disconnected()` | 连接断开 |
| `readyRead()` | **有数据到了**（注意：可能只到一半！） |
| `error(...)` | 出错 |

所以**不能**这样写：
```cpp
s->connectToHost(host, port);
s->write(data);      // ❌ 连接可能还没建立，写进去就丢了
```
正确做法是：连上之后在 `onConnected()` 槽里再开始动作 ——
**这就是 Qt 把"网络"和"信号槽"绑在一起的设计**，也是为什么必须先把 P9–P15 学完。

⚠️ `error` 信号有个重载（`QAbstractSocket` 里既有同名的 error() 函数、又有 error 信号），
连接时要消歧 —— **正好用上你在 P11 学的写法**。

### 📖 要实现的函数

| 函数 | 内容 |
|---|---|
| `connectToDevice()` | 建 socket、连 4 个信号、清缓冲、`connectToHost` |
| `disconnectFromDevice()` | 断开 |
| `onConnected()` / `onDisconnected()` / `onSocketError()` | 三个槽：记日志 + 发信号 |

### ▶️ 第 2 步怎么做（手工联调）

```bash
# 终端 1：起设备模拟器
scripts\run.bat sim

# 终端 2：起上位机
scripts\run.bat monitor
# 点菜单「文件 → 连接设备」，看程序目录下 log\ 里的日志
```

看到 `连接成功 127.0.0.1:1502` 就算过。

---

## 4. 卡住了怎么办

1. 先看**报错原文**（编译错误读第一行，测试失败读 `FAIL!` 那一行）
2. 看 `docs/协议说明.md`（协议字段的完整表格）
3. 看 `docs/需求说明.md`（这一步属于哪个里程碑）
4. 还是不行 → **把报错原文贴给我**，别自己硬耗超过 20 分钟

---

## 5. 收工前要做的事（模拟团队工作流）

```bash
# ① 提交（Conventional Commits 格式）
git add src/acquisition/modbustcpclient.cpp
git commit -m "feat(acquisition): 实现 Modbus 0x03 请求帧组帧与响应解析"

# ② 更新开发日志：在 DEVLOG.md 最上面加今天一条
#    格式：今日目标 / 完成情况 / 遇到的问题 + 怎么解决 / 学到的 / 明日计划

# ③ 合并回 dev
git checkout dev
git merge --no-ff feat-modbus-frame -m "merge: Modbus 协议实现（D2）"
git branch -d feat-modbus-frame
```

> 📌 那个 `DEVLOG.md` 是**给面试官看的**：它证明你每天都在遇到问题并解决它。
> 校招生 90% 的项目没有这个，有了就是差异点。

---

## 6. 完成后的自检清单

- [ ] `build/bin/semieq_tests.exe` 全绿（从 18 passed 起）
- [ ] 能口头解释：**为什么 Modbus 用大端、写反了会怎样**
- [ ] 能口头解释：**粘包/拆包是什么、`m_rxBuffer` 为什么必要**（第 2 步）
- [ ] 能口头解释：**`connectToHost` 之后为什么不能立刻 `write`**
- [ ] `DEVLOG.md` 有 D2 一条
- [ ] `feat-modbus-frame` 已合并进 `dev`

把写完的代码发我，我按「① 指出错误 ② 改正方案 + 新知识小课 ③ 记笔记」三步批改。
