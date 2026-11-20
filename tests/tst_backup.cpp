#include <QtTest>

#include <QDateTime>
#include <QProcess>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>

#include "backup.h"

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
};

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

QTEST_GUILESS_MAIN(TestBackup)
#include "tst_backup.moc"
