#include "data/databasemanager.h"

// =============================================================================
//  ⚠️ 填空式骨架 —— 实现体由你补全（D4 任务）
//
//  为什么这个文件值得你亲手写：
//    明锐理想 #4 / 大族 #6 等岗位的 JD 都明确要求"熟悉一种关系型数据库"，
//    面试也常问 SQL 与"预编译语句为什么能防注入"。这里正好覆盖：
//      建表 DDL → 预编译 INSERT → SELECT 遍历 → 事务
//
//  对答案：git diff ref/skeleton -- src/data/databasemanager.cpp
//  任务卡：docs/tasks/D4_SQLite持久层.md
//  设计说明：docs/数据库设计.md（**注意：它是"需求"，不是"答案"**）
//
//  已给你写好的部分：构造函数 / 析构函数 / defaultDatabasePath()
// =============================================================================

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QSqlDatabase>
#include <QSqlError>
#include <QDebug>
#include <QSqlQuery>
#include "common/logger.h"

namespace semieq {

// （D4 会用到，先注释掉避免未使用告警 —— 你实现时把它们放开）
// #include <QFileInfo>
// #include <QSqlError>
// #include <QSqlQuery>
// #include <QVariant>
// #include "common/logger.h"

// ------------------------------------------------------- 已实现（不用改） ---

DatabaseManager::DatabaseManager()
    : m_connectionName(QStringLiteral("semieq_main"))
{
}

DatabaseManager::~DatabaseManager()
{
    close();
}

QString DatabaseManager::defaultDatabasePath()
{
    return QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("data/semieq.db"));
}

// =================================================== 待你实现（D4 任务） ===

QStringList DatabaseManager::schemaStatements()
{
    // TODO(D4-1) 返回建表 SQL 列表 —— 直接照着 docs/数据库设计.md 抄。
    //
    //  要建 5 张表 + 3 个索引：
    //    equipment      设备台账（code 唯一）
    //    lot            批次
    //    wafer          晶圆（含 pass_count / fail_count / yield）
    //    parameter_log  采集数据（**数据量最大的一张**）
    //    alarm          报警（cleared_at 为 NULL 表示"尚未恢复"）
    //    + idx_param_log_code_time、idx_alarm_active、idx_die_wafer
    //    （还有 die_result 表 —— 看 docs/数据库设计.md，别漏）
    //
    //  ⚠️ 三个关键点：
    //    1. 每条都带 IF NOT EXISTS —— 因为 initSchema() 每次启动都会调，
    //       必须"跑几次都一样"（幂等）
    //    2. 时间字段一律 TEXT（SQLite 没有原生日期类型，存 ISO8601 文本）
    //    3. **为什么要给 parameter_log 建索引？**
    //       因为它每秒都在写、而且查询永远是"按参数+时间"——
    //       索引就是为这个查询模式建的。面试会问"为什么这里要加索引"。
    //
    //  验收：D4 手工验证 —— 启动程序后看有没有生成 semieq.db，
    //        再用 DB Browser for SQLite 打开看表结构对不对

    QStringList list;
    list << QStringLiteral(
        "CREATE TABLE IF NOT EXISTS equipment ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "code TEXT NOT NULL UNIQUE,"
        "model TEXT,"
        "location TEXT,"
        "state INTEGER NOT NULL DEFAULT 0,"
        "created_at TEXT NOT NULL"
        ")"
    );
    list << QStringLiteral(
        "CREATE TABLE IF NOT EXISTS lot ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "lot_id TEXT NOT NULL UNIQUE,"
        "product_model TEXT,"
        "planned_wafers INTEGER DEFAULT 0,"
        "started_at TEXT,"
        "finished_at TEXT"
        ")"
    );
    list << QStringLiteral(
        "CREATE TABLE IF NOT EXISTS wafer ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "wafer_id TEXT NOT NULL,"
        "lot_id TEXT NOT NULL,"
        "slot INTEGER NOT NULL,"
        "cols INTEGER NOT NULL,"
        "rows INTEGER NOT NULL,"
        "pass_count INTEGER DEFAULT 0,"
        "fail_count INTEGER DEFAULT 0,"
        "yield REAL DEFAULT 0,"
        "started_at TEXT,"
        "finished_at TEXT"
        ")"
    );
    list << QStringLiteral(
        "CREATE TABLE IF NOT EXISTS parameter_log ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "param_code TEXT NOT NULL,"
        "param_name TEXT,"
        "unit TEXT,"
        "value REAL NOT NULL,"
        "recorded_at TEXT NOT NULL"
        ")"
    );
    list << QStringLiteral("CREATE INDEX IF NOT EXISTS idx_param_log_code_time ON parameter_log(param_code,recorded_at)");
    list << QStringLiteral(
        "CREATE TABLE IF NOT EXISTS alarm ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "param_code TEXT,"
        "level INTEGER NOT NULL,"
        "message TEXT,"
        "trigger_value REAL,"
        "raised_at TEXT NOT NULL,"
        "cleared_at TEXT,"
        "acknowledged INTEGER DEFAULT 0,"
        "ack_by TEXT"
        ")"
    );
    list << QStringLiteral("CREATE INDEX IF NOT EXISTS idx_alarm_active ON alarm(cleared_at,raised_at)");
    list << QStringLiteral(
        "CREATE TABLE IF NOT EXISTS die_result ("
        "id INTEGER PRIMARY KEY AUTOINCREMENT,"
        "wafer_id TEXT NOT NULL,"
        "lot_id TEXT NOT NULL,"
        "x INTEGER NOT NULL,"
        "y INTEGER NOT NULL,"
        "result INTEGER NOT NULL,"
        "fail_code TEXT"
        ")"
    );
    list << QStringLiteral("CREATE INDEX IF NOT EXISTS idx_die_wafer ON die_result(wafer_id)");

    return list;
}

bool DatabaseManager::open(const QString &dbPath)
{
    // TODO(D4-2) 打开数据库。顺序很重要：
    //   ① 清空 m_lastError
    //   ② 文件所在目录可能不存在（data/ 是运行时才建的）→ QDir().mkpath()
    //   ③ **先检查驱动**：QSqlDatabase::isDriverAvailable("QSQLITE")
    //      —— 驱动缺失时报错信息要写清楚，否则排查半天
    //   ④ QSqlDatabase::addDatabase("QSQLITE", m_connectionName)
    //      —— 用【命名连接】而不是默认连接（为什么？见 databasemanager.h 顶部注释）
    //   ⑤ setDatabaseName(dbPath) 然后 open()，失败要记录 m_db.lastError().text()

    //先清空m_lastError，原因待补充
    m_lastError.clear();

    //.db文件目录可能还没创建，dbPtah是文件路径不是目录
    QDir().mkpath(QFileInfo(dbPath).absolutePath());

    //检查SqLite驱动
    if(!QSqlDatabase::isDriverAvailable("QSQLITE"))
    {
        m_lastError = QStringLiteral("缺少SqLite驱动");
        qDebug()<< m_lastError;
        LOG_ERROR("DatabaseManager",m_lastError);
        return false;
    }

    //通过句柄m_db连接SqLite数据库，通过命名连接而非默认连接
    m_db = QSqlDatabase::addDatabase("QSQLITE",m_connectionName);

    //设置.db文件路径，并尝试打开数据库，若失败则记录
    m_db.setDatabaseName(dbPath);
    if(!m_db.open())
    {
        m_lastError = m_db.lastError().text();
        qDebug() << m_lastError;
        LOG_ERROR("DatabaseManager",m_lastError);
        return false;
    }

    return true;
}

void DatabaseManager::close()
{
    // TODO(D4-3) 关闭并注销连接。这里有【顺序陷阱】：
    //   ① 如果还开着，先 m_db.close()
    //   ② 必须先把 m_db 置为无效对象（m_db = QSqlDatabase();）
    //      再 QSqlDatabase::removeDatabase(...)
    //   —— 否则 Qt 会警告 "connection is still in use"：
    //      因为 m_db 这个句柄还占着那条连接。
    
    //先关闭数据库连接
    m_db.close();

    //注销连接，先置空引用变量，再删除连接记录，若连接记录显示引用数>1则删除时会报错"connection is stil using"
    m_db = QSqlDatabase();
    QSqlDatabase::removeDatabase(m_connectionName);
}

bool DatabaseManager::isOpen() const
{
    // TODO(D4-4) 一行：返回底层数据库是否已打开
    if(!m_db.isOpen()) return false;

    return true;
}

bool DatabaseManager::initSchema()
{
    // TODO(D4-5) 执行建表语句。
    //   ① 库没打开就直接失败（顺手设 m_lastError）
    //   ② 取 schemaStatements()，用 QSqlQuery 逐条 exec()
    //   ③ 任意一条失败：记录 SQL + 错误原因，返回 false
    //   ④ 全部成功：记一条日志，写明执行了几条语句
    //
    //  💡 进阶（可选）：把整组建表语句包在一个事务里。
    //     这样失败时不会留下"建到一半"的库。
    //     QSqlDatabase::transaction() / commit()。

    //数据库未打开就记录后返回false
    if(!m_db.isOpen()) 
    {
        m_lastError = QStringLiteral("数据库未打开");
        return false;
    }

    //用事务包装
    if(!m_db.transaction())
    {
        m_lastError = m_db.lastError().text();
        return false;
    }

    //取 schemaStatements()，用 QSqlQuery 逐条 exec()，若Sql执行失败记录并rollback再返回false
    QStringList statements = schemaStatements();
    QSqlQuery query(m_db);
    for(const QString &sql : statements)
    {
        if(!query.exec(sql))
        {
            m_lastError = QStringLiteral("建表失败：%1\nSql语句：%2").arg(query.lastError().text(),sql);
            m_db.rollback();//若Sql语句执行失败就撤销事务内的所有操作
            return false;
        }
    }

    //事务完成提交，若提交失败则记录并返回false
    if(!m_db.commit())
    {
        m_lastError = m_db.lastError().text();
        return false;
    }

    //Sql语句全部执行成功则记录且返回true
    LOG_INFO("DatabaseManager",QStringLiteral("建表完成，共执行 %1 条语句").arg(statements.size()));

    return true;
}

bool DatabaseManager::insertParameter(const Parameter &p)
{
    // TODO(D4-6) 写入一条采集数据。
    //
    //  必须用【预编译语句】而不是拼字符串：
    //      query.prepare("INSERT INTO parameter_log(...) VALUES(:code, ...)");
    //      query.bindValue(":code", p.code);
    //      ...
    //      query.exec();
    //
    //  为什么？（面试必问）
    //    ① 防 SQL 注入 —— 参数值不会被当作 SQL 语法解析
    //    ② 性能 —— 同一条语句重复执行时，数据库只需解析一次
    //
    //  ⚠️ 这个函数每秒会被调用 8 次（8 个采集点），
    //     所以它必须是"最快的写法"。想一想：每次都 prepare 值得吗？
    Q_UNUSED(p)
    return false;
}

bool DatabaseManager::insertAlarm(const Alarm &a)
{
    // TODO(D4-7) 写入一条报警。和 insertParameter 同构，但要注意：
    //   ① level 是枚举 → 存进库转成 int
    //   ② acknowledged 是 bool → 存成 0/1
    //   ③ clearedAt 可能是【无效时间】（报警还没恢复）→ 存 NULL 而不是空字符串！
    //      这一条直接决定 activeAlarms() 能不能正确查到"未恢复的报警"
    Q_UNUSED(a)
    return false;
}

QVector<Parameter> DatabaseManager::recentParameters(const QString &paramCode, int limit) const
{
    // TODO(D4-8) 查最近 N 条某采集点的数据（时间倒序）。
    //   ① prepare + bindValue（注意 LIMIT 也可以用占位符）
    //   ② exec 失败就记录错误并返回空表
    //   ③ while (query.next()) 按【列下标】取值：query.value(0).toString() ...
    //      —— 下标要和 SELECT 里列的顺序严格对上
    //   ④ 时间字段用 QDateTime::fromString(text, Qt::ISODate) 还原
    Q_UNUSED(paramCode)
    Q_UNUSED(limit)
    return QVector<Parameter>();
}

QVector<Alarm> DatabaseManager::activeAlarms() const
{
    // TODO(D4-9) 查所有"尚未恢复"的报警。
    //
    //  ⚠️ 关键 SQL 知识点：判断 NULL 要用 `IS NULL`，不能写 `= NULL`
    //     （SQL 里 NULL 和任何值比较结果都是未知，`= NULL` 永远查不到东西）
    return QVector<Alarm>();
}

} // namespace semieq
