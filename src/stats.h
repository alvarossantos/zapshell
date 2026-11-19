#pragma once

#include <QString>

namespace zs {

quint64 parseVmRssKb(const QString &procStatus);

// Soma VmRSS do processo e de todos os descendentes (inclui os processos do
// Chromium/WebEngine).
quint64 processTreeRssKb(qint64 rootPid);

// Jiffies de CPU (utime+stime) da árvore — usado para amostrar % CPU.
quint64 processTreeJiffies(qint64 rootPid);

QString formatStats(quint64 rssKb, double cpuPercent);

} // namespace zs
