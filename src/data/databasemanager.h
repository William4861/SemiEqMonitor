#ifndef SEMIEQ_DATABASEMANAGER_H
#define SEMIEQ_DATABASEMANAGER_H

// =============================================================================
//  SQLite 持久层
//
//  设计决策：
//   1. 用命名连接（QSqlDatabase::addDatabase(name)）而不是默认连接 ——
//      便于后续同时挂"本地缓存库"和"归档库"，也避免跨线程共用一个连接句柄
//   2. 建表语句集中放在 schemaStatements()，既供 initSchema() 执行，
//      也能被单元测试和 docs/数据库设计.md 引用（单一事实来源）
//   3. 时间统一以 ISO8601 文本存储 —— SQLite 无原生日期类型，
//      文本可排序、可比较、可直接被人类阅读
// =============================================================================

#include "common/types.h"

#include <QSqlDatabase>
#include <QString>
#include <QStringList>

namespace semieq {

class DatabaseManager
{
public:
    DatabaseManager();
    ~DatabaseManager();

    DatabaseManager(const DatabaseManager &)            = delete;
    DatabaseManager &operator=(const DatabaseManager &) = delete;

    /// 打开（或创建）数据库文件
    bool open(const QString &dbPath);

    /// 关闭并注销连接
    void close();

    bool isOpen() const;

    /// 最近一次错误描述
    QString lastError() const { return m_lastError; }

    /// 建表（幂等，内部为 IF NOT EXISTS），启动时调用
    bool initSchema();

    // -------------------------------------------------------------- 写入 ---
    bool insertParameter(const Parameter &p);
    bool insertAlarm(const Alarm &a);

    // -------------------------------------------------------------- 查询 ---
    /// 最近 N 条某采集点的记录（时间倒序）
    QVector<Parameter> recentParameters(const QString &paramCode, int limit = 100) const;

    /// 尚未恢复的报警
    QVector<Alarm> activeAlarms() const;

    // -------------------------------------------------------------- 元信息 ---
    /// 建表语句（SQLite 方言）。文档与测试的唯一来源。
    static QStringList schemaStatements();

    /// 数据库文件默认路径（放在可执行文件旁）
    static QString defaultDatabasePath();

private:
    QSqlDatabase m_db;
    QString      m_connectionName;
    mutable QString m_lastError;
};

} // namespace semieq

#endif // SEMIEQ_DATABASEMANAGER_H
