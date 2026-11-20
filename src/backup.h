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

// Devolve o perfil anterior quando a instalação do restaurado falha. `false`
// com `error` contendo o caminho preservado se a devolução também falhar.
bool restorePreviousProfile(const QString &displaced, const QString &profileDir,
                            bool hadProfile, QString *error);

// Defesa extra: valida o que realmente foi escrito no staging, além da
// validação textual da listagem do tar (que continua rejeitando l/h/devices).
bool validateStagedProfile(const QString &stagingPath, const QString &name, QString *error);

// Valida o archive antes de tocar no perfil atual. Em caso de falha, o perfil
// atual fica intacto; em caso de sucesso ele é guardado como .before-restore-*.
bool restoreBackup(const QString &archive, const QString &profileDir, QString *error);

} // namespace zs
