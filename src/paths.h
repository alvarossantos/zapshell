#pragma once

#include <QString>

namespace zs::paths {

// $XDG_DATA_HOME/zapshell
QString dataDir();
// $XDG_DATA_HOME/zapshell/profile — nunca em ~/.cache
QString profileDir();
// $XDG_CACHE_HOME/zapshell (cache HTTP apenas)
QString cacheDir();
// $XDG_DATA_HOME/zapshell/backups
QString backupDir();
// $XDG_RUNTIME_DIR/zapshell.sock
QString socketPath();

} // namespace zs::paths
