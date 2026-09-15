#include "ui/mainwindow.h"

#include "common/version.h"     // SEMIEQ_APP_NAME / SEMIEQ_VERSION_STR / SEMIEQ_BUILD_TIME

// =============================================================================
//  ⚠️ 填空式骨架 —— 实现体由你补全（D6 任务，正好与 Qt 第二章「界面编写」同步）
//
//  为什么这块留给你写：
//    你接下来要学的 P17 讲的就是 QMainWindow 的菜单栏/工具栏/状态栏/中心部件 ——
//    这份文件就是那些知识点的直接应用。学完 P17 再回来填，是最省力的顺序。
//
//  对答案：git diff ref/skeleton -- src/ui/mainwindow.cpp
//  任务卡：docs/tasks/D6_主窗口界面.md（学到 P17 时我再给你）
//
//  已给你写好的部分：构造函数（它规定了这个窗口由哪几块拼成）+ 析构函数
// =============================================================================

namespace semieq {

// ------------------------------------------------------- 已实现（不用改） ---

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    // 窗口 = 菜单栏 + 中心区域 + 状态栏，三块各自一个 setup 函数
    setupMenuBar();
    setupCentralArea();
    setupStatusBar();

    setWindowTitle(QStringLiteral("%1 v%2 —— 半导体设备数据采集与监控")
                       .arg(QStringLiteral(SEMIEQ_APP_NAME), QStringLiteral(SEMIEQ_VERSION_STR)));
    resize(1280, 800);
}

MainWindow::~MainWindow() = default;

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
}

void MainWindow::setupCentralArea()
{
    // TODO(D6-2) 建中心区域。
    //   ① new 一个 QWidget 作为 central，setCentralWidget() 挂上去
    //   ② 给它加一个布局（QVBoxLayout）
    //   ③ 先放一个 QLabel 占位（成员 m_placeholder），内容写清"下一步要做什么"
    //      —— 这就是所谓的"可运行的骨架"，任何时候程序都能跑起来
    //
    //  💡 提示：布局要 new 在 central 上（parent 传 central），
    //     这样 central 析构时布局自动回收。
}

void MainWindow::setupStatusBar()
{
    // TODO(D6-3) 建状态栏。
    //   · 左侧（addWidget）：连接状态 m_connStatus，初始显示"未连接"
    //   · 右侧（addPermanentWidget）：版本号 m_versionLabel
    //
    //  ⚠️ addWidget 和 addPermanentWidget 的区别：后者不会被临时消息顶掉。
    //     所以"版本号"这种常驻信息要用 permanent —— 面试可能会问。
}

void MainWindow::showAbout()
{
    // TODO(D6-4) 弹出「关于」对话框（QMessageBox::about）。
    //   内容至少包含：应用名、版本号、一句话描述、
    //   技术栈（C++11 / Qt 5 / SQLite / Modbus）、构建时间、Qt 版本。
    //
    //  版本号宏 SEMIEQ_APP_NAME / SEMIEQ_VERSION_STR / SEMIEQ_BUILD_TIME
    //  来自 src/common/version.h（由 CMake 自动生成）—— 别硬编码版本号。
}

void MainWindow::showNotImplemented()
{
    // TODO(D6-5) 状态栏提示"某功能待实现"。
    //   ① 用 qobject_cast<QAction*>(sender()) 拿到是哪个 action 被点了
    //      —— sender() 是 QObject 提供的"谁给我发的信号"（P12 的延伸）
    //   ② 状态栏 showMessage("「XXX」计划在后续迭代实现", 3000)
    //      —— 第二个参数是自动清除的毫秒数
}

} // namespace semieq
