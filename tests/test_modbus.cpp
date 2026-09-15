// =============================================================================
//  单元测试（Qt Test）
//
//  为什么一个校招项目要有单元测试：
//    1. Modbus 的"大端字节序"是本项目最容易写错、且错了很难肉眼发现的地方
//       （值全变成天文数字）。用测试把这个行为锁死，属于"防回归"。
//    2. 良率计算这类业务逻辑必须正确 —— 算错了交给客户的报表就是错的。
//    3. 面试可直接演示："我给协议解析和统计计算写了单测，跑一下全绿。"
//
//  运行： cmake --build build && build\bin\semieq_tests.exe
//     或： ctest --test-dir build --output-on-failure
// =============================================================================

#include "acquisition/modbustcpclient.h"
#include "common/types.h"

#include <QtTest/QtTest>

using namespace semieq;

namespace {

/// 手工拼一个 0x03 响应帧，方便构造各种异常输入
QByteArray makeResponse(quint16 transactionId,
                        quint8  slaveId,
                        quint8  funcCode,
                        const QVector<quint16> &values)
{
    QByteArray frame;
    const auto append16 = [&frame](quint16 v) {
        frame.append(static_cast<char>((v >> 8) & 0xFF));
        frame.append(static_cast<char>(v & 0xFF));
    };

    const quint16 byteCount = static_cast<quint16>(values.size() * 2);

    append16(transactionId);
    append16(0);                       // 协议标识
    append16(static_cast<quint16>(3 + byteCount));
    frame.append(static_cast<char>(slaveId));
    frame.append(static_cast<char>(funcCode));
    frame.append(static_cast<char>(byteCount & 0xFF));
    for (quint16 v : values) {
        append16(v);
    }
    return frame;
}

} // namespace

class TestModbus : public QObject
{
    Q_OBJECT

private slots:
    // ------------------------------------------------------- 请求帧组装 ---
    void buildRequest_totalLengthIs12();
    void buildRequest_fieldsAreBigEndian();
    void buildRequest_encodesAddressAndCount();

    // ------------------------------------------------------- 响应帧解析 ---
    void parse_validResponse();
    void parse_rejectsTooShortFrame();
    void parse_rejectsWrongFunctionCode();
    void parse_rejectsExceptionResponse();
    void parse_rejectsOddByteCount();
    void parse_rejectsIncompleteData();
    void parse_readsBigEndianValues_data();
    void parse_readsBigEndianValues();

    // ------------------------------------------------------- 期望长度 ---
    void expectedResponseLength_matchesParsedFrame();

    // --------------------------------------------------------- 业务逻辑 ---
    void wafer_yieldCalculation();
    void wafer_yieldIsZeroWhenEmpty();
};

// ---------------------------------------------------------------------------
//  请求帧组装
// ---------------------------------------------------------------------------

void TestModbus::buildRequest_totalLengthIs12()
{
    const QByteArray frame = ModbusTcpClient::buildReadHoldingRegistersRequest(1, 1, 0, 8);
    // MBAP 7 字节 + 功能码 1 + 地址 2 + 个数 2 = 12
    QCOMPARE(frame.size(), 12);
}

void TestModbus::buildRequest_fieldsAreBigEndian()
{
    // 事务标识 0x0102 应以 0x01, 0x02 的顺序出现
    const QByteArray frame = ModbusTcpClient::buildReadHoldingRegistersRequest(0x0102, 0x11, 0, 1);

    QCOMPARE(static_cast<quint8>(frame.at(0)), quint8(0x01));
    QCOMPARE(static_cast<quint8>(frame.at(1)), quint8(0x02));

    // 协议标识固定为 0
    QCOMPARE(static_cast<quint8>(frame.at(2)), quint8(0x00));
    QCOMPARE(static_cast<quint8>(frame.at(3)), quint8(0x00));

    // 长度固定为 6（单元标识 + PDU 5 字节）
    QCOMPARE(static_cast<quint8>(frame.at(4)), quint8(0x00));
    QCOMPARE(static_cast<quint8>(frame.at(5)), quint8(0x06));

    // 单元标识 = 从站地址
    QCOMPARE(static_cast<quint8>(frame.at(6)), quint8(0x11));

    // 功能码 0x03 读保持寄存器
    QCOMPARE(static_cast<quint8>(frame.at(7)), quint8(0x03));
}

void TestModbus::buildRequest_encodesAddressAndCount()
{
    const QByteArray frame = ModbusTcpClient::buildReadHoldingRegistersRequest(1, 1, 0x0010, 0x0002);

    QCOMPARE(static_cast<quint8>(frame.at(8)),  quint8(0x00));
    QCOMPARE(static_cast<quint8>(frame.at(9)),  quint8(0x10));   // 起始地址 16
    QCOMPARE(static_cast<quint8>(frame.at(10)), quint8(0x00));
    QCOMPARE(static_cast<quint8>(frame.at(11)), quint8(0x02));   // 读 2 个寄存器
}

// ---------------------------------------------------------------------------
//  响应帧解析
// ---------------------------------------------------------------------------

void TestModbus::parse_validResponse()
{
    const QByteArray frame = makeResponse(1, 1, 0x03, {100, 200, 300});

    bool    ok = false;
    QString error;
    const QVector<quint16> values =
        ModbusTcpClient::parseReadHoldingRegistersResponse(frame, &ok, &error);

    QVERIFY2(ok, qPrintable(error));
    QCOMPARE(values.size(), 3);
    QCOMPARE(values.at(0), quint16(100));
    QCOMPARE(values.at(1), quint16(200));
    QCOMPARE(values.at(2), quint16(300));
}

void TestModbus::parse_rejectsTooShortFrame()
{
    bool    ok = true;
    QString error;
    ModbusTcpClient::parseReadHoldingRegistersResponse(QByteArray("\x00\x01\x00", 3), &ok, &error);

    QVERIFY(!ok);
    QVERIFY(!error.isEmpty());
}

void TestModbus::parse_rejectsWrongFunctionCode()
{
    // 功能码给成 0x04（读输入寄存器），解析器必须拒绝
    const QByteArray frame = makeResponse(1, 1, 0x04, {1});

    bool    ok = true;
    QString error;
    ModbusTcpClient::parseReadHoldingRegistersResponse(frame, &ok, &error);

    QVERIFY(!ok);
    QVERIFY(error.contains(QStringLiteral("功能码")));
}

void TestModbus::parse_rejectsExceptionResponse()
{
    // 异常响应：功能码最高位置 1，后跟异常码
    QByteArray frame = makeResponse(1, 1, 0x83, {});
    frame.append(char(0x02));       // 异常码：非法数据地址

    bool    ok = true;
    QString error;
    ModbusTcpClient::parseReadHoldingRegistersResponse(frame, &ok, &error);

    QVERIFY(!ok);
    QVERIFY(error.contains(QStringLiteral("异常")));
}

void TestModbus::parse_rejectsOddByteCount()
{
    const QByteArray frame = makeResponse(1, 1, 0x03, {1, 2});
    QByteArray       broken = frame;
    broken[8] = char(0x03);        // 把字节数改成奇数 3

    bool    ok = true;
    QString error;
    ModbusTcpClient::parseReadHoldingRegistersResponse(broken, &ok, &error);

    QVERIFY(!ok);
}

void TestModbus::parse_rejectsIncompleteData()
{
    QByteArray frame = makeResponse(1, 1, 0x03, {1, 2, 3});
    frame.chop(2);                 // 截掉最后 2 字节，声明与实际不符

    bool    ok = true;
    QString error;
    ModbusTcpClient::parseReadHoldingRegistersResponse(frame, &ok, &error);

    QVERIFY(!ok);
    QVERIFY(error.contains(QStringLiteral("不完整")));
}

void TestModbus::parse_readsBigEndianValues_data()
{
    QTest::addColumn<QByteArray>("payload");
    QTest::addColumn<quint16>("expected");

    // Modbus 规定多字节字段为大端：高字节在前
    QTest::newRow("0x0001") << QByteArray("\x00\x01", 2) << quint16(1);
    QTest::newRow("0x0100") << QByteArray("\x01\x00", 2) << quint16(256);
    QTest::newRow("0x1234") << QByteArray("\x12\x34", 2) << quint16(0x1234);
    QTest::newRow("0xFFFF") << QByteArray("\xFF\xFF", 2) << quint16(65535);
}

void TestModbus::parse_readsBigEndianValues()
{
    QFETCH(QByteArray, payload);
    QFETCH(quint16, expected);

    // 直接构造帧，避免依赖 makeResponse（这次要精确控制数据段字节）
    QByteArray frame;
    const auto append16 = [&frame](quint16 v) {
        frame.append(static_cast<char>((v >> 8) & 0xFF));
        frame.append(static_cast<char>(v & 0xFF));
    };

    append16(1);                // 事务标识
    append16(0);                // 协议标识
    append16(5);                // 长度
    frame.append(char(1));      // 单元标识
    frame.append(char(0x03));   // 功能码
    frame.append(char(2));      // 字节数
    frame.append(payload);

    bool    ok = false;
    QString error;
    const QVector<quint16> values =
        ModbusTcpClient::parseReadHoldingRegistersResponse(frame, &ok, &error);

    QVERIFY2(ok, qPrintable(error));
    QCOMPARE(values.size(), 1);
    QCOMPARE(values.at(0), expected);
}

// ---------------------------------------------------------------------------
//  期望长度（粘包/拆包判断依据）
// ---------------------------------------------------------------------------

void TestModbus::expectedResponseLength_matchesParsedFrame()
{
    const int count = 8;
    const QByteArray frame = makeResponse(1, 1, 0x03, {1, 2, 3, 4, 5, 6, 7, 8});

    QCOMPARE(frame.size(), ModbusTcpClient::expectedResponseLength(count));
}

// ---------------------------------------------------------------------------
//  业务逻辑：良率
// ---------------------------------------------------------------------------

void TestModbus::wafer_yieldCalculation()
{
    Wafer wafer;
    wafer.waferId = QStringLiteral("W0001");
    wafer.lotId   = QStringLiteral("L20260916");

    for (int i = 0; i < 8; ++i) {
        Die die;
        die.x      = i;
        die.y      = 0;
        die.result = (i < 6) ? DieResult::Pass : DieResult::Fail;
        if (die.result == DieResult::Fail) {
            die.failCode = QStringLiteral("SHORT");
        }
        wafer.dies.append(die);
    }

    QCOMPARE(wafer.totalCount(), 8);
    QCOMPARE(wafer.passCount(), 6);
    QCOMPARE(wafer.failCount(), 2);
    QCOMPARE(wafer.yield(), 0.75);
}

void TestModbus::wafer_yieldIsZeroWhenEmpty()
{
    Wafer wafer;
    QCOMPARE(wafer.totalCount(), 0);
    QCOMPARE(wafer.yield(), 0.0);     // 不能除零
}

QTEST_GUILESS_MAIN(TestModbus)

#include "test_modbus.moc"
