#include <QtTest>

#include "stats.h"

class TestStats : public QObject
{
    Q_OBJECT

private slots:
    void parsesVmRss();
    void missingVmRssIsZero();
    void formatsStats();
    void selfTreeHasMemory();
};

void TestStats::parsesVmRss()
{
    const QString status = QStringLiteral("Name:\tzapshell\nVmRSS:\t  204800 kB\nThreads:\t12\n");
    QCOMPARE(zs::parseVmRssKb(status), quint64(204800));
}

void TestStats::missingVmRssIsZero()
{
    QCOMPARE(zs::parseVmRssKb(QStringLiteral("Name:\tx\n")), quint64(0));
}

void TestStats::formatsStats()
{
    QCOMPARE(zs::formatStats(204800, 1.25), QStringLiteral("zapshell: 200 MB RSS, 1.2% CPU"));
}

void TestStats::selfTreeHasMemory()
{
    QVERIFY(zs::processTreeRssKb(QCoreApplication::applicationPid()) > 0);
}

QTEST_GUILESS_MAIN(TestStats)
#include "tst_stats.moc"
