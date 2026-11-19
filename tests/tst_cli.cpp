#include <QtTest>

#include "cli.h"

class TestCli : public QObject
{
    Q_OBJECT

private slots:
    void parsesFlags();
    void rejectsUnknownFlag();
    void restoreTakesPath();
    void unknownFlagWritesStderr();
    void versionGoesToStdout();
};

void TestCli::unknownFlagWritesStderr()
{
    QProcess process;
    process.start(QStringLiteral(ZAPSHELL_BIN), {QStringLiteral("--nope")});
    QVERIFY(process.waitForFinished(10000));
    QCOMPARE(process.exitCode(), 1);
    QVERIFY2(process.readAllStandardError().contains("flag desconhecida"),
             qPrintable(QString::fromUtf8(process.readAllStandardError())));
}

void TestCli::versionGoesToStdout()
{
    QProcess process;
    process.start(QStringLiteral(ZAPSHELL_BIN), {QStringLiteral("--version")});
    QVERIFY(process.waitForFinished(10000));
    QCOMPARE(process.exitCode(), 0);
    QVERIFY(process.readAllStandardOutput().contains("zapshell"));
}

void TestCli::parsesFlags()
{
    char a0[] = "zapshell";
    char a1[] = "--hidden";
    char a2[] = "--toggle";
    char *argv[] = {a0, a1, a2};
    zs::cli::Options options;
    QString error;

    QVERIFY(zs::cli::parseArgs(3, argv, options, error));
    QVERIFY2(error.isEmpty(), qPrintable(error));
    QVERIFY(options.hidden);
    QVERIFY(options.toggle);
    QVERIFY(!options.quit);
}

void TestCli::rejectsUnknownFlag()
{
    char a0[] = "zapshell";
    char a1[] = "--nope";
    char *argv[] = {a0, a1};
    zs::cli::Options options;
    QString error;

    QVERIFY(!zs::cli::parseArgs(2, argv, options, error));
    QVERIFY(!error.isEmpty());
}

void TestCli::restoreTakesPath()
{
    char a0[] = "zapshell";
    char a1[] = "--restore";
    char a2[] = "/tmp/a.tar.gz";
    char *argv[] = {a0, a1, a2};
    zs::cli::Options options;
    QString error;

    QVERIFY(zs::cli::parseArgs(3, argv, options, error));
    QVERIFY(options.restore);
    QCOMPARE(options.restorePath, QString("/tmp/a.tar.gz"));
}

QTEST_GUILESS_MAIN(TestCli)
#include "tst_cli.moc"
