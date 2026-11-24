#include <QtTest>

#include <QFile>
#include <QElapsedTimer>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTemporaryDir>
#include <QThread>
#include <QDir>
#include <QProcess>
#include <QProcessEnvironment>
#include <QFileInfo>
#include <QElapsedTimer>
#include <QTest>

#include <sys/socket.h>
#include <sys/un.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstring>
#include <algorithm>
#include <array>
#include <atomic>
#include <thread>
#include <memory>

#include "instance.h"

class TestInstance : public QObject
{
    Q_OBJECT

private slots:
    void secondInstanceSendsVerbToFirst();
    void unknownVerbIsRejected();
    void staleSocketIsReclaimed();
    void waitReportsWhetherSocketWasReleased();
    void oversizedLineIsDropped();
    void doesNotDeleteRegularFileAtSocketPath();
    void idleConnectionIsClosed();
    void concurrentAcquireHasSingleOwner();
    void failedAcquireReleasesLock();
    void waitBudgetIsNotReset();
    void maintenanceAcquireIsExclusive();
    void maintenanceLockBlocksProfileStartup();
    void restoreBlocksProfileStartup();
    void startupWaitReleasesAfterMaintenance();
    void lockLostBeforeMaintenanceDoesNotRelaunch();
    void backupMaintenanceFailureWithoutResponderDoesNotRelaunch();
    void backupWasRunningLockContentionDoesNotRelaunch();
    void ipcCommandsUnchanged();
    void shutdownToMaintenanceContention();
    void restoreFailureDoesNotRelaunch();
    void backupFlowSharedBudgetCapsWaitAndLockContention();
    void acquireIsIdempotentForAppLock();
    void acquireMaintenanceIsIdempotentForMaintenanceLock();
    void invalidTransitionMaintenanceToAcquirePreservesExclusivity();
    void invalidTransitionAcquireToMaintenancePreservesExclusivity();
};

static void configureChild(QProcess &process, const QString &base)
{
    QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
    env.insert("XDG_DATA_HOME", base + "/data");
    env.insert("XDG_CACHE_HOME", base + "/cache");
    env.insert("XDG_RUNTIME_DIR", base + "/runtime");
    env.insert("QT_QPA_PLATFORM", "offscreen");
    process.setProcessEnvironment(env);
}

static bool hasProfileFiles(const QString &base)
{
    return QDir(base + "/data/zapshell/profile").exists();
}

static bool waitForSocket(const QString &base, int timeout = 5000)
{
    QElapsedTimer timer; timer.start();
    while (timer.elapsed() < timeout) {
        if (QFile::exists(base + "/runtime/zapshell.sock")) return true;
        QTest::qWait(50);
    }
    return QFile::exists(base + "/runtime/zapshell.sock");
}

static bool runCli(const QString &base, const QStringList &args, QByteArray *out = nullptr,
                   QByteArray *err = nullptr, int timeout = 5000)
{
    QProcess p; configureChild(p, base); p.start(QStringLiteral(ZAPSHELL_BIN), args);
    if (!p.waitForStarted(3000) || !p.waitForFinished(timeout)) { p.kill(); p.waitForFinished(); return false; }
    if (out) *out = p.readAllStandardOutput();
    if (err) *err = p.readAllStandardError();
    return p.exitStatus() == QProcess::NormalExit && p.exitCode() == 0;
}

struct ChildProcess : QProcess
{
    ~ChildProcess() override { if (state()!=NotRunning) { kill(); waitForFinished(3000); } }
};

void TestInstance::secondInstanceSendsVerbToFirst()
{
    QTemporaryDir dir;
    const QString socketPath = dir.path() + "/t.sock";

    zs::InstanceLock first(socketPath);
    QStringList received;
    first.setHandler([&received](const QString &verb) {
        received << verb;
        return QStringLiteral("ok");
    });
    QString error;
    QVERIFY2(first.acquire(&error), qPrintable(error));

    zs::InstanceLock second(socketPath);
    QVERIFY(!second.acquire()); // já existe dono

    QString reply;
    QVERIFY(second.send(QStringLiteral("toggle"), &reply));
    QCOMPARE(reply, QStringLiteral("ok"));
    QCOMPARE(received, QStringList{QStringLiteral("toggle")});
}

void TestInstance::unknownVerbIsRejected()
{
    QTemporaryDir dir;
    const QString socketPath = dir.path() + "/t.sock";

    zs::InstanceLock first(socketPath);
    first.setHandler([](const QString &verb) {
        if (verb == QLatin1String("toggle"))
            return QStringLiteral("ok");
        return QStringLiteral("err unknown verb");
    });
    QVERIFY(first.acquire());

    zs::InstanceLock client(socketPath);
    QString reply;
    QVERIFY(client.send(QStringLiteral("nuke"), &reply));
    QCOMPARE(reply, QStringLiteral("err unknown verb"));
}

void TestInstance::staleSocketIsReclaimed()
{
    QTemporaryDir dir;
    const QString socketPath = dir.path() + "/t.sock";

    // Socket órfão de verdade: processo filho escuta e morre com _exit, sem
    // rodar destrutor nem desvincular o arquivo.
    const pid_t pid = ::fork();
    if (pid == 0) {
        const int fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        sockaddr_un addr{};
        addr.sun_family = AF_UNIX;
        const QByteArray path = socketPath.toLocal8Bit();
        std::memcpy(addr.sun_path, path.constData(), size_t(path.size()));
        ::bind(fd, reinterpret_cast<sockaddr *>(&addr), sizeof(addr));
        ::listen(fd, 1);
        ::_exit(0);
    }
    QCOMPARE(::waitpid(pid, nullptr, 0), pid);
    QVERIFY(QFile::exists(socketPath));

    zs::InstanceLock next(socketPath);
    QString error;
    QVERIFY2(next.acquire(&error), qPrintable(error));
}

void TestInstance::waitReportsWhetherSocketWasReleased()
{
    QTemporaryDir dir;
    const QString socketPath = dir.path() + "/t.sock";

    QLocalServer server;
    QVERIFY(server.listen(socketPath));
    QVERIFY(!zs::InstanceLock::waitUntilReleased(socketPath, 150));

    server.close();
    QLocalServer::removeServer(socketPath);
    QVERIFY(zs::InstanceLock::waitUntilReleased(socketPath, 150));
}

void TestInstance::oversizedLineIsDropped()
{
    QTemporaryDir dir;
    const QString socketPath = dir.path() + "/t.sock";

    zs::InstanceLock first(socketPath);
    int calls = 0;
    first.setHandler([&calls](const QString &) {
        ++calls;
        return QStringLiteral("ok");
    });
    QVERIFY(first.acquire());

    QLocalSocket client;
    client.connectToServer(socketPath);
    QVERIFY(client.waitForConnected(1000));
    client.write(QByteArray(8192, 'x'));
    client.flush();

    QTRY_COMPARE_WITH_TIMEOUT(client.state(), QLocalSocket::UnconnectedState, 2000);
    QCOMPARE(calls, 0);
}

void TestInstance::doesNotDeleteRegularFileAtSocketPath()
{
    QTemporaryDir dir;
    const QString socketPath = dir.path() + "/t.sock";

    QFile victim(socketPath);
    QVERIFY(victim.open(QIODevice::WriteOnly));
    victim.write("importante");
    victim.close();

    zs::InstanceLock lock(socketPath);
    QString error;
    QVERIFY2(!lock.acquire(&error),
             "não deve assumir a posse apagando arquivo regular no caminho do socket");
    QVERIFY2(QFile::exists(socketPath), "arquivo regular no caminho do socket foi removido");

    QFile check(socketPath);
    QVERIFY(check.open(QIODevice::ReadOnly));
    QCOMPARE(check.readAll(), QByteArray("importante"));
}

void TestInstance::idleConnectionIsClosed()
{
    QTemporaryDir dir;
    const QString socketPath = dir.path() + "/t.sock";

    zs::InstanceLock first(socketPath);
    int calls = 0;
    first.setHandler([&calls](const QString &) {
        ++calls;
        return QStringLiteral("ok");
    });
    QVERIFY(first.acquire());

    QLocalSocket client;
    client.connectToServer(socketPath);
    QVERIFY(client.waitForConnected(1000));
    QTRY_COMPARE_WITH_TIMEOUT(client.state(), QLocalSocket::UnconnectedState, 3500);
    QCOMPARE(calls, 0);
}

void TestInstance::concurrentAcquireHasSingleOwner()
{
    QTemporaryDir dir;
    const QString socketPath = dir.path() + "/t.sock";
    constexpr int kRacers = 4;

    std::atomic<int> atGate{0};
    std::atomic<bool> go{false};
    std::array<bool, kRacers> won{};
    QList<QThread *> threads;

    for (int i = 0; i < kRacers; ++i) {
        auto *t = QThread::create([&, i] {
            zs::InstanceLock lock(socketPath);
            atGate.fetch_add(1);
            while (!go.load())
                QThread::yieldCurrentThread();
            QString error;
            won[i] = lock.acquire(&error);
            if (won[i])
                QThread::msleep(60); // mantém a posse enquanto os outros tentam
        });
        threads << t;
        t->start();
    }

    while (atGate.load() < kRacers)
        QThread::yieldCurrentThread();
    go.store(true);
    for (QThread *t : threads) {
        QVERIFY(t->wait(5000));
        delete t;
    }

    const int owners = std::count(won.begin(), won.end(), true);
    QCOMPARE(owners, 1);
}

void TestInstance::failedAcquireReleasesLock()
{
    {
        QTemporaryDir dir;
        const QString socketPath = dir.path() + "/t.sock";
        QFile victim(socketPath);
        QVERIFY(victim.open(QIODevice::WriteOnly));
        victim.close();

        zs::InstanceLock a(socketPath);
        QString error;
        QVERIFY(!a.acquire(&error));
        QVERIFY(QFile::remove(socketPath));
        zs::InstanceLock b(socketPath);
        QString bErr;
        QVERIFY2(b.acquireMaintenance(&bErr, 0), qPrintable(bErr));
    }
    {
        QTemporaryDir dir;
        const QString socketPath = dir.path() + "/missing/t.sock";
        zs::InstanceLock a(socketPath);
        QString error;
        QVERIFY(!a.acquire(&error));
    }
}

void TestInstance::waitBudgetIsNotReset()
{
    QTemporaryDir dir;
    const QString socketPath = dir.path() + "/t.sock";
    auto holder = std::make_unique<zs::InstanceLock>(socketPath);
    QVERIFY(holder->acquireMaintenance());
    std::atomic<bool> finished{false};
    bool acquired = true;
    qint64 elapsed = 0;
    std::thread attempt([&] {
        zs::InstanceLock other(socketPath);
        QString error;
        QElapsedTimer timer;
        timer.start();
        acquired = other.acquire(&error, 300);
        elapsed = timer.elapsed();
        finished.store(true);
    });
    QTest::qWait(2000);
    const bool completedWithinGuard = finished.load();
    holder.reset();
    attempt.join();
    QVERIFY(completedWithinGuard);
    QVERIFY(!acquired);
    QVERIFY2(elapsed >= 250 && elapsed <= 900, qPrintable(QString::number(elapsed)));
}

void TestInstance::maintenanceAcquireIsExclusive()
{
    QTemporaryDir dir;
    const QString socketPath = dir.path() + "/t.sock";
    {
        zs::InstanceLock holder(socketPath);
        QVERIFY(holder.acquireMaintenance());
        zs::InstanceLock second(socketPath);
        QString error;
        QVERIFY(!second.acquireMaintenance(&error, 0));
        QVERIFY(error.contains(QStringLiteral("lock")));
    }
    zs::InstanceLock third(socketPath);
    QVERIFY(third.acquireMaintenance());
}

void TestInstance::maintenanceLockBlocksProfileStartup()
{
    QTemporaryDir dir; QVERIFY(dir.isValid());
    QDir().mkpath(dir.path() + "/runtime");
    auto holder = std::make_unique<zs::InstanceLock>(dir.path() + "/runtime/zapshell.sock");
    QVERIFY(holder->acquireMaintenance());
    ChildProcess child; configureChild(child, dir.path());
    child.start(QStringLiteral(ZAPSHELL_BIN), {"--hidden"});
    QVERIFY(child.waitForStarted(3000));
    QTest::qWait(2000);
    QVERIFY(!QFile::exists(dir.path() + "/runtime/zapshell.sock"));
    QVERIFY(!hasProfileFiles(dir.path()));
    holder.reset();
    QElapsedTimer wait; wait.start();
    while (!QFile::exists(dir.path() + "/runtime/zapshell.sock") && wait.elapsed() < 18000) {
        QTest::qWait(100);
        if (child.state() == QProcess::NotRunning)
            break;
    }
    const QByteArray out = child.readAllStandardOutput();
    const QByteArray err = child.readAllStandardError();
    qInfo() << "startup diagnostic socket=" << QFile::exists(dir.path() + "/runtime/zapshell.sock")
            << "elapsed=" << wait.elapsed() << "state=" << child.state()
            << "exitCode=" << child.exitCode() << "exitStatus=" << child.exitStatus()
            << "stdout=" << out << "stderr=" << err;
    QVERIFY2(QFile::exists(dir.path() + "/runtime/zapshell.sock"),
             "startup diagnostic above records the blocked child state");
    child.kill(); child.waitForFinished(3000);
}

void TestInstance::restoreBlocksProfileStartup()
{
    QTemporaryDir dir; QVERIFY(dir.isValid()); QDir().mkpath(dir.path()+"/runtime");
    QDir().mkpath(dir.path()+"/data/zapshell/profile"); QFile marker(dir.path()+"/data/zapshell/profile/keep");
    QVERIFY(marker.open(QIODevice::WriteOnly)); marker.write("untouched"); marker.close();
    const QString archive=dir.path()+"/valid.tar.gz";
    QProcess tar; tar.start("tar", {"-czf",archive,"-C",dir.path()+"/data/zapshell","profile"});
    QVERIFY(tar.waitForFinished(5000)); QCOMPARE(tar.exitCode(),0);
    auto holder=std::make_unique<zs::InstanceLock>(dir.path()+"/runtime/zapshell.sock"); QVERIFY(holder->acquireMaintenance());
    ChildProcess restore; configureChild(restore,dir.path()); restore.start(QStringLiteral(ZAPSHELL_BIN),{"--restore",archive}); QVERIFY(restore.waitForStarted(3000));
    QTest::qWait(200); QVERIFY(QFile::exists(marker.fileName())); QCOMPARE(QFileInfo(marker.fileName()).size(),qint64(9));
    holder.reset(); QVERIFY(restore.waitForFinished(16000));
    QVERIFY(QFile::exists(marker.fileName()));
    if (waitForSocket(dir.path(), 3000)) {
        QProcess quit; configureChild(quit, dir.path());
        quit.start(QStringLiteral(ZAPSHELL_BIN), {"--quit"});
        quit.waitForFinished(3000);
    }
}

void TestInstance::startupWaitReleasesAfterMaintenance()
{
    QTemporaryDir dir; QVERIFY(dir.isValid()); QDir().mkpath(dir.path()+"/runtime");
    auto holder=std::make_unique<zs::InstanceLock>(dir.path()+"/runtime/zapshell.sock"); QVERIFY(holder->acquireMaintenance());
    ChildProcess child; configureChild(child,dir.path()); child.start(QStringLiteral(ZAPSHELL_BIN),{"--hidden"}); QVERIFY(child.waitForStarted(3000));
    std::thread release([&holder]{ QThread::msleep(1000); holder.reset(); });
    const bool socket=waitForSocket(dir.path(),3000); release.join();
    QVERIFY2(socket,qPrintable(QString::fromUtf8(child.readAllStandardError()))); child.kill(); child.waitForFinished(3000);

    QTemporaryDir blocked; QVERIFY(blocked.isValid()); QDir().mkpath(blocked.path()+"/runtime");
    auto held=std::make_unique<zs::InstanceLock>(blocked.path()+"/runtime/zapshell.sock"); QVERIFY(held->acquireMaintenance());
    ChildProcess timeout; configureChild(timeout,blocked.path()); QElapsedTimer timer; timer.start();
    timeout.start(QStringLiteral(ZAPSHELL_BIN),{"--hidden"}); QVERIFY(timeout.waitForStarted(3000)); QVERIFY(timeout.waitForFinished(18000));
    const qint64 elapsed=timer.elapsed(); const QByteArray err=timeout.readAllStandardError();
    QVERIFY2(elapsed>=14000 && elapsed<=17500,qPrintable(QString::number(elapsed)));
    QVERIFY2(err.contains("15000 ms"),qPrintable(QString::fromUtf8(err)));
}

void TestInstance::lockLostBeforeMaintenanceDoesNotRelaunch()
{
    QTemporaryDir dir; QVERIFY(dir.isValid()); QDir().mkpath(dir.path()+"/runtime");
    ChildProcess app; configureChild(app,dir.path()); app.start(QStringLiteral(ZAPSHELL_BIN),{"--hidden"}); QVERIFY(app.waitForStarted(3000));
    if (!waitForSocket(dir.path(),3000)) { app.kill(); app.waitForFinished(); QFAIL("app did not acquire its socket"); }

    // Outra instância viva assume o lock após a dona encerrar; a manutenção não
    // deve relançar uma terceira instância desnecessária.
    zs::InstanceLock lock(dir.path()+"/runtime/zapshell.sock");
    QProcess quit; configureChild(quit,dir.path()); quit.start(QStringLiteral(ZAPSHELL_BIN),{"--quit"});
    QVERIFY(quit.waitForFinished(5000));
    QVERIFY(zs::InstanceLock::waitUntilReleased(dir.path()+"/runtime/zapshell.sock", 5000));

    std::atomic<bool> acquired{false}, release{false};
    std::thread contender([&] {
        zs::InstanceLock other(dir.path()+"/runtime/zapshell.sock"); QString error;
        acquired.store(other.acquireMaintenance(&error,17000));
        while(acquired.load() && !release.load()) QThread::msleep(10);
    });
    QElapsedTimer probe; probe.start();
    while (!acquired.load() && probe.elapsed() < 2000) QThread::msleep(10);
    QVERIFY(acquired.load());

    QProcess backup; configureChild(backup,dir.path()); backup.start(QStringLiteral(ZAPSHELL_BIN),{"--backup"});
    const bool finished = backup.waitForFinished(20000);
    release.store(true); contender.join();
    QVERIFY(finished);
    QCOMPARE(backup.exitCode(), 1);
    QTest::qWait(300);
    QVERIFY2(!QFile::exists(dir.path()+"/runtime/zapshell.sock"),
             "manutenção não deve relançar instância quando outra está ativa");
    if (app.state()!=QProcess::NotRunning) { app.kill(); app.waitForFinished(3000); }
}

void TestInstance::backupMaintenanceFailureWithoutResponderDoesNotRelaunch()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QDir().mkpath(dir.path() + "/runtime");
    ChildProcess app;
    configureChild(app, dir.path());
    app.start(QStringLiteral(ZAPSHELL_BIN), {"--hidden"});
    QVERIFY(app.waitForStarted(3000));
    if (!waitForSocket(dir.path(), 3000)) {
        app.kill();
        app.waitForFinished();
        QFAIL("app did not start");
    }

    // ZS-11: app estava ativa; após sair, um maintainer sem servidor IPC segura o lock.
    // Qualquer falha de acquireMaintenance => sem relaunch, mesmo sem responder IPC.
    std::atomic<bool> acquired{false}, release{false}, stop{false};
    std::thread contender([&] {
        const QString socket = dir.path() + "/runtime/zapshell.sock";
        while (!stop.load()) {
            if (!QFile::exists(socket)) {
                zs::InstanceLock other(socket);
                QString err;
                if (other.acquireMaintenance(&err, 0)) {
                    acquired.store(true);
                    while (!release.load())
                        QThread::msleep(10);
                    return;
                }
            }
            QThread::msleep(5);
        }
    });

    QProcess backup;
    configureChild(backup, dir.path());
    backup.start(QStringLiteral(ZAPSHELL_BIN), {"--backup"});
    const bool finished = backup.waitForFinished(35000);
    stop.store(true);
    release.store(true);
    contender.join();
    QVERIFY(finished);
    QCOMPARE(backup.exitCode(), 1);
    QVERIFY2(acquired.load(), "contender devia ter adquirido o lock de manutenção");

    const QByteArray err = backup.readAllStandardError();
    QVERIFY2(err.contains("manutenção abortada") || err.contains("manutencao abortada"),
             qPrintable(QStringLiteral("stderr deve orientar relaunch manual: %1")
                            .arg(QString::fromLocal8Bit(err))));

    QTest::qWait(400);
    QVERIFY2(!QFile::exists(dir.path() + "/runtime/zapshell.sock"),
             "ZS-11: falha de acquireMaintenance não deve relançar automaticamente");
}

void TestInstance::backupWasRunningLockContentionDoesNotRelaunch()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QDir().mkpath(dir.path() + "/runtime");
    ChildProcess app;
    configureChild(app, dir.path());
    app.start(QStringLiteral(ZAPSHELL_BIN), {"--hidden"});
    QVERIFY(app.waitForStarted(3000));
    if (!waitForSocket(dir.path(), 3000)) {
        app.kill();
        app.waitForFinished();
        QFAIL("app did not start");
    }

    // ZS-11: backupFlow observa app viva (wasRunning=true) ao receber resposta
    // "ok" para "backup". O app então encerra e libera o socket; antes que o
    // backupFlow readquira manutenção, outro maintainer vence a disputa.
    const QString socket = dir.path() + "/runtime/zapshell.sock";
    std::atomic<bool> acquired{false};
    std::atomic<bool> release{false};
    std::atomic<bool> stop{false};
    std::thread contender([&] {
        while (!stop.load()) {
            if (!QFile::exists(socket)) {
                zs::InstanceLock other(socket);
                QString err;
                if (other.acquireMaintenance(&err, 0)) {
                    acquired.store(true);
                    while (!release.load() && !stop.load())
                        QThread::msleep(5);
                    return;
                }
            }
            QThread::msleep(1);
        }
    });

    QProcess backup;
    configureChild(backup, dir.path());
    backup.start(QStringLiteral(ZAPSHELL_BIN), {"--backup"});
    const bool finished = backup.waitForFinished(20000);
    stop.store(true);
    release.store(true);
    contender.join();

    QVERIFY2(finished, "backup deve concluir mesmo com contenção de lock");
    QCOMPARE(backup.exitCode(), 1);
    QVERIFY2(acquired.load(),
             "contender devia adquirir manutenção após app sair e antes do backupFlow");

    const QByteArray err = backup.readAllStandardError();
    QVERIFY2(err.contains("manutenção abortada") || err.contains("manutencao abortada"),
             qPrintable(QStringLiteral("stderr deve orientar relaunch manual: %1")
                            .arg(QString::fromLocal8Bit(err))));

    QTest::qWait(400);
    QVERIFY2(!QFile::exists(socket),
             "ZS-11: wasRunning+lock contention não deve relançar automaticamente");
    if (app.state() != QProcess::NotRunning) {
        app.kill();
        app.waitForFinished(3000);
    }
}

void TestInstance::ipcCommandsUnchanged()
{
    QTemporaryDir dir; QVERIFY(dir.isValid()); QDir().mkpath(dir.path()+"/runtime");
    ChildProcess app; configureChild(app,dir.path()); app.start(QStringLiteral(ZAPSHELL_BIN),{"--hidden"}); QVERIFY(app.waitForStarted(3000));
    if (!waitForSocket(dir.path(),3000)) { app.kill(); app.waitForFinished(); QFAIL("app did not start"); }
    QElapsedTimer timer; timer.start(); QByteArray out; QVERIFY(runCli(dir.path(),{"--stats"},&out)); QVERIFY(timer.elapsed()<1000); QVERIFY(!out.isEmpty());
    app.kill(); app.waitForFinished(3000); QLocalServer::removeServer(dir.path()+"/runtime/zapshell.sock");
    for (const QString &arg : {"--quit","--toggle","--stats"}) { timer.restart(); QVERIFY(runCli(dir.path(),{arg},&out)); QVERIFY(timer.elapsed()<1000); }
}

void TestInstance::shutdownToMaintenanceContention()
{
    QTemporaryDir dir; QVERIFY(dir.isValid()); QDir().mkpath(dir.path()+"/runtime"); QDir().mkpath(dir.path()+"/data/zapshell/profile");
    QFile marker(dir.path()+"/data/zapshell/profile/keep"); QVERIFY(marker.open(QIODevice::WriteOnly)); marker.write("x"); marker.close();
    auto holder=std::make_unique<zs::InstanceLock>(dir.path()+"/runtime/zapshell.sock"); QVERIFY(holder->acquireMaintenance());
    ChildProcess backup; configureChild(backup,dir.path()); backup.start(QStringLiteral(ZAPSHELL_BIN),{"--backup"}); QVERIFY(backup.waitForStarted(3000));
    QTest::qWait(200); QVERIFY(QDir(dir.path()+"/data/zapshell/backups").entryList({"*.tar.gz"},QDir::Files).isEmpty());
    holder.reset(); QVERIFY(backup.waitForFinished(16000)); QCOMPARE(backup.exitCode(),0);
    QVERIFY(!QDir(dir.path()+"/data/zapshell/backups").entryList({"*.tar.gz"},QDir::Files).isEmpty());
}

void TestInstance::restoreFailureDoesNotRelaunch()
{
    QTemporaryDir dir; QVERIFY(dir.isValid()); QDir().mkpath(dir.path()+"/runtime");
    QFile bad(dir.path()+"/bad.tar.gz"); QVERIFY(bad.open(QIODevice::WriteOnly)); bad.write("invalid"); bad.close();
    QByteArray err; ChildProcess restore; configureChild(restore,dir.path()); restore.start(QStringLiteral(ZAPSHELL_BIN),{"--restore",bad.fileName()});
    QVERIFY(restore.waitForFinished(8000)); QVERIFY(restore.exitCode()!=0); QVERIFY(!restore.readAllStandardError().isEmpty());
    QTest::qWait(300); QVERIFY(!QFile::exists(dir.path()+"/runtime/zapshell.sock"));
}

void TestInstance::backupFlowSharedBudgetCapsWaitAndLockContention()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString base = dir.path();
    QVERIFY(QDir().mkpath(base + "/runtime"));
    QVERIFY(QDir().mkpath(base + "/data/zapshell/profile"));
    QFile marker(base + "/data/zapshell/profile/keep.txt");
    QVERIFY(marker.open(QIODevice::WriteOnly));
    marker.write("original");
    marker.close();
    QFile readBefore(marker.fileName());
    QVERIFY(readBefore.open(QIODevice::ReadOnly));
    const QByteArray original = readBefore.readAll();
    readBefore.close();

    const QString socketPath = base + "/runtime/zapshell.sock";
    QLocalServer::removeServer(socketPath);
    QLocalServer server;
    QVERIFY2(server.listen(socketPath), qPrintable(server.errorString()));

    std::atomic<bool> backupRequested{false};
    connect(&server, &QLocalServer::newConnection, &server, [&server, &backupRequested]() {
        while (QLocalSocket *socket = server.nextPendingConnection()) {
            connect(socket, &QLocalSocket::readyRead, socket, [socket, &backupRequested]() {
                if (!socket->canReadLine())
                    return;
                const QString verb = QString::fromUtf8(socket->readLine()).trimmed();
                if (verb == QLatin1String("backup"))
                    backupRequested.store(true);
                socket->write(QByteArray("ok\n"));
                socket->flush();
                socket->disconnectFromServer();
                socket->deleteLater();
            });
            connect(socket, &QLocalSocket::disconnected, socket, &QLocalSocket::deleteLater);
        }
    });

    std::atomic<bool> contentionReady{false};
    std::atomic<bool> releaseContention{false};
    std::thread contender([&] {
        zs::InstanceLock holder(socketPath);
        QString lockError;
        if (holder.acquireMaintenance(&lockError, 20000)) {
            contentionReady.store(true);
            while (!releaseContention.load())
                QThread::msleep(10);
        }
    });

    QElapsedTimer waitReady;
    waitReady.start();
    while (!contentionReady.load() && waitReady.elapsed() < 3000)
        QTest::qWait(10);
    QVERIFY2(contentionReady.load(), "contendor de manutenção não adquiriu o lock");

    QProcess backup;
    configureChild(backup, base);
    QElapsedTimer timer;
    timer.start();
    backup.start(QStringLiteral(ZAPSHELL_BIN), {QStringLiteral("--backup")});
    QVERIFY(backup.waitForStarted(3000));

    QElapsedTimer requestWait;
    requestWait.start();
    while (!backupRequested.load() && requestWait.elapsed() < 5000)
        QTest::qWait(10);
    QVERIFY2(backupRequested.load(), "backupFlow não enviou pedido de backup ao app vivo");

    // Mantém o socket presente brevemente para consumir parte do orçamento.
    QTest::qWait(800);
    server.close();
    QLocalServer::removeServer(socketPath);

    const bool finished = backup.waitForFinished(17000);
    const qint64 elapsed = timer.elapsed();
    releaseContention.store(true);
    contender.join();

    QVERIFY2(finished, "processo --backup não terminou dentro do limite do teste");
    QVERIFY2(backup.exitCode() != 0, "backup deveria falhar quando orçamento total esgota");
    QVERIFY2(elapsed < 16500,
             qPrintable(QStringLiteral("orçamento total excedido: %1 ms").arg(elapsed)));

    const QByteArray err = backup.readAllStandardError();
    QVERIFY2(err.contains("15000 ms") || err.toLower().contains("orçamento")
                 || err.toLower().contains("orcamento") || err.contains("tempo"),
             qPrintable(QStringLiteral("erro pouco claro: %1").arg(QString::fromLocal8Bit(err))));

    const QStringList backups = QDir(base + "/data/zapshell/backups")
                                    .entryList({QStringLiteral("*.tar.gz")}, QDir::Files);
    QVERIFY2(backups.isEmpty(), "backup não deve ser criado sem acesso exclusivo seguro ao perfil");

    QFile readAfter(marker.fileName());
    QVERIFY(readAfter.open(QIODevice::ReadOnly));
    QCOMPARE(readAfter.readAll(), original);
}

void TestInstance::acquireIsIdempotentForAppLock()
{
    QTemporaryDir dir;
    const QString socketPath = dir.path() + "/t.sock";
    zs::InstanceLock lock(socketPath);
    QString error;
    QVERIFY2(lock.acquire(&error), qPrintable(error));
    QVERIFY2(lock.acquire(&error), qPrintable(error));

    zs::InstanceLock other(socketPath);
    QString otherError;
    QVERIFY(!other.acquire(&otherError, 0));
    QVERIFY(!otherError.isEmpty());
}

void TestInstance::acquireMaintenanceIsIdempotentForMaintenanceLock()
{
    QTemporaryDir dir;
    const QString socketPath = dir.path() + "/t.sock";
    zs::InstanceLock lock(socketPath);
    QString error;
    QVERIFY2(lock.acquireMaintenance(&error), qPrintable(error));
    QVERIFY2(lock.acquireMaintenance(&error), qPrintable(error));

    zs::InstanceLock other(socketPath);
    QString otherError;
    QVERIFY(!other.acquireMaintenance(&otherError, 0));
    QVERIFY(!otherError.isEmpty());
}

void TestInstance::invalidTransitionMaintenanceToAcquirePreservesExclusivity()
{
    QTemporaryDir dir;
    const QString socketPath = dir.path() + "/t.sock";
    zs::InstanceLock lock(socketPath);
    QString error;
    QVERIFY2(lock.acquireMaintenance(&error), qPrintable(error));

    QString transitionError;
    QVERIFY(!lock.acquire(&transitionError, 0));
    QVERIFY2(!transitionError.isEmpty(), "transição inválida deve reportar erro claro");

    QString recheckError;
    QVERIFY2(lock.acquireMaintenance(&recheckError, 0),
             qPrintable(QStringLiteral("estado original de manutenção não deve ser corrompido: %1")
                            .arg(recheckError)));

    zs::InstanceLock other(socketPath);
    QString otherError;
    QVERIFY2(!other.acquireMaintenance(&otherError, 0),
             "transição inválida não deve liberar exclusividade");
    QVERIFY(!otherError.isEmpty());
}

void TestInstance::invalidTransitionAcquireToMaintenancePreservesExclusivity()
{
    QTemporaryDir dir;
    const QString socketPath = dir.path() + "/t.sock";
    zs::InstanceLock lock(socketPath);
    QString error;
    QVERIFY2(lock.acquire(&error), qPrintable(error));

    QString transitionError;
    QVERIFY(!lock.acquireMaintenance(&transitionError, 0));
    QVERIFY2(!transitionError.isEmpty(), "transição inválida deve reportar erro claro");

    QString recheckError;
    QVERIFY2(lock.acquire(&recheckError, 0),
             qPrintable(QStringLiteral("estado original de aplicação não deve ser corrompido: %1")
                            .arg(recheckError)));

    zs::InstanceLock other(socketPath);
    QString otherError;
    QVERIFY2(!other.acquireMaintenance(&otherError, 0),
             "transição inválida não deve liberar exclusividade");
    QVERIFY(!otherError.isEmpty());
}

QTEST_GUILESS_MAIN(TestInstance)
#include "tst_instance.moc"
