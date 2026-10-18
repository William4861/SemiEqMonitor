#include "ui/curvewidget.h"
#include "common/logger.h"

#include <QColor>
#include <QPainter>
#include <QPen>
#include <QPointF>
#include <QPolygonF>
#include <QRect>

// =============================================================================
//  ⚠️ 填空式骨架 —— 实现体由你补全（D5 任务，正好用上刚验收完的 P36–P38 绘图知识）
//
//  任务卡：docs/tasks/D5_实时曲线.md（每个 TODO 的完整要求 + 前置小课 + 验收标准）
//  验收  ：scripts\build.bat → 0 error；run.bat sim + run.bat monitor → 曲线在动
//  对答案：git diff ref/skeleton -- src/ui/curvewidget.cpp（写完先自查，再发我批改）
//
//  已给你写好的部分：构造函数 + setCapacity / clear / setCurrentChannel
//  （后三个顺便示范了"改状态 → update() 请求重绘"的标准套路）
// =============================================================================

namespace semieq {

namespace {
// 绘图区四边留白（plotArea 用）——按视觉效果微调
constexpr int kMarginLeft   = 56;   // 左侧留给 Y 轴刻度数字
constexpr int kMarginRight  = 16;
constexpr int kMarginTop    = 10;
constexpr int kMarginBottom = 26;
}

// --------------------------------------------------------- 已实现（不用改） ---

CurveWidget::CurveWidget(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(480, 260);
}

void CurveWidget::setCapacity(int n)
{
    m_capacity = qMax(2, n);
    for (Channel &ch : m_channels) {
        while (ch.values.size() > m_capacity) {
            ch.values.removeFirst();
        }
    }
    update();
}

void CurveWidget::clear()
{
    for (Channel &ch : m_channels) {
        ch.values.clear();
    }
    update();
}

void CurveWidget::setCurrentChannel(int index)
{
    // ⭐ 标准套路：改"状态" → update() 请求重绘（画面永远跟着数据和状态走，P38 的机制）
    if (index < 0 || index >= m_channels.size() || index == m_current) {
        return;
    }
    m_current = index;
    update();
}

// ====================================================== 待你实现（D5 任务） ===

void CurveWidget::setChannels(const QVector<Parameter> &definitions)
{
    // TODO(D5-1) 换一批通道定义（MainWindow 启动时调一次）：
    //   ① 清空 m_channels；m_current 归零
    //   ② 遍历 definitions 逐个建 Channel：
    //      code / name / unit / lowerLimit / upperLimit 原样抄进 Channel 对应字段；
    //      color = channelColor(下标)；values 保持空
    //   ③ 别忘了请求重绘 update()

    //清空m_channels和m_current归零
    m_channels.clear();
    m_current=0;

    //遍历definitions复制到m_channels
    for(const Parameter &param : definitions)
    {
        Channel channel;
        channel.code = param.code;
        channel.name = param.name;
        channel.unit = param.unit;
        channel.lowerLimit = param.lowerLimit;
        channel.upperLimit = param.upperLimit;
        channel.color = channelColor(m_channels.size());//m_channels.size()刚好等于每个通道的序号

        m_channels.append(channel);
    }

    //请求重绘
    update();
}

void CurveWidget::appendSamples(const QVector<Parameter> &samples)
{
    // TODO(D5-2) 曲线吃数据的唯一入口（MainWindow 收到 parametersRead 后调）：
    //   ① 遍历 samples，按 code 在 m_channels 里找对应通道（数据量小，双层循环就够）
    //   ② 找到 → 把 value 追加进该通道 values 队尾；超过 capacity() 就丢最旧的
    //      （removeFirst 一次删一个，或者算出超量后一次 mid(...) 截断——想想哪个更省）
    //   ③ 记录"本轮有没有真的写入"——只有写入了才 update()
    //      （一次都没匹配上就白重绘一次，没必要）
    
    //标记是否有样本写入
    bool isWrote = false;

    for(const Parameter & param:samples)
    {
        for(Channel &channel:m_channels )
        {
            //遍历对比code找到对应通道
            if(param.code == channel.code)
            {
                //将sample数据加入channel数据队尾
                channel.values.append(param.value);
                //如果超过capacity()就丢最旧的
                while(channel.values.size()>m_capacity)
                {
                    channel.values.removeFirst();
                }
                isWrote = true;
                //每个样本只对应一个通道，找到了通道可以结束这层循环了
                break;
            }
        }
    }
    //如果写入了样本数据请求重绘
    if(isWrote) update();

}

QColor CurveWidget::channelColor(int index)
{
    // TODO(D5-3) 通道调色板：switch (index % 8) 返回 8 个区分度高的颜色：
    //   0 红(231,76,60)   1 蓝(52,152,219)  2 绿(46,204,113)  3 橙(243,156,18)
    //   4 紫(155,89,182)  5 青(26,188,156)  6 黄(241,196,15)  7 灰蓝(149,165,166)
    //   默认返回深灰(100,100,100)（颜色顺手挑，这组只是建议）

    switch (index%8)
    {
    case 0:
        return QColor(231,76,60);//红色
    case 1:
        return QColor(52,152,219);//蓝色
    case 2:
        return QColor(46,204,113);//绿色
    case 3:
        return QColor(243,156,18);//橙色
    case 4:
        return QColor(155,89,182);//紫色
    case 5:
        return QColor(26,188,156);//青色
    case 6:
        return QColor(241,196,15);//黄色
    case 7:
        return QColor(149,165,166);//灰蓝色
    default:
        return QColor(100,100,100);//深灰色

    }
}

QRect CurveWidget::plotArea() const
{
    // TODO(D5-4) 算"绘图区"矩形（四周给坐标轴/标签留白，常数在文件顶部）：
    //   QRect(kMarginLeft, kMarginTop,
    //         width() - kMarginLeft - kMarginRight,
    //         height() - kMarginTop - kMarginBottom)
    //   （width()/height() 是控件当前尺寸；窗口拉伸时自动变大变小）
    return QRect();
}

void CurveWidget::computeRange(double &minValue, double &maxValue) const
{
    // TODO(D5-5) 算当前显示通道的 Y 量程（两个出参——所以用引用 `&`）：
    //   ① 通道没数据（或没通道）→ 给个默认量程先返回（比如 0 ~ 100），画面不至于空
    //   ② 遍历 values 拿 min/max；再把 lowerLimit / upperLimit 并进来
    //      （为什么：规格线必须在画面里看得见）
    //   ③ ⚠️ 防除零：如果 max-min 小到几乎为 0（数据几乎一条直线），上下各撑开一点
    //      （想想不撑开会怎样——映射公式的分母就是它）
}

void CurveWidget::drawGridAndAxes(QPainter &painter, const QRect &plot,
                                  double minValue, double maxValue) const
{
    // TODO(D5-6) 边框 + 网格 + Y 轴刻度数字：
    //   ① 浅灰 pen（比如 QColor(200,200,200)）先画 plot 边框（drawRect）
    //   ② 横向 5 条线（含上下边）：for i in 0..4 画横线，y = plot.top() + i * plot.height() / 4
    //      每条线对应的"值" = maxValue - i * (maxValue - minValue) / 4
    //      ⚠️ 方向：最上面那条 = maxValue，最下面 = minValue
    //   ③ 刻度数字：每行在左侧留白里画一个文字（保留 1 位小数：QString::number(值, 'f', 1)）
    //      对齐用带 flags 的 drawText 重载（见任务卡 §3 的小补充）
    //   ④ 纵向 5 条竖线（不配文字）
}

void CurveWidget::drawCurves(QPainter &painter, const QRect &plot,
                             double minValue, double maxValue) const
{
    // TODO(D5-7) 规格线 + 折线（本控件的核心）：
    //   ① 边界情况：当前通道没数据 → 直接 return
    //   ② 规格线（两条水平虚线：setStyle(Qt::DashLine)，红/橙色 1px）：
    //      LSL 的 y = 用映射公式把 lowerLimit 算成 y；USL 同理
    //      （量程里已并进上下限，正常都落在画面内——改动逻辑时记得复查）
    //   ③ 折线：
    //      QPolygonF poly; 逐点 poly << QPointF(x, y);
    //      painter.setRenderHint(QPainter::Antialiasing);   ← 画曲线必开（P37）
    //      painter.setPen(QPen(颜色, 2)); painter.drawPolyline(poly);
    //   映射公式（x 按序号、y 按量程）：
    //      x = plot.left()   + (double)序号 / (capacity() - 1) * plot.width()
    //      y = plot.bottom() - (值 - minValue) / (maxValue - minValue) * plot.height()
    //   （QPolygonF 细节 → 任务卡 §3 小课；也可直接跑
    //     笔记库/QT/示例代码/demo_D5_曲线映射_QPolygonF_已编译通过.cpp 看输出与配图）
}

void CurveWidget::drawLegend(QPainter &painter) const
{
    // TODO(D5-8) 左上角图例：通道名 + 最新值 + 单位，例如 "腔体温度  45.3 ℃"
    //   ① 取当前通道最新值：values 非空 → values.last()；空 → 文字 "（暂无数据）"
    //   ② 用通道颜色的 pen 画文字（drawText 的"点"或"小矩形"重载都行）
    //   ③ 位置建议：绘图区左上角往内收一点，别压在边框线上
}

void CurveWidget::paintEvent(QPaintEvent *event)
{
    // TODO(D5-9) 组装五步（这里只"指挥"，具体画法都在上面几个函数里）：
    //   ① QPainter painter(this);
    //   ② 铺底色：painter.fillRect(rect(), QColor(255,255,255));
    //      （rect() = 控件整个矩形 (0,0,宽,高)）
    //   ③ const QRect plot = plotArea();
    //   ④ double minV = 0.0, maxV = 0.0; computeRange(minV, maxV);
    //   ⑤ 依次调 drawGridAndAxes → drawCurves → drawLegend（参数照着签名传）
    //   ⚠️ 事件对象没用到 → (void)event;（小习惯：告诉编译器"故意的"）
    //   ⚠️ 千万别在这里再调 update()（P38：自触发 → 死循环）
    (void)event;
}

} // namespace semieq
