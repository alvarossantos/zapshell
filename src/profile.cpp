#include "profile.h"

#include "paths.h"
#include "web.h"

#include <QDir>
#include <QFileInfo>
#include <QStandardPaths>
#include <QUrl>

#include <QWebEngineDownloadRequest>
#include <QWebEnginePage>
#include <QWebEnginePermission>
#include <QWebEngineProfile>
#include <QWebEngineQuotaRequest>
#include <QWebEngineSettings>

namespace zs {
namespace {

QString originOf(const QUrl &url)
{
    QString origin = url.scheme() + "://" + url.host();
    if (url.port() != -1)
        origin += ":" + QString::number(url.port());
    return origin;
}

QString uniqueFileName(const QString &directory, const QString &fileName)
{
    const QFileInfo info(fileName);
    const QString base = info.completeBaseName();
    const QString suffix = info.suffix().isEmpty() ? QString() : "." + info.suffix();

    QString candidate = fileName;
    for (int i = 1; QFileInfo::exists(directory + "/" + candidate); ++i)
        candidate = base + " (" + QString::number(i) + ")" + suffix;
    return candidate;
}

} // namespace

Profile::Profile(QObject *parent)
    : QObject(parent)
{
    QDir().mkpath(paths::profileDir());
    QDir().mkpath(paths::cacheDir());

    m_profile = new QWebEngineProfile(QStringLiteral("zapshell"), this);
    m_profile->setPersistentStoragePath(paths::profileDir());
    m_profile->setCachePath(paths::cacheDir());
    m_profile->setPersistentCookiesPolicy(QWebEngineProfile::ForcePersistentCookies);
    m_profile->setPersistentPermissionsPolicy(
        QWebEngineProfile::PersistentPermissionsPolicy::StoreOnDisk);
    m_profile->setHttpCacheType(QWebEngineProfile::DiskHttpCache);
    m_profile->setHttpCacheMaximumSize(512 * 1024 * 1024);
    m_profile->setHttpUserAgent(web::normalizeUserAgent(m_profile->httpUserAgent()));

    QWebEngineSettings *settings = m_profile->settings();
    settings->setAttribute(QWebEngineSettings::JavascriptEnabled, true);
    settings->setAttribute(QWebEngineSettings::LocalStorageEnabled, true);
    settings->setAttribute(QWebEngineSettings::ScreenCaptureEnabled, true);
    settings->setAttribute(QWebEngineSettings::PdfViewerEnabled, false);
    settings->setAttribute(QWebEngineSettings::WebGLEnabled, true);

    m_page = new QWebEnginePage(m_profile, this);

    connect(m_page, &QWebEnginePage::permissionRequested,
            this, &Profile::onPermissionRequested);
    connect(m_page, &QWebEnginePage::quotaRequested, this, &Profile::onQuotaRequested);
    connect(m_profile, &QWebEngineProfile::downloadRequested,
            this, &Profile::onDownloadRequested);
}

Profile::~Profile()
{
    // A página precisa morrer antes do perfil; senão o Qt avisa
    // "Release of profile requested but WebEnginePage still not deleted".
    delete m_page;
    m_page = nullptr;
}

void Profile::onPermissionRequested(QWebEnginePermission permission){
    if (!permission.isValid())
        return;

    const bool granted = web::permissionGranted(originOf(permission.origin()),
                                                int(permission.permissionType()));
    if (granted)
        permission.grant();
    else
        permission.deny();
}

void Profile::onQuotaRequested(QWebEngineQuotaRequest request)
{
    // Sem aceitar a quota o IndexedDB do WhatsApp fica efêmero e o histórico
    // de mensagens/mídia se perde.
    if (web::trustedOrigin(originOf(request.origin())))
        request.accept();
    else
        request.reject();
}

void Profile::onDownloadRequested(QWebEngineDownloadRequest *download)
{
    const QString directory =
        QStandardPaths::writableLocation(QStandardPaths::DownloadLocation);
    QDir().mkpath(directory);

    download->setDownloadDirectory(directory);
    download->setDownloadFileName(uniqueFileName(directory, download->downloadFileName()));
    download->accept();
}

} // namespace zs
