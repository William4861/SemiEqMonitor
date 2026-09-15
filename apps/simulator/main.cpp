// =============================================================================
//  SemiEqSimulator —— 半导体设备模拟器入口
//
//  与上位机是两个完全独立的可执行程序（真实项目里"设备"和"上位机"本来就分属
//  不同厂商、不同代码库）。它只依赖 Qt Core + Network，不依赖上位机的任何代码。
//
//  用法： SemiEqSimulator.exe [--port=1502] [--interval=500]
// =============================================================================

#include "devicesimulator.h"

#include <QCoreApplication>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QTextStream>

namespace {

void printBanner(const semieq::DeviceSimulator::Config &config)
{
    QTextStream out(stdout);
    out << "\n"
        << "==============================================================\n"
        << "  SemiEqSimulator  v0.1.0  —— 半导体设备模拟器\n"
        << "  监听端口 : " << config.port << "  (Modbus-TCP)\n"
        << "  从站地址 : " << config.slaveId << "\n"
        << "  寄存器数 : " << config.registerCount << "\n"
        << "--------------------------------------------------------------\n"
        << "  控制台命令（输入后回车）：\n"
        << "    h  注入故障：腔体温度飙升（触发上位机上限报警）\n"
        << "    n  恢复正常\n"
        << "    d  强制断开客户端（验证上位机断线重连）\n"
        << "    l  打印当前模拟值\n"
        << "    q  退出\n"
        << "==============================================================\n\n";
    out.flush();
}

} // namespace

int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setApplicationName(QStringLiteral("SemiEqSimulator"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));

    semieq::DeviceSimulator::Config config;

    const QStringList args = QCoreApplication::arguments();
    for (const QString &arg : args) {
        if (arg.startsWith(QStringLiteral("--port="))) {
            config.port = static_cast<quint16>(arg.mid(QStringLiteral("--port=").size()).toUInt());
        } else if (arg.startsWith(QStringLiteral("--interval="))) {
            config.valueIntervalMs =
                arg.mid(QStringLiteral("--interval=").size()).toInt();
        }
    }

    printBanner(config);

    semieq::DeviceSimulator simulator(config);
    if (!simulator.start()) {
        QTextStream(stderr) << "启动失败，退出。\n";
        return 1;
    }

    // 控制台读取线程 → 模拟器命令槽
    semieq::ConsoleReader reader;
    QObject::connect(&reader,
                     &semieq::ConsoleReader::commandEntered,
                     &simulator,
                     &semieq::DeviceSimulator::onCommand);
    reader.start();

    const int exitCode = app.exec();

    reader.quit();
    reader.wait(2000);
    simulator.stop();

    return exitCode;
}
