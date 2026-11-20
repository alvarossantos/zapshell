#include "web.h"

#include "webfeatures.h"

#include <QFileInfo>
#include <QRegularExpression>

#include <QWebEnginePage>
#include <QWebEnginePermission>

static_assert(int(QWebEnginePermission::PermissionType::MediaAudioCapture) == zs::FeatureMediaAudioCapture,
              "FeatureMediaAudioCapture divergiu de QWebEnginePermission::PermissionType");
static_assert(int(QWebEnginePermission::PermissionType::MediaVideoCapture) == zs::FeatureMediaVideoCapture,
              "FeatureMediaVideoCapture divergiu de QWebEnginePermission::PermissionType");
static_assert(int(QWebEnginePermission::PermissionType::MediaAudioVideoCapture)
                  == zs::FeatureMediaAudioVideoCapture,
              "FeatureMediaAudioVideoCapture divergiu de QWebEnginePermission::PermissionType");
static_assert(int(QWebEnginePermission::PermissionType::DesktopVideoCapture) == zs::FeatureDesktopVideoCapture,
              "FeatureDesktopVideoCapture divergiu de QWebEnginePermission::PermissionType");
static_assert(int(QWebEnginePermission::PermissionType::DesktopAudioVideoCapture)
                  == zs::FeatureDesktopAudioVideoCapture,
              "FeatureDesktopAudioVideoCapture divergiu de QWebEnginePermission::PermissionType");
static_assert(int(QWebEnginePermission::PermissionType::Notifications) == zs::FeatureNotifications,
              "FeatureNotifications divergiu de QWebEnginePermission::PermissionType");

namespace zs::web {
namespace {

const char *const kDefaultFlags =
    "--renderer-process-limit=1 "
    "--js-flags=--max-old-space-size=256 "
    "--disable-gpu "
    "--disable-features=Translate,MediaRouter,OptimizationHints "
    "--disable-background-networking";

const QString kWhatsappOrigin = QStringLiteral("https://web.whatsapp.com");

} // namespace

QString chromiumFlags(const QString &overrides)
{
    const QString trimmed = overrides.trimmed();
    return trimmed.isEmpty() ? QString::fromLatin1(kDefaultFlags) : trimmed;
}

QString normalizeUserAgent(QString userAgent)
{
    userAgent.remove(QRegularExpression(QStringLiteral("QtWebEngine/[\\d.]+")));
    userAgent.remove(QRegularExpression(QStringLiteral("Qt/[\\d.]+")));
    return userAgent.simplified();
}

QString uniqueFileName(const QString &directory, const QString &fileName)
{
    QString name = QFileInfo(fileName).fileName();
    if (name.isEmpty() || name == QLatin1String(".") || name == QLatin1String("..")
        || name.contains(QLatin1Char('\\')))
        name = QStringLiteral("download");

    const QFileInfo info(name);
    const QString base = info.completeBaseName();
    const QString suffix = info.suffix().isEmpty() ? QString() : "." + info.suffix();

    QString candidate = name;
    for (int i = 1; QFileInfo::exists(directory + "/" + candidate); ++i)
        candidate = base + " (" + QString::number(i) + ")" + suffix;
    return candidate;
}

bool trustedOrigin(const QString &origin)
{
    return origin == kWhatsappOrigin;
}

bool permissionGranted(const QString &origin, int feature)
{
    if (!trustedOrigin(origin))
        return false;

    switch (feature) {
    case FeatureMediaAudioCapture:
    case FeatureMediaVideoCapture:
    case FeatureMediaAudioVideoCapture:
    case FeatureDesktopVideoCapture:
    case FeatureDesktopAudioVideoCapture:
    case FeatureNotifications:
        return true;
    default:
        return false;
    }
}

} // namespace zs::web
