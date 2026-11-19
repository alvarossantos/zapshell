#pragma once

#include <QObject>

class QWebEngineDownloadRequest;
class QWebEnginePage;
class QWebEnginePermission;
class QWebEngineProfile;
class QWebEngineQuotaRequest;

namespace zs {

// Perfil persistente do WhatsApp: histórico, mídia e permissões sobrevivem a
// reinícios. O perfil mora em paths::profileDir(); paths::cacheDir() recebe
// somente o cache HTTP.
class Profile : public QObject
{
    Q_OBJECT

public:
    explicit Profile(QObject *parent = nullptr);
    ~Profile() override;

    QWebEngineProfile *profile() const { return m_profile; }
    QWebEnginePage *page() const { return m_page; }

signals:
    void downloadFinished(const QString &fileName);

private:
    void onPermissionRequested(QWebEnginePermission permission);
    void onQuotaRequested(QWebEngineQuotaRequest request);
    void onDownloadRequested(QWebEngineDownloadRequest *download);

    QWebEngineProfile *m_profile = nullptr;
    QWebEnginePage *m_page = nullptr;
};

} // namespace zs
