#include <QtTest>

#include <QDateTime>
#include <QProcess>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "backup.h"

class ScopedEnvironmentVariable
{
public:
    explicit ScopedEnvironmentVariable(const QByteArray &name)
        : m_name(name), m_wasSet(qEnvironmentVariableIsSet(name.constData())),
          m_oldValue(qgetenv(name.constData()))
    {
    }

    ~ScopedEnvironmentVariable()
    {
        if (m_wasSet)
            qputenv(m_name.constData(), m_oldValue);
        else
            qunsetenv(m_name.constData());
    }

private:
    QByteArray m_name;
    bool m_wasSet;
    QByteArray m_oldValue;
};

class TestBackup : public QObject
{
    Q_OBJECT

private slots:
    void keepsNewestFour();
    void nameIsSortable();
    void createAndRestoreRoundTrip();
    void refusesPathTraversal();
    void restoresWhenProfileIsMissing();
    void snapshotKeepsBytesAfterSourceChanges();
    void failedRollbackIsReported();
    void successfulRollbackRestoresPreviousProfile();
    void stagedSymlinkIsRejected();
    void stagedProfileIsAccepted();
    void twoRestoresInSameSecondKeepBothPreviousProfiles();
    void hiddenSymlinkIsRejected();
    void stalePartialIsNotLeftBehind();
    void existingArchiveIsReplacedWithoutGap();
    void failedPublishKeepsPreviousArchive();
    void restoredProfileBecomesPrivate();
    void tarCrashIsRejected();
    void tarFailedToStartIsRejected();
};

void TestBackup::tarCrashIsRejected()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString fakeBin = dir.path() + QStringLiteral("/bin");
    QVERIFY(QDir().mkpath(fakeBin));

    QFile fakeTar(fakeBin + QStringLiteral("/tar"));
    QVERIFY(fakeTar.open(QIODevice::WriteOnly));
    fakeTar.write("#!/bin/sh\nexec kill -9 $$\n");
    fakeTar.close();
    QVERIFY(fakeTar.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                   | QFileDevice::ExeOwner | QFileDevice::ReadGroup
                                   | QFileDevice::ExeGroup | QFileDevice::ReadOther
                                   | QFileDevice::ExeOther));

    ScopedEnvironmentVariable restorePath("PATH");
    ScopedEnvironmentVariable restoreDataHome("XDG_DATA_HOME");
    ScopedEnvironmentVariable restoreCacheHome("XDG_CACHE_HOME");
    ScopedEnvironmentVariable restoreRuntimeDir("XDG_RUNTIME_DIR");
    const QByteArray oldPath = qgetenv("PATH");
    qputenv("PATH", QFile::encodeName(fakeBin) + ':' + oldPath);
    qputenv("XDG_DATA_HOME", QFile::encodeName(dir.path() + QStringLiteral("/data")));
    qputenv("XDG_CACHE_HOME", QFile::encodeName(dir.path() + QStringLiteral("/cache")));
    qputenv("XDG_RUNTIME_DIR", QFile::encodeName(dir.path() + QStringLiteral("/runtime")));
    QVERIFY(QDir().mkpath(dir.path() + QStringLiteral("/data")));
    QVERIFY(QDir().mkpath(dir.path() + QStringLiteral("/cache")));
    QVERIFY(QDir().mkpath(dir.path() + QStringLiteral("/runtime")));
    QVERIFY(QFile::setPermissions(dir.path() + QStringLiteral("/runtime"),
                                  QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                      | QFileDevice::ExeOwner));

    const QString profileDir = dir.path() + QStringLiteral("/profile");
    const QString backupDir = dir.path() + QStringLiteral("/backups");
    QVERIFY(QDir().mkpath(profileDir));
    QFile marker(profileDir + QStringLiteral("/marker.txt"));
    QVERIFY(marker.open(QIODevice::WriteOnly));
    marker.write("ok");
    marker.close();

    QString archive;
    QString error;
    const QDateTime now(QDate(2026, 11, 23), QTime(20, 50, 0));
    QVERIFY(!zs::createBackup(profileDir, backupDir, now, &archive, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY2(error.contains(QStringLiteral("sinal")), qPrintable(error));
    QVERIFY(QDir(backupDir).entryList({QStringLiteral("*.partial")}, QDir::Files).isEmpty());
}

void TestBackup::tarFailedToStartIsRejected()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString fakeBin = dir.path() + QStringLiteral("/bin");
    QVERIFY(QDir().mkpath(fakeBin));

    QFile fakeTar(fakeBin + QStringLiteral("/tar"));
    QVERIFY(fakeTar.open(QIODevice::WriteOnly));
    fakeTar.close();
    QVERIFY(fakeTar.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                   | QFileDevice::ReadGroup | QFileDevice::ReadOther));

    ScopedEnvironmentVariable restorePath("PATH");
    ScopedEnvironmentVariable restoreDataHome("XDG_DATA_HOME");
    ScopedEnvironmentVariable restoreCacheHome("XDG_CACHE_HOME");
    ScopedEnvironmentVariable restoreRuntimeDir("XDG_RUNTIME_DIR");
    qputenv("PATH", QFile::encodeName(fakeBin));
    qputenv("XDG_DATA_HOME", QFile::encodeName(dir.path() + QStringLiteral("/data")));
    qputenv("XDG_CACHE_HOME", QFile::encodeName(dir.path() + QStringLiteral("/cache")));
    qputenv("XDG_RUNTIME_DIR", QFile::encodeName(dir.path() + QStringLiteral("/runtime")));
    QVERIFY(QDir().mkpath(dir.path() + QStringLiteral("/data")));
    QVERIFY(QDir().mkpath(dir.path() + QStringLiteral("/cache")));
    QVERIFY(QDir().mkpath(dir.path() + QStringLiteral("/runtime")));
    QVERIFY(QFile::setPermissions(dir.path() + QStringLiteral("/runtime"),
                                  QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                      | QFileDevice::ExeOwner));

    const QString profileDir = dir.path() + QStringLiteral("/profile");
    const QString backupDir = dir.path() + QStringLiteral("/backups");
    QVERIFY(QDir().mkpath(profileDir));
    QFile marker(profileDir + QStringLiteral("/marker.txt"));
    QVERIFY(marker.open(QIODevice::WriteOnly));
    marker.write("ok");
    marker.close();

    QString archive;
    QString error;
    const QDateTime now(QDate(2026, 11, 23), QTime(20, 50, 0));
    QVERIFY(!zs::createBackup(profileDir, backupDir, now, &archive, &error));
    QVERIFY(!error.isEmpty());
    QVERIFY2(error.contains(QStringLiteral("iniciar tar")), qPrintable(error));
    QVERIFY(QDir(backupDir).entryList({QStringLiteral("*.partial")}, QDir::Files).isEmpty());
}

void TestBackup::keepsNewestFour()
{
    const QStringList all{
        QStringLiteral("zapshell-20260101-000000.tar.gz"),
        QStringLiteral("zapshell-20260102-000000.tar.gz"),
        QStringLiteral("zapshell-20260103-000000.tar.gz"),
        QStringLiteral("zapshell-20260104-000000.tar.gz"),
        QStringLiteral("zapshell-20260105-000000.tar.gz"),
    };
    QCOMPARE(zs::backupsToDelete(all, 4),
             QStringList{QStringLiteral("zapshell-20260101-000000.tar.gz")});
}

void TestBackup::nameIsSortable()
{
    const QDateTime when(QDate(2026, 11, 18), QTime(9, 5, 3));
    QCOMPARE(zs::backupName(when), QStringLiteral("zapshell-20261118-090503.tar.gz"));
}

void TestBackup::createAndRestoreRoundTrip()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString profile = dir.path() + "/profile";
    const QString backups = dir.path() + "/backups";
    QVERIFY(QDir().mkpath(profile));

    QFile original(profile + "/history.dat");
    QVERIFY(original.open(QIODevice::WriteOnly));
    original.write("historico");
    original.close();

    const QDateTime now(QDate(2026, 11, 18), QTime(9, 5, 3));
    QString archive;
    QString error;
    QVERIFY2(zs::createBackup(profile, backups, now, &archive, &error), qPrintable(error));
    QVERIFY(QFile::exists(archive));

    QFile changed(profile + "/history.dat");
    QVERIFY(changed.open(QIODevice::WriteOnly));
    changed.write("corrompido");
    changed.close();

    QVERIFY2(zs::restoreBackup(archive, profile, &error), qPrintable(error));
    QFile restored(profile + "/history.dat");
    QVERIFY(restored.open(QIODevice::ReadOnly));
    QCOMPARE(restored.readAll(), QByteArray("historico"));
}

void TestBackup::refusesPathTraversal()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = dir.path() + "/src";
    QVERIFY(QDir().mkpath(source + "/profile"));

    QFile file(source + "/profile/ok.txt");
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("ok");
    file.close();

    QProcess tar;
    tar.start(QStringLiteral("tar"),
              {QStringLiteral("-czf"), dir.path() + "/evil.tar.gz", QStringLiteral("-C"), source,
               QStringLiteral("--transform"),
               QStringLiteral("s|^profile/ok.txt|profile/../../evil|"),
               QStringLiteral("profile/ok.txt")});
    QVERIFY(tar.waitForFinished(10000));
    QCOMPARE(tar.exitCode(), 0);

    QString error;
    QVERIFY(!zs::restoreBackup(dir.path() + "/evil.tar.gz", dir.path() + "/profile", &error));
    QVERIFY(!error.isEmpty());
}

void TestBackup::restoresWhenProfileIsMissing()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString profile = dir.path() + "/profile";
    const QString backups = dir.path() + "/backups";
    QVERIFY(QDir().mkpath(profile));

    QFile original(profile + "/history.dat");
    QVERIFY(original.open(QIODevice::WriteOnly));
    original.write("sobrevivente");
    original.close();

    const QDateTime now(QDate(2026, 11, 18), QTime(9, 5, 3));
    QString archive;
    QString error;
    QVERIFY2(zs::createBackup(profile, backups, now, &archive, &error), qPrintable(error));

    // Perdemos o diretório do perfil inteiro — é exatamente o cenário do restore.
    QVERIFY(QDir(profile).removeRecursively());
    QVERIFY(!QFileInfo::exists(profile));

    QVERIFY2(zs::restoreBackup(archive, profile, &error), qPrintable(error));
    QFile restored(profile + "/history.dat");
    QVERIFY(restored.open(QIODevice::ReadOnly));
    QCOMPARE(restored.readAll(), QByteArray("sobrevivente"));
}

void TestBackup::snapshotKeepsBytesAfterSourceChanges()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = dir.path() + "/archive.tar.gz";
    const QString snapshot = dir.path() + "/snapshot.tar.gz";

    QFile original(source);
    QVERIFY(original.open(QIODevice::WriteOnly));
    original.write("conteudo original");
    original.close();

    QString error;
    QVERIFY2(zs::snapshotArchive(source, snapshot, &error), qPrintable(error));

    QFile changed(source);
    QVERIFY(changed.open(QIODevice::WriteOnly));
    changed.write("conteudo trocado");
    changed.close();

    QFile copy(snapshot);
    QVERIFY(copy.open(QIODevice::ReadOnly));
    QCOMPARE(copy.readAll(), QByteArray("conteudo original"));
}

void TestBackup::failedRollbackIsReported()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString previous = dir.path() + "/profile.before-restore";
    const QString profile = dir.path() + "/profile";
    QVERIFY(QDir().mkpath(previous));
    QVERIFY(QDir().mkpath(profile + "/bloqueio")); // destino não vazio: rename falha

    QString error;
    QVERIFY(!zs::restorePreviousProfile(previous, profile, true, &error));
    QVERIFY2(error.contains(previous), qPrintable(error));
    QVERIFY(QFileInfo::exists(previous));
}

void TestBackup::successfulRollbackRestoresPreviousProfile()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString previous = dir.path() + "/profile.before-restore";
    const QString profile = dir.path() + "/profile";
    QVERIFY(QDir().mkpath(previous));

    QFile marker(previous + "/marker.txt");
    QVERIFY(marker.open(QIODevice::WriteOnly));
    marker.write("ok");
    marker.close();

    QString error;
    QVERIFY2(zs::restorePreviousProfile(previous, profile, true, &error), qPrintable(error));
    QVERIFY(QFileInfo::exists(profile + "/marker.txt"));
    QVERIFY(!QFileInfo::exists(previous));
}

void TestBackup::stagedSymlinkIsRejected()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString root = dir.path() + "/profile";
    QVERIFY(QDir().mkpath(root));
    QVERIFY(QFile::link(QStringLiteral("/etc/passwd"), root + "/link"));

    QString error;
    QVERIFY(!zs::validateStagedProfile(dir.path(), QStringLiteral("profile"), &error));
    QVERIFY(error.contains(QStringLiteral("link")));
}

void TestBackup::stagedProfileIsAccepted()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString root = dir.path() + "/profile";
    QVERIFY(QDir().mkpath(root + "/sub"));

    QFile marker(root + "/sub/marker.txt");
    QVERIFY(marker.open(QIODevice::WriteOnly));
    marker.write("ok");
    marker.close();

    QString error;
    QVERIFY2(zs::validateStagedProfile(dir.path(), QStringLiteral("profile"), &error),
             qPrintable(error));
}

void TestBackup::twoRestoresInSameSecondKeepBothPreviousProfiles()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString profile = dir.path() + "/profile";
    const QString backups = dir.path() + "/backups";
    QVERIFY(QDir().mkpath(profile));

    QFile original(profile + "/history.dat");
    QVERIFY(original.open(QIODevice::WriteOnly));
    original.write("original");
    original.close();

    QString archive;
    QString error;
    const QDateTime backupTime(QDate(2026, 11, 18), QTime(9, 5, 3));
    QVERIFY2(zs::createBackup(profile, backups, backupTime, &archive, &error), qPrintable(error));

    const QDateTime first(QDate(2026, 11, 23), QTime(20, 31, 20, 100));
    const QDateTime second(QDate(2026, 11, 23), QTime(20, 31, 20, 900));
    QVERIFY2(zs::restoreBackup(archive, profile, &error, first), qPrintable(error));
    QVERIFY2(zs::restoreBackup(archive, profile, &error, second), qPrintable(error));

    const QStringList kept = QDir(dir.path()).entryList(
        {QStringLiteral("profile.before-restore-*")}, QDir::Dirs);
    QCOMPARE(kept.size(), 2);
}

void TestBackup::hiddenSymlinkIsRejected()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString root = dir.path() + "/profile";
    QVERIFY(QDir().mkpath(root));
    QVERIFY(QFile::link(QStringLiteral("/etc/passwd"), root + "/.oculto"));

    QString error;
    QVERIFY(!zs::validateStagedProfile(dir.path(), QStringLiteral("profile"), &error));
    QVERIFY(error.contains(QStringLiteral(".oculto")));
}

void TestBackup::stalePartialIsNotLeftBehind()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString profile = dir.path() + "/profile";
    const QString backups = dir.path() + "/backups";
    QVERIFY(QDir().mkpath(profile));
    QVERIFY(QDir().mkpath(backups));

    QFile marker(profile + "/marker.txt");
    QVERIFY(marker.open(QIODevice::WriteOnly));
    marker.write("ok");
    marker.close();

    const QDateTime now(QDate(2026, 11, 23), QTime(20, 50, 0));
    const QString archive = backups + "/" + zs::backupName(now);
    QFile stale(archive + QStringLiteral(".partial"));
    QVERIFY(stale.open(QIODevice::WriteOnly));
    stale.write("resto de tentativa anterior");
    stale.close();

    QString out;
    QString error;
    QVERIFY2(zs::createBackup(profile, backups, now, &out, &error), qPrintable(error));
    QVERIFY(QFile::exists(archive));
    QVERIFY(!QFile::exists(archive + QStringLiteral(".partial")));
}

void TestBackup::existingArchiveIsReplacedWithoutGap()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString profile = dir.path() + "/profile";
    const QString backups = dir.path() + "/backups";
    QVERIFY(QDir().mkpath(profile));

    const QDateTime now(QDate(2026, 11, 23), QTime(21, 0, 0));
    const QString archive = backups + "/" + zs::backupName(now);

    QFile oldMarker(profile + "/marker.txt");
    QVERIFY(oldMarker.open(QIODevice::WriteOnly));
    oldMarker.write("antigo");
    oldMarker.close();
    QString first;
    QString error;
    QVERIFY2(zs::createBackup(profile, backups, now, &first, &error), qPrintable(error));
    QCOMPARE(first, archive);

    QFile newMarker(profile + "/marker.txt");
    QVERIFY(newMarker.open(QIODevice::WriteOnly));
    newMarker.write("novo");
    newMarker.close();
    QString second;
    QVERIFY2(zs::createBackup(profile, backups, now, &second, &error), qPrintable(error));
    QCOMPARE(second, archive);
    QVERIFY(QFile::exists(archive));
    QVERIFY(!QFile::exists(archive + QStringLiteral(".partial")));

    QProcess tar;
    tar.start(QStringLiteral("tar"), {QStringLiteral("-xzOf"), archive,
                                      QStringLiteral("profile/marker.txt")});
    QVERIFY(tar.waitForFinished(10000));
    QCOMPARE(tar.readAllStandardOutput(), QByteArray("novo"));
}

void TestBackup::failedPublishKeepsPreviousArchive()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString profile = dir.path() + "/profile";
    const QString backups = dir.path() + "/backups";
    QVERIFY(QDir().mkpath(profile));
    QVERIFY(QDir().mkpath(backups));

    QFile marker(profile + "/marker.txt");
    QVERIFY(marker.open(QIODevice::WriteOnly));
    marker.write("sobrevivente");
    marker.close();

    const QDateTime timestampA(QDate(2026, 11, 23), QTime(21, 0, 0));
    const QString archiveA = backups + "/" + zs::backupName(timestampA);
    QString error;
    QString out;
    QVERIFY2(zs::createBackup(profile, backups, timestampA, &out, &error), qPrintable(error));
    QCOMPARE(out, archiveA);
    QVERIFY(QFile::exists(archiveA));

    QProcess tar;
    tar.start(QStringLiteral("tar"), {QStringLiteral("-tzf"), archiveA});
    QVERIFY(tar.waitForFinished(10000));
    QCOMPARE(tar.exitCode(), 0);

    const QDateTime timestampB(QDate(2026, 11, 23), QTime(21, 5, 0));
    const QString archiveB = backups + "/" + zs::backupName(timestampB);
    QVERIFY(QDir().mkpath(archiveB));
    QString retryError;
    QVERIFY(!zs::createBackup(profile, backups, timestampB, nullptr, &retryError));
    QVERIFY(QFile::exists(archiveA));
    QProcess verifyA;
    verifyA.start(QStringLiteral("tar"), {QStringLiteral("-tzf"), archiveA});
    QVERIFY(verifyA.waitForFinished(10000));
    QCOMPARE(verifyA.exitCode(), 0);
    QVERIFY(QDir(archiveB).exists());
    QVERIFY(!retryError.isEmpty());
    QVERIFY(!QFile::exists(archiveB + QStringLiteral(".partial")));
}

void TestBackup::restoredProfileBecomesPrivate()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString source = dir.path() + "/src";
    const QString profile = dir.path() + "/profile";
    QVERIFY(QDir().mkpath(source + "/profile"));
    QVERIFY(QFile::setPermissions(source + "/profile",
                                  QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner |
                                      QFile::ReadGroup | QFile::ExeGroup | QFile::ReadOther
                                      | QFile::ExeOther));
    QFile session(source + "/profile/session.dat");
    QVERIFY(session.open(QIODevice::WriteOnly));
    session.write("sessao");
    session.close();

    QProcess tar;
    tar.start(QStringLiteral("tar"),
              {QStringLiteral("-czf"), dir.path() + "/old.tar.gz", QStringLiteral("-C"), source,
               QStringLiteral("profile")});
    QVERIFY(tar.waitForFinished(10000));
    QCOMPARE(tar.exitCode(), 0);

    QString error;
    QVERIFY2(zs::restoreBackup(dir.path() + "/old.tar.gz", profile, &error), qPrintable(error));
    QCOMPARE(QFileInfo(profile).permissions(),
             QFile::Permissions(QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner
                                | QFile::ReadUser | QFile::WriteUser | QFile::ExeUser));
}

QTEST_GUILESS_MAIN(TestBackup)
#include "tst_backup.moc"
