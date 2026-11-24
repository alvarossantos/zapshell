#include <QtTest>

#include <QFile>
#include <QElapsedTimer>
#include <QLocalServer>
#include <QLocalSocket>
#include <QTemporaryDir>
#include <QThread>

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

QTEST_GUILESS_MAIN(TestInstance)
#include "tst_instance.moc"
