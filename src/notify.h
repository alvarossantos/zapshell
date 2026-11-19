#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class QDBusInterface;

namespace zs {

enum class NotifyDecision {
    Send,
    Group,
    Drop,
};

// Decide o destino de uma notificação. Repetições do mesmo título dentro de
// `groupWindowMs` viram Group (não notificam de novo); modo silencioso descarta.
NotifyDecision decideNotify(const QString &title, qint64 nowMs, bool silent,
                            qint64 groupWindowMs = 3000);

// "(3) WhatsApp" -> 3; sem badge -> 0.
int unreadFromTitle(const QString &title);

// A chave `default` é a que o servidor de notificações devolve em ActionInvoked;
// sem ela, clicar na notificação não abre a janela.
QStringList notificationActions();

// Notificações nativas via org.freedesktop.Notifications. Best-effort: se o
// DBus falhar, o chat continua funcionando.
class Notifier : public QObject
{
    Q_OBJECT

public:
    explicit Notifier(QObject *parent = nullptr);

    void setSilent(bool silent) { m_silent = silent; }
    bool isSilent() const { return m_silent; }

    bool show(const QString &summary, const QString &body);

signals:
    void clicked();

private slots:
    void onActionInvoked(uint id, const QString &key);

private:
    QDBusInterface *m_interface = nullptr;
    bool m_silent = false;
};

} // namespace zs
