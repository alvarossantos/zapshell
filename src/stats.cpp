#include "stats.h"

#include <cmath>
#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QSet>

namespace zs {
namespace {

QList<qint64> treePids(qint64 rootPid)
{
    QList<qint64> pending{rootPid};
    QSet<qint64> seen;
    QList<qint64> result;

    while (!pending.isEmpty()) {
        const qint64 pid = pending.takeFirst();
        if (pid <= 0 || seen.contains(pid))
            continue;
        seen.insert(pid);
        result << pid;

        const QString tasks = QStringLiteral("/proc/%1/task").arg(pid);
        for (const QString &task : QDir(tasks).entryList(QDir::Dirs | QDir::NoDotAndDotDot)) {
            QFile children(tasks + "/" + task + "/children");
            if (!children.open(QIODevice::ReadOnly))
                continue;
            for (const QByteArray &child : children.readAll().split(' ')) {
                bool ok = false;
                const qint64 value = child.toLongLong(&ok);
                if (ok)
                    pending << value;
            }
        }
    }
    return result;
}

QString procFile(qint64 pid, const QString &name)
{
    return QStringLiteral("/proc/%1/%2").arg(pid).arg(name);
}

} // namespace

quint64 parseVmRssKb(const QString &procStatus)
{
    static const QRegularExpression rss(QStringLiteral("^VmRSS:\\s*(\\d+)\\s*kB"),
                                        QRegularExpression::MultilineOption);
    const QRegularExpressionMatch match = rss.match(procStatus);
    return match.hasMatch() ? match.captured(1).toULongLong() : 0;
}

quint64 processTreeRssKb(qint64 rootPid)
{
    quint64 total = 0;
    for (const qint64 pid : treePids(rootPid)) {
        QFile status(procFile(pid, QStringLiteral("status")));
        if (!status.open(QIODevice::ReadOnly))
            continue;
        total += parseVmRssKb(QString::fromLocal8Bit(status.readAll()));
    }
    return total;
}

quint64 processTreeJiffies(qint64 rootPid)
{
    quint64 total = 0;
    for (const qint64 pid : treePids(rootPid)) {
        QFile stat(procFile(pid, QStringLiteral("stat")));
        if (!stat.open(QIODevice::ReadOnly))
            continue;
        const QString line = QString::fromLocal8Bit(stat.readAll());
        const int close = line.lastIndexOf(QLatin1Char(')'));
        if (close < 0)
            continue;
        const QStringList fields =
            line.mid(close + 1).trimmed().split(' ', Qt::SkipEmptyParts);
        // Depois de ')': índice 0 = state(3), 11 = utime(14), 12 = stime(15).
        if (fields.size() > 12)
            total += fields.at(11).toULongLong() + fields.at(12).toULongLong();
    }
    return total;
}

QString formatStats(quint64 rssKb, double cpuPercent)
{
    return QStringLiteral("zapshell: %1 MB RSS, %2% CPU")
        .arg(rssKb / 1024)
        .arg(std::floor(cpuPercent * 10.0) / 10.0, 0, 'f', 1);
}

} // namespace zs
