#include "common/logger.h"

#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QMutexLocker>

#include <cstdio>

namespace semieq {

Logger &Logger::instance()
{
    // C++11 起，函数内 static 变量的初始化由编译器保证线程安全
    // （即使两个线程同时首次调用，也只会构造一次）
    static Logger s_instance;
    return s_instance;
}

Logger::Logger(QObject *parent)
    : QObject(parent)
{
}

Logger::~Logger()
{
    close();
}

bool Logger::open(const QString &filePath, LogLevel minLevel)
{
    QMutexLocker locker(&m_mutex);

    // 注意：这里不能调用 close()，因为 close() 也会加同一把锁 → 死锁。
    // 所以下面直接把已有文件关掉。
    if (m_file.isOpen()) {
        m_stream.flush();
        m_file.close();
    }

    const QFileInfo info(filePath);
    const QString   dir = info.absolutePath();
    if (!dir.isEmpty()) {
        QDir().mkpath(dir);        // 日志目录不存在就创建
    }

    m_minLevel = minLevel;
    m_file.setFileName(filePath);

    const bool ok = m_file.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
    if (ok) {
        m_stream.setDevice(&m_file);
    }
    return ok;
}

void Logger::close()
{
    QMutexLocker locker(&m_mutex);
    if (m_file.isOpen()) {
        m_stream.flush();
        m_file.close();
    }
}

void Logger::log(LogLevel level, const QString &module, const QString &message)
{
    // 先做无锁的等级过滤，避免无谓的加锁与字符串拼接
    if (level < m_minLevel) {
        return;
    }

    const QString line = QStringLiteral("[%1] [%2] [%3] %4")
                             .arg(QDateTime::currentDateTime()
                                      .toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz")))
                             .arg(levelName(level), 5)
                             .arg(module, 18)
                             .arg(message);

    QMutexLocker locker(&m_mutex);

    if (m_file.isOpen()) {
        m_stream << line << '\n';
        m_stream.flush();          // 崩溃时不丢日志（设备软件的硬要求）
    } else {
        // 未打开文件时退回标准错误输出，便于开发期在控制台观察
        QTextStream err(stderr);
        err << line << '\n';
        err.flush();
    }
}

QString Logger::levelName(LogLevel level)
{
    switch (level) {
    case LogLevel::Debug:   return QStringLiteral("DEBUG");
    case LogLevel::Info:    return QStringLiteral("INFO");
    case LogLevel::Warning: return QStringLiteral("WARN");
    case LogLevel::Error:   return QStringLiteral("ERROR");
    }
    return QStringLiteral("?");
}

} // namespace semieq
