#include "ui/mainwindow.h"

#include "common/version.h"     // SEMIEQ_APP_NAME / SEMIEQ_VERSION_STR / SEMIEQ_BUILD_TIME
#include "ui/curvewidget.h"
#include "acquisition/modbustcpclient.h"
#include "acquisition/ideviceclient.h"

#include <QMenuBar>
#include <QKeySequence>
#include <QApplication>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QComboBox>
#include <QTimer>
#include <QThread>
// =============================================================================
//  主窗口实现 —— 分两批完成：
//    ✅ D6（已完成）：菜单栏 / 中央区域骨架 / 状态栏 / 关于框 / 未实现提示
//    ⏳ D5（本次）：采集接线 —— 「连接设备」→ 客户端 + 轮询定时器 + 状态栏 + 喂曲线
//
//  任务卡：docs/tasks/D5_实时曲线.md（接线部分 = §4 的 D5-10）
//  对答案：git diff ref/skeleton -- src/ui/mainwindow.cpp
//
//  已给你写好的部分：构造函数、D6 全部函数、D5 的中央区域（曲线+下拉框）与默认通道表
// =============================================================================

namespace semieq {

// =================================================== D5：默认采集点定义 ======
//  ✅ 已给（配置数据，不需要你写）—— 8 个通道与设备模拟器（apps/simulator）的
//  registerNames / Units / Limits 保持一致；真实项目里应来自配置/数据库，
//  这里为演示直接内联（面试可如实讲）。
//  ⚠️ 顺序必须与寄存器地址顺序一致（0x0000 起的 8 个）。
namespace {

QVector<Parameter> defaultChannels()
{
    QVector<Parameter> channels;
    channels.reserve(8);

    const auto add = [&channels](const QString &code, const QString &name,
                                 const QString &unit, double scale,double lo, double hi) {
        Parameter p;
        p.code       = code;
        p.name       = name;
        p.unit       = unit;
        p.scale      = scale;
        p.lowerLimit = lo;
        p.upperLimit = hi;
        channels.append(p);
    };

    add(QStringLiteral("TEMP_CH1"), QStringLiteral("腔体温度"),   QStringLiteral("℃"), 0.01,20.0, 80.0);
    add(QStringLiteral("PRES_CH1"), QStringLiteral("腔体压力"),   QStringLiteral("Pa"),0.01,90.0, 120.0);
    add(QStringLiteral("FLOW_CH1"), QStringLiteral("冷却水流量"), QStringLiteral("L/min"),0.001,1.0, 12.0);
    add(QStringLiteral("RPM_CH1"),  QStringLiteral("主轴转速"),   QStringLiteral("rpm"),1.0,0.0, 3000.0);
    add(QStringLiteral("POWR_CH1"), QStringLiteral("加热功率"),   QStringLiteral("W"),0.1, 0.0, 900.0);
    add(QStringLiteral("VACM_CH1"), QStringLiteral("真空度"),     QStringLiteral("kPa"),0.0001,0.0, 1.0);
    add(QStringLiteral("BELT_CH1"), QStringLiteral("传送带速度"), QStringLiteral("mm/s"),0.01,0.0, 120.0);
    add(QStringLiteral("CURR_CH1"), QStringLiteral("电源电流"),   QStringLiteral("A"),0.001,0.0, 16.0);

    return channels;
}

} // namespace

// ------------------------------------------------------- 已实现（不用改） ---

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // 窗口 = 菜单栏 + 中心区域 + 状态栏 + 采集接线（D5 加），四块各自一个 setup 函数
    setupMenuBar();
    setupCentralArea();
    setupStatusBar();
    setupAcquisition();   // D5：建客户端 + 轮询定时器 + 连信号（函数体待你补，见文件底部）

    setWindowTitle(QStringLiteral("%1 v%2 —— 半导体设备数据采集与监控")
                       .arg(QStringLiteral(SEMIEQ_APP_NAME), QStringLiteral(SEMIEQ_VERSION_STR)));
    resize(1280, 800);
}

MainWindow::~MainWindow()
{
    if(m_acqThread)
    {
        m_acqThread->quit();
        m_acqThread->wait();
    }
}

// =================================================== 待你实现（D6 任务） ===

void MainWindow::setupMenuBar()
{
    // TODO(D6-1) 建菜单栏。三个主菜单：
    //
    //   「文件」：连接设备(Ctrl+K) / 导出数据 / 分隔线 / 退出(Ctrl+Q)
    //   「视图」：实时监控 / 数据查询 / 报警列表 / SPC 分析 / Wafer Map
    //   「帮助」：关于 SemiEqMonitor / 关于 Qt
    //
    //  用到的类（P17 会讲）：QMenuBar / QMenu / QAction / QKeySequence
    //  ⚠️ 两个细节：
    //    · 菜单标题写 "文件(&F)" 里的 & 是【助记符】，Alt+F 能打开它
    //    · 「关于 Qt」那一项直接连到 QApplication::aboutQt，
    //      这行代码本身就是展示"我知道 Qt 自带什么"
    //
    //  暂时没实现的功能（导出/视图那几项）先连到 showNotImplemented()，
    //  它会弹一条状态栏提示 —— 别让菜单点了没反应。

    //创建菜单栏
    QMenuBar *mBar = this->menuBar();

    //创建菜单项
    QMenu *fileMenu = mBar->addMenu(QStringLiteral("文件(&F)"));
    QMenu *viewMenu = mBar->addMenu(QStringLiteral("视图(&V)"));
    QMenu *helpMenu = mBar->addMenu(QStringLiteral("帮助(&H)"));
    
    //文件菜单项的Action
    QAction *actConnet = fileMenu->addAction(QStringLiteral("连接设备(Ctrl+K)"));
    QAction *actExport = fileMenu->addAction(QStringLiteral("导出数据"));
    fileMenu->addSeparator();
    QAction *actQuit = fileMenu->addAction(QStringLiteral("退出(Ctrl+Q)"));

    //视图菜单项的Action
    QAction *actMonitor = viewMenu->addAction(QStringLiteral("实时监控"));
    QAction *actDataSearch = viewMenu->addAction(QStringLiteral("数据查询"));
    QAction *actAlarmList = viewMenu->addAction(QStringLiteral("报警列表"));
    QAction *actSPC = viewMenu->addAction(QStringLiteral("SPC分析"));
    QAction *actWaferMap = viewMenu->addAction(QStringLiteral("WaferMap"));

    //帮助菜单栏的Action
    QAction *actAboutSemi = helpMenu->addAction(QStringLiteral("关于SemiEqMonitor"));
    QAction *actAboutQt = helpMenu->addAction(QStringLiteral("关于Qt"));

    //文件菜单项Action的快捷键
    actConnet->setShortcut(QKeySequence(QStringLiteral("Ctrl+K")));
    actQuit->setShortcut(QKeySequence(QStringLiteral("Ctrl+Q")));

    //文件菜单项的连接
    connect(actConnet,&QAction::triggered,this,&MainWindow::onConnectDevice);
    connect(actExport,&QAction::triggered,this,&MainWindow::showNotImplemented);
    connect(actQuit,&QAction::triggered,this,&QWidget::close);
    
    //视图菜单项的连接
    connect(actMonitor,&QAction::triggered,this,&MainWindow::showNotImplemented);
    connect(actDataSearch,&QAction::triggered,this,&MainWindow::showNotImplemented);
    connect(actAlarmList,&QAction::triggered,this,&MainWindow::showNotImplemented);
    connect(actSPC,&QAction::triggered,this,&MainWindow::showNotImplemented);
    connect(actWaferMap,&QAction::triggered,this,&MainWindow::showNotImplemented);

    //帮助菜单项的连接
    connect(actAboutSemi,&QAction::triggered,this,&MainWindow::showAbout);
    connect(actAboutQt,&QAction::triggered,this,&QApplication::aboutQt);
    
}

void MainWindow::setupCentralArea()
{
    // ✅ D6-2 已完成（QWidget + QVBoxLayout 骨架）；D5 起中央区域升级为：
    //   顶部通道选择器（QComboBox）＋ 实时曲线（CurveWidget，自绘）

    QWidget *widget = new QWidget(this);
    setCentralWidget(widget);

    QVBoxLayout *vLayout = new QVBoxLayout(widget);

    // —— D5：通道选择器 + 实时曲线 ——
    //   通道定义来自本文件顶部 defaultChannels()，下拉框与曲线共用同一份，
    //   保证"下拉选中的通道"和"曲线画的通道"始终对得上。
    m_channelSelector = new QComboBox(widget);
    for (const Parameter &channel : defaultChannels()) {
        m_channelSelector->addItem(channel.name);
    }
    vLayout->addWidget(m_channelSelector);

    m_curve = new CurveWidget(widget);
    m_curve->setChannels(defaultChannels());   // 通道定义先交给曲线；之后只管喂数据
    vLayout->addWidget(m_curve, 1);            // 拉伸系数 1：窗口放大时曲线吃掉剩余空间

    // 下拉切换 → 曲线换显示通道（⭐"改状态 → update()"的标准套路，P38 刚验收）
    //   ⚠️ currentIndexChanged 有 int / QString 两个重载 → 用 QOverload<int>::of 指定要哪个
    //   （实测坑：小写 qOverload 是 C++14 变量模板，本项目 C++11 编不过 → 用类的 of() 形式）
    connect(m_channelSelector, QOverload<int>::of(&QComboBox::currentIndexChanged),
            m_curve, &CurveWidget::setCurrentChannel);
}

void MainWindow::setupStatusBar()
{
    // TODO(D6-3) 建状态栏。
    //   · 左侧（addWidget）：连接状态 m_connStatus，初始显示"未连接"
    //   · 右侧（addPermanentWidget）：版本号 m_versionLabel
    //
    //  ⚠️ addWidget 和 addPermanentWidget 的区别：后者不会被临时消息顶掉。
    //     所以"版本号"这种常驻信息要用 permanent —— 面试可能会问。

    QStatusBar *sBar = statusBar();

    m_connStatus = new QLabel(QStringLiteral("未连接"),sBar);
    m_versionLabel = new QLabel(QStringLiteral("v %1").arg(SEMIEQ_VERSION_STR),sBar);
    sBar->addWidget(m_connStatus);
    sBar->addPermanentWidget(m_versionLabel);
}

void MainWindow::showAbout()
{
    // TODO(D6-4) 弹出「关于」对话框（QMessageBox::about）。
    //   内容至少包含：应用名、版本号、一句话描述、
    //   技术栈（C++11 / Qt 5 / SQLite / Modbus）、构建时间、Qt 版本。
    //
    //  版本号宏 SEMIEQ_APP_NAME / SEMIEQ_VERSION_STR / SEMIEQ_BUILD_TIME
    //  来自 src/common/version.h（由 CMake 自动生成）—— 别硬编码版本号。

    QMessageBox::about(this,QStringLiteral("关于SemiEqMonitor"),QStringLiteral("应用名：%1\n版本号：%2\n构建时间：%3\nQt版本：5.14.2\n").arg(SEMIEQ_APP_NAME,SEMIEQ_VERSION_STR,SEMIEQ_BUILD_TIME));
}

void MainWindow::showNotImplemented()
{
    // TODO(D6-5) 状态栏提示"某功能待实现"。
    //   ① 用 qobject_cast<QAction*>(sender()) 拿到是哪个 action 被点了
    //      —— sender() 是 QObject 提供的"谁给我发的信号"（P12 的延伸）
    //   ② 状态栏 showMessage("「XXX」计划在后续迭代实现", 3000)
    //      —— 第二个参数是自动清除的毫秒数
    QAction *temp = qobject_cast<QAction*>(sender());
    if(!temp) return;
    statusBar()->showMessage(QStringLiteral("[%1] 功能计划在后续迭代实现").arg(temp->text()),3000);
}

// ==================================================== D5：采集接线（待你实现）=

void MainWindow::setupAcquisition()
{
    // TODO(D5-10②) 建客户端 + 轮询定时器 + 连信号（细节见 docs/tasks/D5_实时曲线.md）：
    //   ① ModbusTcpClient::Config cfg：
    //        host = "127.0.0.1"；port = 1502（⚠️ 模拟器端口，不是标准 502）；
    //        slaveId = 1；registerCount = 8；其余默认
    //   ② m_client = new ModbusTcpClient(cfg, this);
    //      m_client->setParameters(defaultChannels());
    //        ⚠️ 不喂定义的话，parametersRead 发出来的是【空列表】——数据对不上
    //   ③ m_pollTimer = new QTimer(this); 间隔 500ms（＝模拟器刷新周期）
    //      connect：m_pollTimer timeout → m_client poll
    //   ④ 连客户端 4 个信号（信号声明在基类 IDeviceClient，用 &IDeviceClient::xxx）：
    //        connected        → onDeviceConnected
    //        disconnected     → onDeviceDisconnected
    //        parametersRead   → onParametersRead
    //        connectionError  → onConnectionError
    //   （定时器先别 start —— 连上以后由 onDeviceConnected 启动）

    //创建配置对象
    ModbusTcpClient::Config cfg;
    cfg.host = QStringLiteral("127.0.0.1");
    cfg.port = 1502;
    cfg.slaveId = 1;
    cfg.registerCount = 8;

    //创建ModbusTcp客户端对象
    m_client = new ModbusTcpClient(cfg);
    m_client->setParameters(defaultChannels()); //配置好要采集的8个设备的code、name、unit、lowerLimt、upperLimit

    //把m_client移动到采集线程
    m_acqThread = new QThread(this);
    m_client->moveToThread(m_acqThread);
    m_acqThread->start();

    //创建定时器
    m_pollTimer = new QTimer(this);
    m_pollTimer->setInterval(500);//500ms
    

    //给客户端4个信号连接好对应槽函数
    connect(m_client,&IDeviceClient::connected,this,&MainWindow::onDeviceConnected);
    connect(m_client,&IDeviceClient::disconnected,this,&MainWindow::onDeviceDisconnected);
    connect(m_client,&IDeviceClient::parametersRead,this,&MainWindow::onParametersRead);
    connect(m_client,&IDeviceClient::connectionError,this,&MainWindow::onConnectionError);
    
    //跨线程信号槽连接
    connect(m_pollTimer,&QTimer::timeout,m_client,&ModbusTcpClient::poll);//轮询设备
    connect(this,&MainWindow::connectRequested,m_client,&ModbusTcpClient::connectToDevice);//连接设备
    connect(this,&MainWindow::disconnectRequested,m_client,&ModbusTcpClient::disconnectFromDevice);//断开设备连接
    connect(m_acqThread,&QThread::finished,m_client,&QObject::deleteLater);//采集线程销毁则回收m_clinet内存
}

void MainWindow::onConnectDevice()
{
    // TODO(D5-10④) 菜单「连接设备」被点击（记得把 setupMenuBar 里 actConnet 那行 connect
    //   从 showNotImplemented 改成本槽）：
    //   ① 状态栏先提示"连接中…"
    //   ② m_client->connectToDevice()
    //   ③ 防抖：已经连着/正在连就直接 return（想清楚怎么判断——isConnected()？）

    //已连接或连接中直接返回
    if(m_client->isConnected() || m_client->isConnecting()) return;

    //状态栏显示“连接中”
    m_connStatus->setText(QStringLiteral("连接中..."));

    //连接设备
    emit connectRequested();
}

void MainWindow::onDeviceConnected()
{
    // TODO(D5-10④) ① m_connStatus 显示 "已连接 127.0.0.1:1502"（可以从 config 拼）
    //              ② m_pollTimer->start()   ← 从这一刻起，每 500ms 问一次设备

    //状态栏显示已连接
    m_connStatus->setText(QStringLiteral("已连接 %1").arg(m_client->getEndpoint()));

    //开始轮询设备
    m_pollTimer->start();
}

void MainWindow::onDeviceDisconnected()
{
    // TODO(D5-10④) ① m_pollTimer->stop() ② m_connStatus 回到 "未连接"

    //计时器停止
    m_pollTimer->stop();

    //状态栏显示“未连接”
    m_connStatus->setText(QStringLiteral("未连接"));
}

void MainWindow::onParametersRead(const QVector<semieq::Parameter> &params)
{
    // TODO(D5-10⑤) 就一行：m_curve->appendSamples(params);
    //   ⚠️ 忍住不要在这里画图 —— 数据驱动绘制：这里只改数据，
    //      曲线内部自己 update() 请求重绘，画由 paintEvent 负责（P38 ④ 刚验收过）

    //给CurveWidget同步数据
    m_curve->appendSamples(params);
}

void MainWindow::onConnectionError(const QString &reason)
{
    // TODO(D5-10④) ① m_pollTimer->stop() ② 状态栏显示 "连接失败：" + reason

    //状态栏显示"连接失败：" + reason
    m_connStatus->setText(QStringLiteral("连接失败:%1").arg(reason));
}

} // namespace semieq
