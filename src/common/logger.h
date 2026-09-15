#ifndef SEMIEQ_LOGGER_H
#define SEMIEQ_LOGGER_H

// =============================================================================
//  分级日志器（线程安全的单例）
//
//  设计决策：
//   1. 用单例而不是到处传 Logger* —— 日志是横切关注点，传参会污染所有函数签名
//      （C++11 起函数内 static 变量初始化线程安全，见 logger.cpp）
//   2. 用 QMutex 保护写入 —— 采集线程 / 业务线程 / UI 线程都会写日志
//   3. 每条 flush —— 程序崩溃时日志不能丢，这是设备软件的硬要求
//   4. 不用 qDebug —— 它无法分级落盘、也无法统一带模块名
// =============================================================================

#include <QFile>
#include <QMutex>
#include <QObject>
#include <QString>
#include <QTextStream>

namespace semieq {

enum class LogLevel {
    Debug = 0,
    Info,
    Warning,
    Error
};

class Logger : public QObject
{
    Q_OBJECT

public:
    /// 全局唯一实例
    static Logger &instance();

    /// 打开日志文件（目录不存在会自动创建）。重复调用会切换文件。
    bool open(const QString &filePath, LogLevel minLevel = LogLevel::Info);

    /// 关闭日志文件
    void close();

    /// 写一条日志（线程安全）
    void log(LogLevel level, const QString &module, const QString &message);

    void     setMinLevel(LogLevel level) { m_minLevel = level; }
    LogLevel minLevel() const { return m_minLevel; }

    static QString levelName(LogLevel level);

private:
    explicit Logger(QObject *parent = nullptr);
    ~Logger() override;

    Logger(const Logger &)            = delete;
    Logger &operator=(const Logger &) = delete;

    QMutex      m_mutex;             ///< 保护 m_file / m_stream
    QFile       m_file;
    QTextStream m_stream;
    LogLevel    m_minLevel = LogLevel::Info;
};

} // namespace semieq

// ------------------------------------------------------------------ 便捷宏 ---
// 用法： LOG_INFO("ModbusTcpClient", QString("已连接 %1").arg(host));
#define LOG_DEBUG(mod, msg) ::semieq::Logger::instance().log(::semieq::LogLevel::Debug,   (mod), (msg))
#define LOG_INFO(mod, msg)  ::semieq::Logger::instance().log(::semieq::LogLevel::Info,    (mod), (msg))
#define LOG_WARN(mod, msg)  ::semieq::Logger::instance().log(::semieq::LogLevel::Warning, (mod), (msg))
#define LOG_ERROR(mod, msg) ::semieq::Logger::instance().log(::semieq::LogLevel::Error,   (mod), (msg))

#endif // SEMIEQ_LOGGER_H
