#ifndef SEMIEQ_MAINWINDOW_H
#define SEMIEQ_MAINWINDOW_H

// =============================================================================
//  主窗口
//
//  D1 骨架：先把"外壳"立起来 —— 菜单栏 / 状态栏 / 中央区域占位，
//  并在标题与「关于」里带上版本号（对应"软件更新"要求的最小可见证据）。
//
//  后续规划（对应排期）：
//    D6-D8  中央区域换成 QSplitter：左侧参数列表 + 右上实时曲线 + 右下数据表格
//    D9     状态栏接入连接状态与报警计数
//    D10-D11 增加「SPC 分析」「Wafer Map」页签
// =============================================================================

#include <QMainWindow>

class QLabel;

namespace semieq {

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void showAbout();
    void showNotImplemented();     ///< 占位：提示功能待实现（带功能名）

private:
    void setupMenuBar();
    void setupCentralArea();
    void setupStatusBar();

    QLabel *m_placeholder  = nullptr;
    QLabel *m_connStatus   = nullptr;
    QLabel *m_versionLabel = nullptr;
};

} // namespace semieq

#endif // SEMIEQ_MAINWINDOW_H
