#include "AchievementsNetwork.hpp"
#include "AchievementsClient.hpp"

#include <QImage>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

AchievementsNetwork::AchievementsNetwork(AchievementsClient *client_) : client(client_)
{
}

void AchievementsNetwork::dispatch(const QString &url, const QByteArray &post_data,
                                    const QString &content_type, const QString &user_agent,
                                    void *request_handle)
{
    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::UserAgentHeader, user_agent.toUtf8());
    if (!content_type.isEmpty())
        request.setHeader(QNetworkRequest::ContentTypeHeader, content_type.toUtf8());

    // rc_client leaves post_data null for a GET request, non-null for POST --
    // preserve that distinction rather than inferring it from emptiness.
    QNetworkReply *reply = post_data.isNull() ? manager.get(request) : manager.post(request, post_data);

    connect(reply, &QNetworkReply::finished, this, [this, reply, request_handle] {
        int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        QByteArray body = reply->readAll();
        std::string error = reply->error() == QNetworkReply::NoError ? std::string() : reply->errorString().toStdString();
        client->enqueueCompletedResponse(status, body.toStdString(), error, request_handle);
        reply->deleteLater();
    });
}

void AchievementsNetwork::fetchImage(const QString &url, const QString &user_agent)
{
    // Plain GET, decoded straight to RGBA8888 via Qt's own PNG codec --
    // deliberately not routed through dispatch()/enqueueCompletedResponse(),
    // since those exist to carry rc_client's request/response protocol, not
    // arbitrary image bytes.
    QNetworkRequest request{QUrl(url)};
    request.setHeader(QNetworkRequest::UserAgentHeader, user_agent.toUtf8());
    QNetworkReply *reply = manager.get(request);

    connect(reply, &QNetworkReply::finished, this, [this, reply, url] {
        if (reply->error() == QNetworkReply::NoError)
        {
            QImage image = QImage::fromData(reply->readAll()).convertToFormat(QImage::Format_RGBA8888);
            if (!image.isNull())
            {
                std::vector<uint8_t> rgba(image.constBits(), image.constBits() + image.sizeInBytes());
                client->enqueueBadgeImage(url.toStdString(), std::move(rgba), image.width(), image.height());
            }
        }
        reply->deleteLater();
    });
}
