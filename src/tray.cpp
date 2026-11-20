#include "tray.h"

#include <QAction>
#include <QApplication>
#include <QIcon>
#include <QMenu>
#include <QStyle>

namespace zs {

Tray::Tray(QObject *parent)
    : QObject(parent)
{
    QIcon icon = QIcon::fromTheme(QStringLiteral("zapshell-tray"));
    if (icon.isNull())
        icon = QIcon::fromTheme(QStringLiteral("dialog-information"));
    if (icon.isNull())
        icon = qApp->style()->standardIcon(QStyle::SP_MessageBoxInformation);

    m_icon = new QSystemTrayIcon(icon, this);
    m_icon->setToolTip(QStringLiteral("zapshell"));

    auto *menu = new QMenu();

    QAction *show = menu->addAction(QStringLiteral("Mostrar/Ocultar"));
    connect(show, &QAction::triggered, this, &Tray::showRequested);

    QAction *reload = menu->addAction(QStringLiteral("Recarregar"));
    connect(reload, &QAction::triggered, this, &Tray::reloadRequested);

    QAction *backup = menu->addAction(QStringLiteral("Backup agora"));
    connect(backup, &QAction::triggered, this, &Tray::backupRequested);

    menu->addSeparator();

    m_silent = menu->addAction(QStringLiteral("Silenciar"));
    m_silent->setCheckable(true);
    connect(m_silent, &QAction::toggled, this, &Tray::silentToggled);

    menu->addSeparator();

    QAction *quit = menu->addAction(QStringLiteral("Sair"));
    connect(quit, &QAction::triggered, this, &Tray::quitRequested);

    m_icon->setContextMenu(menu);
    connect(m_icon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger)
            emit showRequested();
    });

    m_icon->show();
}

void Tray::setUnread(int count)
{
    m_icon->setToolTip(count > 0 ? QStringLiteral("zapshell — %1 não lidas").arg(count)
                                 : QStringLiteral("zapshell"));
}

void Tray::toggleSilent()
{
    m_silent->toggle();
}

} // namespace zs
