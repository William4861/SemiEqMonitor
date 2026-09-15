#include "common/logger.h"
#include "common/types.h"
#include "common/version.h"
#include "ui/mainwindow.h"

#include <QApplication>
#include <QDate>
#include <QDir>
#include <QMetaType>
#include <QString>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    QApplication::setApplicationName(QStringLiteral(SEMIEQ_APP_NAME));
    QApplication::setApplicationVersion(QStringLiteral(SEMIEQ_VERSION_STR));
    QApplication::setOrganizationName(QStringLiteral(SEMIEQ_VERSION_ORG));

    // ------------------------------------------------------------------------
    // 跨线程信号槽传递自定义类型之前必须注册，否则 QueuedConnection 会在运行时
    // 报 "Cannot queue arguments of type ..."（采集线程 → UI 线程会踩这个）
    // ------------------------------------------------------------------------
    qRegisterMetaType<semieq::Parameter>("semieq::Parameter");
    qRegisterMetaType<semieq::Alarm>("semieq::Alarm");
    qRegisterMetaType<QVector<semieq::Parameter>>("QVector<semieq::Parameter>");
    qRegisterMetaType<QVector<semieq::Alarm>>("QVector<semieq::Alarm>");

    // ------------------------------------------------------------------------
    // 日志：写到可执行文件旁的 log/ 目录（打包发布后也能定位问题）
    // ------------------------------------------------------------------------
    const QString logDir = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("log"));
    const QString logPath =
        QDir(logDir).filePath(QStringLiteral("semieq_%1.log")
                                  .arg(QDate::currentDate().toString(QStringLiteral("yyyyMMdd"))));

    if (!semieq::Logger::instance().open(logPath, semieq::LogLevel::Info)) {
        qWarning("日志文件打开失败：%s", qPrintable(logPath));
    }

    LOG_INFO("main", QStringLiteral("SemiEqMonitor v%1 启动（%2，构建于 %3）")
                         .arg(QStringLiteral(SEMIEQ_VERSION_STR),
                              QStringLiteral(SEMIEQ_APP_DESC),
                              QStringLiteral(SEMIEQ_BUILD_TIME)));

    semieq::MainWindow window;
    window.show();

    const int exitCode = app.exec();

    LOG_INFO("main", QStringLiteral("程序退出，返回码 %1").arg(exitCode));
    semieq::Logger::instance().close();

    return exitCode;
}
