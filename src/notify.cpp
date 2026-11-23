#include "notify.h"

#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusMessage>
#include <QDBusReply>
#include <QDebug>
#include <QHash>
#include <QRegularExpression>

#include <algorithm>

namespace zs {
namespace {

const char *const kService = "org.freedesktop.Notifications";
const char *const kPath = "/org/freedesktop/Notifications";
const char *const kInterface = "org.freedesktop.Notifications";

constexpr int kMaxSeenTitles = 256;

QHash<QString, qint64> &lastSeen()
{
    static QHash<QString, qint64> seen;
    return seen;
}

void rememberTitle(const QString &title, qint64 nowMs)
{
    QHash<QString, qint64> &seen = lastSeen();
    if (!seen.contains(title) && seen.size() >= kMaxSeenTitles) {
        const auto oldest = std::min_element(seen.cbegin(), seen.cend());
        if (oldest != seen.cend())
            seen.remove(oldest.key());
    }
    seen.insert(title, nowMs);
}

} // namespace

NotifyDecision decideNotify(const QString &title, qint64 nowMs, bool silent, qint64 groupWindowMs)
{
    if (silent)
        return NotifyDecision::Drop;

    QHash<QString, qint64> &seen = lastSeen();
    const auto it = seen.constFind(title);
    if (it != seen.cend() && nowMs - *it < groupWindowMs) {
        seen[title] = nowMs;
        return NotifyDecision::Group;
    }

    rememberTitle(title, nowMs);
    return NotifyDecision::Send;
}

int unreadFromTitle(const QString &title)
{
    static const QRegularExpression badge(QStringLiteral("^\\((\\d+)\\)"));
    const QRegularExpressionMatch match = badge.match(title);
    return match.hasMatch() ? match.captured(1).toInt() : 0;
}

QStringList notificationActions()
{
    return {QStringLiteral("default"), QStringLiteral("Abrir")};
}

QString escapeNotificationText(const QString &text)
{
    return text.toHtmlEscaped();
}

Notifier::Notifier(QObject *parent)
    : QObject(parent)
{
    m_interface = new QDBusInterface(QString::fromLatin1(kService), QString::fromLatin1(kPath),
                                     QString::fromLatin1(kInterface), QDBusConnection::sessionBus(),
                                     this);

    if (!m_interface->isValid())
        qWarning() << "zapshell: servidor de notificações indisponível:"
                   << m_interface->lastError().message();

    QDBusConnection::sessionBus().connect(
        QString::fromLatin1(kService), QString::fromLatin1(kPath), QString::fromLatin1(kInterface),
        QStringLiteral("ActionInvoked"), this, SLOT(onActionInvoked(uint, QString)));
}

void Notifier::onActionInvoked(uint id, const QString &key)
{
    Q_UNUSED(id)
    if (key == QLatin1String("default"))
        emit clicked();
}

bool Notifier::show(const QString &summary, const QString &body)
{
    if (m_silent)
        return false;

    if (!m_interface->isValid())
        return false;

    const QDBusMessage reply =
        m_interface->call(QStringLiteral("Notify"), QStringLiteral("zapshell"), uint(0), QString(),
                          escapeNotificationText(summary), escapeNotificationText(body),
                          notificationActions(), QVariantMap(), 5000);
    if (reply.type() == QDBusMessage::ErrorMessage) {
        qWarning() << "zapshell: falha ao notificar:" << reply.errorMessage();
        return false;
    }
    return true;
}

} // namespace zs
