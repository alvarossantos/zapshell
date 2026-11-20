#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>

namespace zs {

QString backupName(const QDateTime &now);

// Nomes ordenáveis: devolve os mais antigos que excedem `keep`.
QStringList backupsToDelete(const QStringList &names, int keep = 4);

// Tira o tar com o app parado (o chamador garante). Retenção aplicada ao criar.
bool createBackup(const QString &profileDir, const QString &backupDir, const QDateTime &now,
                  QString *outPath, QString *error);

// Cópia estável do archive: validação e extração devem ler os mesmos bytes,
// mesmo que o arquivo de origem seja trocado no meio do restore.
bool snapshotArchive(const QString &source, const QString &destination, QString *error);

// Valida o archive antes de tocar no perfil atual. Em caso de falha, o perfil
// atual fica intacto; em caso de sucesso ele é guardado como .before-restore-*.
bool restoreBackup(const QString &archive, const QString &profileDir, QString *error);

} // namespace zs
