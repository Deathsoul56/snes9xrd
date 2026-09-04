#pragma once

#include <QByteArray>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>

class AchievementsClient;

// Owns the QNetworkAccessManager on the GUI thread -- QNAM must live on a
// thread that pumps a Qt event loop, which the emulation thread deliberately
// does not (see AchievementsClient's class comment). AchievementsClient hops
// here via a queued Q_INVOKABLE call from whatever thread rc_client decides
// to make a server call from, the exact same cross-thread idiom
// Snes9xController::S9xMessage() already uses to reach EmuMainWindow's
// showCoreError (QMetaObject::invokeMethod with Qt::QueuedConnection).
//
// This class knows nothing about rc_client_t -- request_handle is an opaque
// token handed back unchanged to AchievementsClient::enqueueCompletedResponse
// once the reply completes, keeping the HTTP transport reusable and
// independent of the RetroAchievements SDK types.
class AchievementsNetwork : public QObject
{
    Q_OBJECT

  public:
    explicit AchievementsNetwork(AchievementsClient *client);

    Q_INVOKABLE void dispatch(const QString &url, const QByteArray &post_data,
                               const QString &content_type, const QString &user_agent,
                               void *request_handle);

    // Plain image GET (e.g. an achievement badge) -- decoded and handed to
    // AchievementsClient::enqueueBadgeImage(), independent of dispatch()'s
    // rc_client request/response protocol.
    Q_INVOKABLE void fetchImage(const QString &url, const QString &user_agent);

  private:
    AchievementsClient *client;
    QNetworkAccessManager manager;
};
