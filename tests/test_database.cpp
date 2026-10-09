// =============================================================================
//  D4 第 6~9 步 · 持久层读写测试（insertParameter / insertAlarm / 两个查询）
//
//  为什么单独一个可执行：
//    Qt Test 一个可执行只能有一个 QTEST_*_MAIN，所以这组单独编译成
//    semieq_db_tests.exe。
//
//  设计要点：
//    1. 每个用例都用【独立的临时库】—— init() 建、cleanup() 删，
//       所以用例之间互不影响、可以任意单独跑（-functions 列表挑着跑也行）
//    2. 不碰 defaultDatabasePath()（那是 build/bin/data/semieq.db，
//       真机跑程序用的），一律用 QDir::tempPath() 下的临时文件
//    3. 断言里带了【失败原因提示】—— 失败时直接告诉你该去哪个函数找问题
//
//  运行： scripts\run.bat dbtest
//     或： build\bin\semieq_db_tests.exe
//     挑单个： build\bin\semieq_db_tests.exe recentParametersRespectsLimit
// =============================================================================

#include "data/databasemanager.h"

#include <QDir>
#include <QFile>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlRecord>
#include <QtTest/QtTest>

using namespace semieq;

class TestDatabase : public QObject
{
    Q_OBJECT

private slots:
    // ---- 生存周期：每个用例一套干净的库 ----
    void init();
    void cleanup();

    // ---- D4-2~D4-5 回归（顺手把前面的成果锁住，防止改坏） ----
    void opensDatabase();
    void initSchemaCreatesSixTablesAndThreeIndexes();
    void initSchemaIsIdempotent();
    void closeMakesIsOpenFalse();

    // ---- D4-6 insertParameter ----
    void insertParameterWritesRowThatCanBeReadBack();
    void insertParameterStoresUnitAndName();

    // ---- D4-7 insertAlarm ----
    void insertAlarmWritesRow();
    void insertAlarmStoresLevelAsInteger();
    void insertAlarmKeepsClearedAtNullWhenNotCleared();

    // ---- D4-8 recentParameters ----
    void recentParametersReturnsNewestFirst();
    void recentParametersRespectsLimit();
    void recentParametersFiltersByParamCode();
    void recentParametersOnEmptyTableReturnsEmpty();

    // ---- D4-9 activeAlarms ----
    void activeAlarmsReturnsOnlyUnclearedOnes();
    void activeAlarmsReturnsEmptyWhenAllCleared();

    // ---- 健壮性：库没打开时必须失败而不是崩 ----
    void insertParameterFailsWhenDatabaseClosed();

private:
    QString          m_dbPath;
    DatabaseManager *m_db = nullptr;
};

// ---------------------------------------------------------------------------
//  init / cleanup
// ---------------------------------------------------------------------------
void TestDatabase::init()
{
    m_dbPath = QDir::tempPath()
             + QStringLiteral("/semieq_test_%1.db")
                   .arg(QCoreApplication::applicationPid());
    QFile::remove(m_dbPath);

    m_db = new DatabaseManager;
    QVERIFY2(m_db->open(m_dbPath),
             qPrintable(QStringLiteral("open() 失败：") + m_db->lastError()));

    QVERIFY2(m_db->initSchema(),
             qPrintable(QStringLiteral("initSchema() 失败：") + m_db->lastError()));
}

void TestDatabase::cleanup()
{
    if (m_db) {
        m_db->close();
        delete m_db;
        m_db = nullptr;
    }
    QFile::remove(m_dbPath);
}

// ---------------------------------------------------------------------------
//  D4-2~D4-5 回归
// ---------------------------------------------------------------------------
void TestDatabase::opensDatabase()
{
    QVERIFY(m_db->isOpen());
    QVERIFY(QFile::exists(m_dbPath));
}

void TestDatabase::initSchemaCreatesSixTablesAndThreeIndexes()
{
    QSqlQuery q(QSqlDatabase::database(QStringLiteral("semieq_main")));

    QStringList tables;
    QVERIFY(q.exec(QStringLiteral(
        "SELECT name FROM sqlite_master WHERE type='table' "
        "AND name NOT LIKE 'sqlite_%' ORDER BY name")));
    while (q.next()) {
        tables << q.value(0).toString();
    }

    const QStringList expected = {QStringLiteral("alarm"),
                                  QStringLiteral("die_result"),
                                  QStringLiteral("equipment"),
                                  QStringLiteral("lot"),
                                  QStringLiteral("parameter_log"),
                                  QStringLiteral("wafer")};
    for (const QString &t : expected) {
        QVERIFY2(tables.contains(t),
                 qPrintable(QStringLiteral("缺表 %1 —— 检查 schemaStatements() 的 DDL；"
                                           "现有：" ).arg(t) + tables.join(QStringLiteral(","))));
    }

    QStringList indexes;
    QVERIFY(q.exec(QStringLiteral(
        "SELECT name FROM sqlite_master WHERE type='index' "
        "AND name NOT LIKE 'sqlite_%' ORDER BY name")));
    while (q.next()) {
        indexes << q.value(0).toString();
    }
    QCOMPARE(indexes.size(), 3);
    QVERIFY(indexes.contains(QStringLiteral("idx_param_log_code_time")));
    QVERIFY(indexes.contains(QStringLiteral("idx_alarm_active")));
    QVERIFY(indexes.contains(QStringLiteral("idx_die_wafer")));
}

void TestDatabase::initSchemaIsIdempotent()
{
    // 启动时每次都会调，跑第二遍必须照样成功（DDL 里要带 IF NOT EXISTS）
    QVERIFY2(m_db->initSchema(),
             qPrintable(QStringLiteral("第二次 initSchema() 失败：") + m_db->lastError()));
    QVERIFY(m_db->initSchema());
}

void TestDatabase::closeMakesIsOpenFalse()
{
    m_db->close();
    QVERIFY(!m_db->isOpen());
}

// ---------------------------------------------------------------------------
//  D4-6 insertParameter
// ---------------------------------------------------------------------------
void TestDatabase::insertParameterWritesRowThatCanBeReadBack()
{
    Parameter p;
    p.code      = QStringLiteral("TEMP_CH1");
    p.name      = QStringLiteral("腔体温度");
    p.unit      = QStringLiteral("℃");
    p.value     = 85.5;
    p.timestamp = QDateTime(QDate(2026, 10, 3), QTime(12, 0, 0));

    QVERIFY2(m_db->insertParameter(p),
             qPrintable(QStringLiteral("insertParameter() 返回 false：") + m_db->lastError()));

    const QVector<Parameter> rows = m_db->recentParameters(QStringLiteral("TEMP_CH1"));
    QCOMPARE(rows.size(), 1);

    QCOMPARE(rows.at(0).code,  QStringLiteral("TEMP_CH1"));
    QCOMPARE(rows.at(0).name,  QStringLiteral("腔体温度"));
    QCOMPARE(rows.at(0).unit,  QStringLiteral("℃"));
    QCOMPARE(rows.at(0).value, 85.5);
    QCOMPARE(rows.at(0).timestamp, p.timestamp);
}

void TestDatabase::insertParameterStoresUnitAndName()
{
    Parameter p;
    p.code      = QStringLiteral("PRESS_CH2");
    p.name      = QStringLiteral("腔体压力");
    p.unit      = QStringLiteral("kPa");
    p.value     = 101.5;
    p.timestamp = QDateTime(QDate(2026, 10, 3), QTime(12, 0, 1));
    QVERIFY(m_db->insertParameter(p));

    // 直接查库确认列名没写错（param_code / param_name / unit / value / recorded_at）
    QSqlQuery q(QSqlDatabase::database(QStringLiteral("semieq_main")));
    QVERIFY(q.exec(QStringLiteral(
        "SELECT param_code, param_name, unit, value FROM parameter_log")));
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toString(), QStringLiteral("PRESS_CH2"));
    QCOMPARE(q.value(1).toString(), QStringLiteral("腔体压力"));
    QCOMPARE(q.value(2).toString(), QStringLiteral("kPa"));
    QCOMPARE(q.value(3).toDouble(), 101.5);
}

// ---------------------------------------------------------------------------
//  D4-7 insertAlarm
// ---------------------------------------------------------------------------
static Alarm makeAlarm(const QString &code, AlarmLevel level,
                       const QDateTime &raised, const QDateTime &cleared = QDateTime())
{
    Alarm a;
    a.paramCode    = code;
    a.level        = level;
    a.message      = QStringLiteral("测试报警");
    a.triggerValue = 99.0;
    a.raisedAt     = raised;
    a.clearedAt    = cleared;
    a.acknowledged = false;
    return a;
}

void TestDatabase::insertAlarmWritesRow()
{
    const QDateTime raised(QDate(2026, 10, 3), QTime(13, 0, 0));
    QVERIFY2(m_db->insertAlarm(makeAlarm(QStringLiteral("TEMP_CH1"),
                                         AlarmLevel::Warning, raised)),
             qPrintable(QStringLiteral("insertAlarm() 返回 false：") + m_db->lastError()));

    QSqlQuery q(QSqlDatabase::database(QStringLiteral("semieq_main")));
    QVERIFY(q.exec(QStringLiteral("SELECT COUNT(*) FROM alarm")));
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), 1);
}

void TestDatabase::insertAlarmStoresLevelAsInteger()
{
    const QDateTime raised(QDate(2026, 10, 3), QTime(13, 1, 0));
    // Critical == 2（枚举值）
    QVERIFY(m_db->insertAlarm(makeAlarm(QStringLiteral("VAC_CH3"),
                                        AlarmLevel::Critical, raised)));

    QSqlQuery q(QSqlDatabase::database(QStringLiteral("semieq_main")));
    QVERIFY(q.exec(QStringLiteral("SELECT level FROM alarm")));
    QVERIFY(q.next());
    QCOMPARE(q.value(0).toInt(), static_cast<int>(AlarmLevel::Critical));

    // 回读也要还原成枚举
    const QVector<Alarm> active = m_db->activeAlarms();
    QCOMPARE(active.size(), 1);
    QCOMPARE(active.at(0).level, AlarmLevel::Critical);
}

void TestDatabase::insertAlarmKeepsClearedAtNullWhenNotCleared()
{
    const QDateTime raised(QDate(2026, 10, 3), QTime(13, 2, 0));
    QVERIFY(m_db->insertAlarm(makeAlarm(QStringLiteral("TEMP_CH1"),
                                        AlarmLevel::Warning, raised)));

    // ⚠️ 这一条就是 D4-9 的前提：没恢复的报警必须存成 NULL，不能存空字符串
    QSqlQuery q(QSqlDatabase::database(QStringLiteral("semieq_main")));
    QVERIFY(q.exec(QStringLiteral("SELECT cleared_at FROM alarm")));
    QVERIFY(q.next());
    QVERIFY2(q.value(0).isNull(),
             "cleared_at 必须绑成 NULL —— 如果绑了 QDateTime() 的 toString()，"
             "会存成空字符串 ''，那 activeAlarms() 的 IS NULL 就永远查不到");
}

// ---------------------------------------------------------------------------
//  D4-8 recentParameters
// ---------------------------------------------------------------------------
void TestDatabase::recentParametersReturnsNewestFirst()
{
    const QDateTime base(QDate(2026, 10, 3), QTime(14, 0, 0));

    Parameter older; older.code = QStringLiteral("TEMP_CH1"); older.value = 1.0;
    older.timestamp = base;
    Parameter newer; newer.code = QStringLiteral("TEMP_CH1"); newer.value = 3.0;
    newer.timestamp = base.addSecs(120);
    Parameter middle; middle.code = QStringLiteral("TEMP_CH1"); middle.value = 2.0;
    middle.timestamp = base.addSecs(60);

    QVERIFY(m_db->insertParameter(middle));
    QVERIFY(m_db->insertParameter(older));
    QVERIFY(m_db->insertParameter(newer));

    const QVector<Parameter> rows = m_db->recentParameters(QStringLiteral("TEMP_CH1"));
    QCOMPARE(rows.size(), 3);
    QCOMPARE(rows.at(0).value, 3.0);     // 最新
    QCOMPARE(rows.at(1).value, 2.0);
    QCOMPARE(rows.at(2).value, 1.0);     // 最旧
}

void TestDatabase::recentParametersRespectsLimit()
{
    const QDateTime base(QDate(2026, 10, 3), QTime(15, 0, 0));
    for (int i = 0; i < 5; ++i) {
        Parameter p;
        p.code      = QStringLiteral("TEMP_CH1");
        p.value     = i;
        p.timestamp = base.addSecs(i);
        QVERIFY(m_db->insertParameter(p));
    }

    QCOMPARE(m_db->recentParameters(QStringLiteral("TEMP_CH1"), 2).size(), 2);
    QCOMPARE(m_db->recentParameters(QStringLiteral("TEMP_CH1"), 100).size(), 5);
    // 最近 2 条应该是 value = 4, 3
    const QVector<Parameter> top2 = m_db->recentParameters(QStringLiteral("TEMP_CH1"), 2);
    QCOMPARE(top2.at(0).value, 4.0);
    QCOMPARE(top2.at(1).value, 3.0);
}

void TestDatabase::recentParametersFiltersByParamCode()
{
    const QDateTime base(QDate(2026, 10, 3), QTime(16, 0, 0));

    Parameter a; a.code = QStringLiteral("TEMP_CH1"); a.value = 1.0; a.timestamp = base;
    Parameter b; b.code = QStringLiteral("PRESS_CH2"); b.value = 2.0; b.timestamp = base;
    QVERIFY(m_db->insertParameter(a));
    QVERIFY(m_db->insertParameter(b));

    const QVector<Parameter> temp = m_db->recentParameters(QStringLiteral("TEMP_CH1"));
    QCOMPARE(temp.size(), 1);
    QCOMPARE(temp.at(0).code, QStringLiteral("TEMP_CH1"));
}

void TestDatabase::recentParametersOnEmptyTableReturnsEmpty()
{
    const QVector<Parameter> rows = m_db->recentParameters(QStringLiteral("NOT_EXIST"));
    QVERIFY2(rows.isEmpty(),
             "空结果集必须返回空 QVector（不是 1 个空元素），而且不能崩");
}

// ---------------------------------------------------------------------------
//  D4-9 activeAlarms
// ---------------------------------------------------------------------------
void TestDatabase::activeAlarmsReturnsOnlyUnclearedOnes()
{
    const QDateTime base(QDate(2026, 10, 3), QTime(17, 0, 0));

    // ① 未恢复（clearedAt 无效）→ 应该查到
    QVERIFY(m_db->insertAlarm(makeAlarm(QStringLiteral("TEMP_CH1"),
                                        AlarmLevel::Warning, base)));
    // ② 已恢复（clearedAt 有值）→ 不该查到
    QVERIFY(m_db->insertAlarm(makeAlarm(QStringLiteral("PRESS_CH2"),
                                        AlarmLevel::Warning, base,
                                        base.addSecs(30))));

    const QVector<Alarm> active = m_db->activeAlarms();
    QCOMPARE(active.size(), 1);
    QCOMPARE(active.at(0).paramCode, QStringLiteral("TEMP_CH1"));
}

void TestDatabase::activeAlarmsReturnsEmptyWhenAllCleared()
{
    const QDateTime base(QDate(2026, 10, 3), QTime(18, 0, 0));
    QVERIFY(m_db->insertAlarm(makeAlarm(QStringLiteral("TEMP_CH1"),
                                        AlarmLevel::Warning, base, base.addSecs(10))));
    QVERIFY(m_db->insertAlarm(makeAlarm(QStringLiteral("PRESS_CH2"),
                                        AlarmLevel::Warning, base, base.addSecs(20))));

    QVERIFY2(m_db->activeAlarms().isEmpty(),
             "全恢复时 activeAlarms() 必须为空 —— 检查 WHERE cleared_at IS NULL；"
             "写成 = NULL 会导致【全部查不到】或【全都查出来】");
}

// ---------------------------------------------------------------------------
//  健壮性
// ---------------------------------------------------------------------------
void TestDatabase::insertParameterFailsWhenDatabaseClosed()
{
    m_db->close();

    Parameter p;
    p.code      = QStringLiteral("TEMP_CH1");
    p.value     = 1.0;
    p.timestamp = QDateTime::currentDateTime();

    const bool ok = m_db->insertParameter(p);
    QVERIFY2(!ok, "库已关闭时 insertParameter() 必须返回 false，而不是静默成功/崩溃");
    QVERIFY2(!m_db->lastError().isEmpty(),
             "失败时必须把原因写进 m_lastError（用 query.lastError().text()）");
}

QTEST_GUILESS_MAIN(TestDatabase)

#include "test_database.moc"
