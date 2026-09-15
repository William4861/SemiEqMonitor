#include "ui/mainwindow.h"

#include "common/logger.h"
#include "common/version.h"

#include <QAction>
#include <QApplication>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QWidget>

namespace semieq {

namespace {
const char *kModule = "MainWindow";
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setupMenuBar();
    setupCentralArea();
    setupStatusBar();

    setWindowTitle(QStringLiteral("%1 v%2 —— 半导体设备数据采集与监控")
                       .arg(QStringLiteral(SEMIEQ_APP_NAME), QStringLiteral(SEMIEQ_VERSION_STR)));
    resize(1280, 800);

    LOG_INFO(kModule, QStringLiteral("主窗口已创建"));
}

MainWindow::~MainWindow() = default;

void MainWindow::setupMenuBar()
{
    // ---- 文件 ----
    QMenu *fileMenu = menuBar()->addMenu(QStringLiteral("文件(&F)"));

    QAction *connectAction = fileMenu->addAction(QStringLiteral("连接设备(&C)"));
    connectAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+K")));
    connectAction->setStatusTip(QStringLiteral("连接 Modbus-TCP 设备（D2 实现）"));
    connect(connectAction, &QAction::triggered, this, &MainWindow::showNotImplemented);

    QAction *exportAction = fileMenu->addAction(QStringLiteral("导出数据(&E)"));
    exportAction->setStatusTip(QStringLiteral("导出采集数据为 CSV（D11 实现）"));
    connect(exportAction, &QAction::triggered, this, &MainWindow::showNotImplemented);

    fileMenu->addSeparator();

    QAction *quitAction = fileMenu->addAction(QStringLiteral("退出(&Q)"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    // ---- 视图 ----
    QMenu *viewMenu = menuBar()->addMenu(QStringLiteral("视图(&V)"));
    const QStringList viewItems{
        QStringLiteral("实时监控"),
        QStringLiteral("数据查询"),
        QStringLiteral("报警列表"),
        QStringLiteral("SPC 分析"),
        QStringLiteral("Wafer Map"),
    };
    for (const QString &item : viewItems) {
        QAction *action = viewMenu->addAction(item);
        action->setStatusTip(QStringLiteral("%1（排期 D6 之后实现）").arg(item));
        connect(action, &QAction::triggered, this, &MainWindow::showNotImplemented);
    }

    // ---- 帮助 ----
    QMenu *helpMenu = menuBar()->addMenu(QStringLiteral("帮助(&H)"));

    QAction *aboutAction = helpMenu->addAction(QStringLiteral("关于 %1").arg(QStringLiteral(SEMIEQ_APP_NAME)));
    connect(aboutAction, &QAction::triggered, this, &MainWindow::showAbout);

    helpMenu->addAction(QStringLiteral("关于 Qt"))->setObjectName(QStringLiteral("aboutQt"));
    connect(helpMenu->actions().last(), &QAction::triggered, qApp, &QApplication::aboutQt);
}

void MainWindow::setupCentralArea()
{
    QWidget *central = new QWidget(this);
    QVBoxLayout *layout = new QVBoxLayout(central);

    m_placeholder = new QLabel(central);
    m_placeholder->setAlignment(Qt::AlignCenter);
    m_placeholder->setWordWrap(true);
    m_placeholder->setText(
        QStringLiteral("骨架已就绪（v%1）\n\n"
                       "下一步（D2-D8）：\n"
                       "  · 设备模拟器 + Modbus-TCP 采集\n"
                       "  · QThread 多线程采集与生产者-消费者队列\n"
                       "  · 实时曲线（自绘 QPainter）与数据表格（QAbstractTableModel）")
            .arg(QStringLiteral(SEMIEQ_VERSION_STR)));
    m_placeholder->setObjectName(QStringLiteral("placeholder"));

    layout->addWidget(m_placeholder);
    setCentralWidget(central);
}

void MainWindow::setupStatusBar()
{
    m_connStatus = new QLabel(QStringLiteral("未连接"), this);
    statusBar()->addWidget(m_connStatus);

    m_versionLabel = new QLabel(QStringLiteral("v%1").arg(QStringLiteral(SEMIEQ_VERSION_STR)), this);
    statusBar()->addPermanentWidget(m_versionLabel);
}

void MainWindow::showAbout()
{
    QMessageBox::about(
        this,
        QStringLiteral("关于 %1").arg(QStringLiteral(SEMIEQ_APP_NAME)),
        QStringLiteral("<h3>%1 v%2</h3>"
                       "<p>%3</p>"
                       "<p><b>技术栈</b>：C++11 / Qt 5 / SQLite / Modbus</p>"
                       "<p><b>构建时间</b>：%4</p>"
                       "<p>Qt 版本：%5</p>")
            .arg(QStringLiteral(SEMIEQ_APP_NAME),
                 QStringLiteral(SEMIEQ_VERSION_STR),
                 QStringLiteral(SEMIEQ_APP_DESC),
                 QStringLiteral(SEMIEQ_BUILD_TIME),
                 QStringLiteral(QT_VERSION_STR)));
}

void MainWindow::showNotImplemented()
{
    QAction *action = qobject_cast<QAction *>(sender());
    const QString name = action != nullptr ? action->text() : QStringLiteral("该功能");

    LOG_INFO(kModule, QStringLiteral("点击了待实现功能：%1").arg(name));
    statusBar()->showMessage(QStringLiteral("「%1」计划在后续迭代实现").arg(name), 3000);
}

} // namespace semieq
