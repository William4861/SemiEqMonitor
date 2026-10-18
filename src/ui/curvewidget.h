#ifndef SEMIEQ_CURVEWIDGET_H
#define SEMIEQ_CURVEWIDGET_H

// =============================================================================
//  实时曲线控件（自绘 · 不依赖 Qt Charts / QCustomPlot）
//
//  为什么自绘（docs/技术方案.md 已定）：
//    实时曲线只需"折线 + 坐标轴 + 规格线"，自绘更可控，且绘制原理能在面试里讲透。
//
//  数据从哪来（调用链 —— 面试可照此讲）：
//    设备/模拟器 → ModbusTcpClient::onReadyRead() --parametersRead(信号)->
//    MainWindow::onParametersRead() → 本控件 appendSamples()（唯一入口）
//    → appendSamples() 有数据写入时调 update()（P38：标记脏区域，不是立刻重绘）
//    → Qt 在下一轮事件循环调 paintEvent() → 按"当前数据"画图（P38：数据驱动绘制）
//
//  P0 显示策略（重要设计决策，面试点）：
//    数据层 = 多通道（每条曲线一份缓冲）；显示层 = "当前选中 1 个通道" + 下拉框切换。
//    原因：8 个通道量纲差异巨大（主轴转速 ~1500 rpm vs 真空度 ~0.5 kPa），
//    共享 Y 轴会把小信号压成一条直线；多 Y 轴 / 按通道归一化列为 P1。
//
//  绘制分解（paintEvent 里依次调用，每步只干一件事）：
//    paintEvent → plotArea() → computeRange() → drawGridAndAxes() → drawCurves() → drawLegend()
//
//  ⚠️ 所有绘制只允许发生在 paintEvent 及其调用链里（Qt 硬规定，见笔记 P38 ④）
// =============================================================================

#include "common/types.h"

#include <QColor>
#include <QRect>
#include <QWidget>

class QPainter;

namespace semieq {

class CurveWidget : public QWidget
{
    Q_OBJECT

public:
    explicit CurveWidget(QWidget *parent = nullptr);

    /// 设定通道定义（名称/单位/上下限来自 Parameter）；同时清空已有数据。
    /// 由 MainWindow 在启动时调用一次。
    void setChannels(const QVector<Parameter> &definitions);

    /// 追加上一批采样（采集层 parametersRead 原样喂进来）。
    /// 内部：按 code 匹配通道 → 追加 value → 裁剪到 capacity() → 有写入则请求重绘。
    void appendSamples(const QVector<Parameter> &samples);

    /// 切换"当前显示"的通道（越界忽略；切换后立即请求重绘）
    void setCurrentChannel(int index);

    /// 当前显示的通道下标
    int currentChannel() const { return m_current; }

    /// 每通道保留的采样点数（超出后丢最旧的）——默认 300（≈2.5 分钟 @500ms）
    int  capacity() const { return m_capacity; }
    void setCapacity(int n);

    /// 清空所有通道的数据（保留通道定义）
    void clear();

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    // ---- 数据模型 -----------------------------------------------------------
    struct Channel {
        QString code;                 ///< 采集点编码（与 Parameter::code 对齐，靠它匹配数据）
        QString name;                 ///< 中文名（图例显示）
        QString unit;                 ///< 单位
        double  lowerLimit = 0.0;     ///< LSL 规格下限（画虚线）
        double  upperLimit = 0.0;     ///< USL 规格上限
        QColor  color;                ///< 曲线颜色
        QVector<double> values;       ///< 最近 capacity() 个采样值（时间序，队尾最新）
    };

    // ---- 绘制分步（每个函数只干一件事，全部由 paintEvent 调用）---------------
    QRect plotArea() const;                                       // ① 算绘图区矩形（四边留白）
    void  computeRange(double &minValue, double &maxValue) const; // ② 算 Y 量程（数据 ∪ 规格线）
    void  drawGridAndAxes(QPainter &painter, const QRect &plot,
                          double minValue, double maxValue) const; // ③ 边框 + 网格 + Y 轴刻度
    void  drawCurves(QPainter &painter, const QRect &plot,
                     double minValue, double maxValue) const;      // ④ 规格虚线 + 折线
    void  drawLegend(QPainter &painter) const;                     // ⑤ 通道名 + 最新值

    static QColor channelColor(int index);   // 通道调色板（8 色循环）

    QVector<Channel> m_channels;
    int m_current  = 0;      ///< 当前显示的通道下标
    int m_capacity = 300;    ///< 每通道保留点数
};

} // namespace semieq

#endif // SEMIEQ_CURVEWIDGET_H
