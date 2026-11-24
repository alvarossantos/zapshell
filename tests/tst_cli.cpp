#include <QtTest>
#include <QTemporaryDir>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QProcess>
#include <QProcessEnvironment>

#include "cli.h"

class TestCli : public QObject
{
    Q_OBJECT

private slots:
    void parsesFlags();
    void rejectsUnknownFlag();
    void restoreTakesPath();
    void rejectsConflictingActions();
    void unknownFlagWritesStderr();
    void versionGoesToStdout();
    void relanceAfterMaintenance();
};

static QProcessEnvironment environmentFor(const QString &base)
{
    auto env=QProcessEnvironment::systemEnvironment();
    env.insert("XDG_DATA_HOME",base+"/data"); env.insert("XDG_CACHE_HOME",base+"/cache");
    env.insert("XDG_RUNTIME_DIR",base+"/runtime"); env.insert("QT_QPA_PLATFORM","offscreen");
    return env;
}

static bool awaitSocket(const QString &base,int ms=10000)
{
    QElapsedTimer t; t.start(); while(t.elapsed()<ms) { if(QFile::exists(base+"/runtime/zapshell.sock")) return true; QTest::qWait(50); }
    return QFile::exists(base+"/runtime/zapshell.sock");
}

static bool invoke(const QString &base,const QStringList &args,QByteArray *out=nullptr,int ms=20000)
{
    QProcess p; p.setProcessEnvironment(environmentFor(base)); p.start(QStringLiteral(ZAPSHELL_BIN),args);
    if(!p.waitForStarted(3000)||!p.waitForFinished(ms)) { p.kill(); p.waitForFinished(); return false; }
    if(out) *out=p.readAllStandardOutput(); return p.exitStatus()==QProcess::NormalExit && p.exitCode()==0;
}

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

void TestCli::relanceAfterMaintenance()
{
    QTemporaryDir dir; QVERIFY(dir.isValid()); QVERIFY(QDir().mkpath(dir.path()+"/runtime"));
    QVERIFY(QDir().mkpath(dir.path()+"/data/zapshell/profile"));
    QFile seed(dir.path()+"/data/zapshell/profile/seed"); QVERIFY(seed.open(QIODevice::WriteOnly)); seed.write("seed"); seed.close();
    QProcess owner; owner.setProcessEnvironment(environmentFor(dir.path())); owner.start(QStringLiteral(ZAPSHELL_BIN),{"--hidden"}); QVERIFY(owner.waitForStarted(3000));
    if(!awaitSocket(dir.path())) { owner.kill(); owner.waitForFinished(); QFAIL("hidden owner did not listen"); }
    QByteArray stats; QVERIFY(invoke(dir.path(),{"--stats"},&stats,3000)); QVERIFY(!stats.isEmpty());
    QByteArray backupOut; QVERIFY(invoke(dir.path(),{"--backup"},&backupOut,25000));
    QString archive;
    for (const QString &line : QString::fromLocal8Bit(backupOut).split('\n'))
        if (line.trimmed().endsWith(".tar.gz")) archive=line.trimmed();
    QVERIFY2(!archive.isEmpty(),qPrintable(QString::fromLocal8Bit(backupOut)));
    QVERIFY(awaitSocket(dir.path(),10000)); QVERIFY(invoke(dir.path(),{"--stats"},&stats,3000));
    QProcess quit; quit.setProcessEnvironment(environmentFor(dir.path())); quit.start(QStringLiteral(ZAPSHELL_BIN),{"--quit"}); QVERIFY(quit.waitForFinished(3000));
    QElapsedTimer gone; gone.start(); while(QFile::exists(dir.path()+"/runtime/zapshell.sock")&&gone.elapsed()<5000) QTest::qWait(50);
    QVERIFY(!QFile::exists(dir.path()+"/runtime/zapshell.sock"));
    QVERIFY(invoke(dir.path(),{"--restore",archive},nullptr,20000));
    QVERIFY(awaitSocket(dir.path(),10000)); QVERIFY(invoke(dir.path(),{"--stats"},&stats,3000));
    QProcess stop; stop.setProcessEnvironment(environmentFor(dir.path())); stop.start(QStringLiteral(ZAPSHELL_BIN),{"--quit"}); QVERIFY(stop.waitForFinished(3000));
    owner.kill(); owner.waitForFinished(3000);
}

void TestCli::rejectsConflictingActions()
{
    char a0[] = "zapshell";
    char a1[] = "--backup";
    char a2[] = "--restore";
    char a3[] = "/tmp/arquivo.tar.gz";
    char *argv[] = {a0, a1, a2, a3};
    zs::cli::Options options;
    QString error;

    QVERIFY(!zs::cli::parseArgs(4, argv, options, error));
    QVERIFY2(error.contains("backup"), qPrintable(error));
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
