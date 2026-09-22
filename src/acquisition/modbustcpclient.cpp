#include "acquisition/modbustcpclient.h"

// =============================================================================
//  ⚠️ 填空式骨架 —— 本文件是所有实现体需要你自己补全的地方（D2 / D3 任务）
//
//  唯一验收标准：测试变绿
//      cmake --build build
//      build\bin\semieq_tests.exe        （或 scripts\run.bat test）
//    当前预期：TestModbus 有 12 个用例失败 —— 这是正常的"红"，不是坏了。
//
//  对答案（写完一个函数再对，别提前看）：
//      git diff ref/skeleton -- src/acquisition/modbustcpclient.cpp
//      或 git show ref/skeleton:src/acquisition/modbustcpclient.cpp
//
//  任务卡（含前置知识小课，先看它再动手）：
//      docs/tasks/D2_Modbus协议实现.md
//
//  已给你写好的部分（不用动）：
//      · 构造函数 / name() / isConnected() / setConfig() / setParameters()
//      · 协议帧结构图在 modbustcpclient.h 顶部
//      · 全部单元测试在 tests/test_modbus.cpp（它就是"需求规格"）
// =============================================================================

namespace semieq {

// ------------------------------------------------------- 已实现（不用改） ---

ModbusTcpClient::ModbusTcpClient(QObject *parent)
    : IDeviceClient(parent)
{
}

ModbusTcpClient::ModbusTcpClient(const Config &config, QObject *parent)
    : IDeviceClient(parent)
    , m_config(config)
{
}

QString ModbusTcpClient::name() const
{
    return QStringLiteral("Modbus/TCP");
}

bool ModbusTcpClient::isConnected() const
{
    return m_socket != nullptr && m_socket->state() == QAbstractSocket::ConnectedState;
}

void ModbusTcpClient::setConfig(const Config &config)
{
    m_config = config;
}

void ModbusTcpClient::setParameters(const QVector<Parameter> &params)
{
    m_parameters = params;
}

// ========================================================== D2 任务：协议 ===
//  这一组是【纯函数】—— 不碰网络，只做字节搬运。所以最容易被单元测试覆盖，
//  也最适合第一天动手。写完它们，协议这一关就算过了。

QByteArray ModbusTcpClient::buildReadHoldingRegistersRequest(int transactionId,
                                                             int slaveId,
                                                             int startAddress,
                                                             int registerCount)
{
    // TODO(D2-1) 组帧：按 modbustcpclient.h 顶部那张图，依次放入
    //            事务标识(2) / 协议标识(2) / 长度(2) / 单元标识(1) / 功能码(1)
    //            / 起始地址(2) / 寄存器个数(2)  →  一共 12 字节
    //
    //  ⚠️ 唯一的坑：所有 2 字节字段都是【大端】—— 高字节在前。
    //     写反了不会报错，只会读到天文数字（比如 0x0102 变成 0x0201）。
    //     拼字节的写法在 C++ 里就是：(值 >> 8) & 0xFF 先放，再放 值 & 0xFF。
    //
    //  提示：QByteArray 用 append(char) 追加；返回值长度应为 12。
    //  验收：buildRequest_totalLengthIs12 / buildRequest_fieldsAreBigEndian /
    //        buildRequest_encodesAddressAndCount
   QByteArray res;
   res.append(static_cast<char>((transactionId>>8)&0xff));
   res.append(static_cast<char>(transactionId&0xff));
   res.append(static_cast<char>(0x00));
   res.append(static_cast<char>(0x00));
   res.append(static_cast<char>(0x00));
   res.append(static_cast<char>(0x06));
   res.append(static_cast<char>(slaveId&0xff));
   res.append(char(0x03));
   res.append(static_cast<char>((startAddress>>8)&0xff));
   res.append(static_cast<char>(startAddress&0xff));
   res.append(static_cast<char>((registerCount>>8)&0xff));
   res.append(static_cast<char>(registerCount&0xff));

    return res;
}

int ModbusTcpClient::expectedResponseLength(int registerCount)
{
    // TODO(D2-2) 算出"一个正常响应帧应该有多少字节"。
    //  它只被 onReadyRead() 用来判断"收够了没有"（粘包/拆包的关键）。
    //
    //  拆解：MBAP 头 7 字节 + 功能码 1 + 字节数 1 + 数据(每个寄存器 2 字节)
    //
    //  验收：expectedResponseLength_matchesParsedFrame
    int res = 7+1+1+registerCount*2;
    return res;
}

QVector<quint16> ModbusTcpClient::parseReadHoldingRegistersResponse(const QByteArray &frame,
                                                                    bool            *ok,
                                                                    QString         *error)
{
    // TODO(D2-3) 解析响应帧。按顺序做判断，任何一步不合法就返回失败：
    //
    //   ① 帧长 < 9 字节                      → 失败（连包头都读不全）
    //   ② 功能码最高位是 1（即 >= 0x80）      → 这是【异常响应】，
    //                                          第 9 字节是异常码，要报出来
    //   ③ 功能码 != 0x03                      → 失败
    //   ④ 第 9 字节 = 字节数，必须满足：是偶数，且剩余字节数 >= 它
    //   ⑤ 把数据段两字节一组、按【大端】还原成 quint16
    //
    //  怎么写"失败"：函数开头先放一个 fail lambda，然后直接
    //  `return fail(QStringLiteral("原因"))` —— 它会帮你置 ok/error。
    //
    //  ⚠️ 修正：上一版注释说"我给你留着了"，实际骨架漏了（我的锅，已更正如上）。
    //     下面这段【照抄进函数开头】，三行函数体你自己写：
    //
    //      auto fail = [ok, error](const QString &reason) {
    //          // ① 若 ok 非空 → *ok = false
    //          // ② 若 error 非空 → *error = reason
    //          // ③ 返回一个空 QVector<quint16>()
    //      };
    //
    //  错误信息要含关键词（测试会查）：功能码 / 异常 / 不完整
    //
    //  验收：parse_validResponse + 四个 parse_rejects* + 两个大数据端用例
    auto fail = [ok, error](const QString& reason)
        {
            if (ok) *ok = false;
            if (error) *error = reason;
            return QVector<quint16>();
        };
    if(frame.size()<9 )
    {
        return fail(QStringLiteral("帧长过短，解析失败!"));
    }
    else if (static_cast<quint8>(frame.at(7))&static_cast<quint8>(0x80))
    {
        return fail(QStringLiteral("功能码最高位是1，解析异常!异常码为:")+QString::number(static_cast<quint8>(frame.at(8))));
    }
    else if (frame.at(7) != static_cast<char>(0x03))
    {
        return fail(QStringLiteral("功能码!=0x03，解析失败!"));
    }
    else if (static_cast<quint8>(frame.at(8)) % 2 != 0)          // ① 字节数是奇数
    {
        return fail(QStringLiteral("字节数必须是偶数"));
    }
    else if (frame.size() < 9 + static_cast<quint8>(frame.at(8))) // ② 剩余字节不够
    {
        return fail(QStringLiteral("数据不完整：声明 ")
            + QString::number(static_cast<quint8>(frame.at(8)))
            + QStringLiteral(" 字节，实际只有 ")
            + QString::number(frame.size() - 9)
            + QStringLiteral(" 字节"));
    }
    else
    {
        *ok = true;
        QVector<quint16> res;
        QVector<quint8> temp;
        for (quint8 i = 0; i < static_cast<quint8>(frame.at(8)); i++)
        {
            temp.append(static_cast<quint8>(frame.at(9+i)));
            if (i % 2 != 0)
            {
                res.append(static_cast<quint16>((temp.at(i - 1) << 8) | temp.at(i)));
            }
            
        }
        return res;
    }
    


    
}

// ================================================== D2 任务：连接与信号 ===
//  这一组要开始碰网络了。选做顺序就是下面这个顺序 —— 先把"能连上"跑通。

void ModbusTcpClient::connectToDevice()
{
    // TODO(D2-4) ① 如果 m_socket 还是 nullptr，new 一个 QTcpSocket(this)
    //            （parent 给 this，这样客户端对象销毁时 socket 一起走）
    //            ② 把 socket 的四个信号连到本类的私有槽上（槽函数就在下面）：
    //                 connected     → onConnected
    //                 disconnected  → onDisconnected
    //                 readyRead     → onReadyRead
    //                 error         → onSocketError   ← 这个有重载，注意 P11 学过
    //                                             的消歧写法
    //            ③ 清空 m_rxBuffer，然后 connectToHost(m_config.host, m_config.port)
    //
    //  验收：无单测，靠 D2 手工联调 —— 起模拟器，点「连接设备」，看日志有没有"连接成功"
}

void ModbusTcpClient::disconnectFromDevice()
{
    // TODO(D2-5) 断开连接（一行，但要注意 m_socket 可能是 nullptr）
}

void ModbusTcpClient::onConnected()
{
    // TODO(D2-6) 记一条日志 + emit connected()
}

void ModbusTcpClient::onDisconnected()
{
    // TODO(D2-6) 把等待响应的标志清掉 + 记日志 + emit disconnected()
}

void ModbusTcpClient::onSocketError(QAbstractSocket::SocketError error)
{
    // TODO(D2-6) 从 m_socket 取 errorString() 记错误日志 + emit connectionError(原因)
    Q_UNUSED(error)
}

// ======================================================= D3 任务：轮询采集 ===
//  到这里"联调"才算完整闭环：上位机主动问 → 模拟器答 → 上位机解析成参数值。

void ModbusTcpClient::poll()
{
    // TODO(D3-1) ① 没连接就直接 emit connectionError 并返回
    //            ② 事务标识自增（每问一次 +1，用来和响应对上号）
    //            ③ 调 buildReadHoldingRegistersRequest() 组帧
    //            ④ m_socket->write(帧)
    //
    //  ⚠️ 顺手想一想：如果设备很慢，上一帧还没回、这一帧又发出去会怎样？
    //     要不要加个"上一帧未回应就跳过本次"的保护？（参考实现里留了 TODO 标记）
}

void ModbusTcpClient::onReadyRead()
{
    // TODO(D3-2) ① 把 m_socket->readAll() 追加进 m_rxBuffer
    //            ② 判断攒够 expectedResponseLength() 没有 —— 不够就直接 return，
    //               等下一次 readyRead（这就是【拆包】处理）
    //            ③ 取出一帧（其余字节留在缓冲里 —— 【粘包】处理）
    //            ④ 调 parseReadHoldingRegistersResponse() 解析
    //            ⑤ 解析出来的寄存器值 → 按顺序配上 m_parameters 里的名称/单位/上下限，
    //               补上时间戳，emit parametersRead(参数列表)
    //
    //  为什么要 m_rxBuffer：TCP 是【字节流】不是"消息流"。
    //  一次 readyRead 可能只收到半帧，也可能一次收到两帧 ——
    //  没有缓冲区就只能靠运气。
}

} // namespace semieq
