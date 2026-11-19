#pragma once

#include <QObject>
#include <QSystemTrayIcon>

class QAction;
class QMenu;

namespace zs {

// Ícone de bandeja: mostrar/ocultar, recarregar, backup, silenciar e sair.
class Tray : public QObject
{
    Q_OBJECT

public:
    explicit Tray(QObject *parent = nullptr);

    void setUnread(int count);
    void toggleSilent();

signals:
    void showRequested();
    void reloadRequested();
    void backupRequested();
    void silentToggled(bool silent);
    void quitRequested();

private:
    QSystemTrayIcon *m_icon = nullptr;
    QAction *m_silent = nullptr;
};

} // namespace zs
