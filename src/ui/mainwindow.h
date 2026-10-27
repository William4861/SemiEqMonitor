#ifndef SEMIEQ_MAINWINDOW_H
#define SEMIEQ_MAINWINDOW_H

// =============================================================================
//  主窗口
//
//  D1 骨架：先把"外壳"立起来 —— 菜单栏 / 状态栏 / 中央区域占位，
//  并在标题与「关于」里带上版本号（对应"软件更新"要求的最小可见证据）。
//
//  后续规划（对应排期）：
//    ✅ D5     中央区域 = 通道选择器 + 实时曲线；「连接设备」接线采集（本文件已声明）
//    D6-D8  中央区域升级 QSplitter：左侧参数列表 + 右上实时曲线 + 右下数据表格
//    D9     状态栏接入报警计数
//    D10-D11 增加「SPC 分析」「Wafer Map」页签
// =============================================================================

#include "common/types.h"    // semieq::Parameter（onParametersRead 槽签名要用）

#include <QMainWindow>

class QComboBox;
class QLabel;
class QTimer;
class QThread;      // D6-2：采集工作线程

namespace semieq {

class CurveWidget;
class ModbusTcpClient;
class DatabaseManager;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    /// D6-3：注入持久层（由 main() 建好并打开后传进来；本类**不拥有**它，不负责 close/delete）
    void setDatabase(DatabaseManager *db) { m_db = db; }

private slots:
    void showAbout();
    void showNotImplemented();     ///< 占位：提示功能待实现（带功能名）

    // —— D5：采集接线（实现见 mainwindow.cpp 的 TODO(D5-10)）——
    void onConnectDevice();                                           ///< 菜单「连接设备」
    void onDeviceConnected();                                         ///< 连上 → 启动轮询定时器
    void onDeviceDisconnected();                                      ///< 断开 → 停定时器 + 状态栏
    void onParametersRead(const QVector<semieq::Parameter> &params);  ///< 一批采样 → 喂曲线
    void onConnectionError(const QString &reason);                    ///< 连接失败 → 状态栏提示

signals:
    // —— D6-2：跨线程「命令」通道 ——
    //   🔴 为什么需要：m_client 搬进工作线程后，UI 线程**不能直接调用**它的成员函数
    //      （会报 QSocketNotifier ... from another thread）。必须 emit 信号，
    //      由 AutoConnection 自动退化为 QueuedConnection，让槽在**工作线程**执行。
    //   实现提示：onConnectDevice() 里把 m_client->connectToDevice() 换成 emit connectRequested();
    void connectRequested();        ///< → ModbusTcpClient::connectToDevice（在工作线程执行）
    void disconnectRequested();     ///< → ModbusTcpClient::disconnectFromDevice（在工作线程执行）

private:
    void setupMenuBar();
    void setupCentralArea();
    void setupStatusBar();
    void setupAcquisition();       ///< D5：建客户端 + 轮询定时器 + 连信号（待实现）

    QLabel *m_connStatus   = nullptr;
    QLabel *m_versionLabel = nullptr;

    // —— D5：实时曲线 + 采集接线 ——
    CurveWidget     *m_curve           = nullptr;   ///< 中央实时曲线（自绘）
    QComboBox       *m_channelSelector = nullptr;   ///< 通道切换下拉框
    ModbusTcpClient *m_client          = nullptr;   ///< Modbus-TCP 客户端
    QTimer          *m_pollTimer       = nullptr;   ///< 采集轮询定时器（500ms → poll()）
    QThread         *m_acqThread       = nullptr;   ///< D6-2：采集工作线程（m_client 搬进去）
                                                    ///< ⚠️ 析构里必须先 quit() + wait() 再让它被删
    DatabaseManager *m_db              = nullptr;   ///< D6-3：持久层（外部注入，本类不拥有）
};

} // namespace semieq

#endif // SEMIEQ_MAINWINDOW_H
