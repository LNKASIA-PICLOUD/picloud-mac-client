/*
 * Copyright (C) by Hannah von Reth <hannah.vonreth@owncloud.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#include "fetchserversettings.h"

#include "gui/accountstate.h"
#include "gui/connectionvalidator.h"

#include "libsync/networkjobs/jsonjob.h"

using namespace std::chrono_literals;

using namespace OCC;


Q_LOGGING_CATEGORY(lcfetchserversettings, "sync.fetchserversettings", QtInfoMsg)

namespace {
constexpr auto timeoutC = 20s;
}

// TODO: move to libsync?
FetchServerSettingsJob::FetchServerSettingsJob(const OCC::AccountPtr &account, QObject *parent)
    : QObject(parent)
    , _account(account)
{
}


void FetchServerSettingsJob::start()
{
    // The main flow now needs the capabilities
    auto *job = new JsonApiJob(_account, QStringLiteral("ocs/v2.php/cloud/capabilities"), {}, {}, this);
    job->setAuthenticationJob(isAuthJob());
    job->setTimeout(timeoutC);

    QObject::connect(job, &JsonApiJob::finishedSignal, this, [job, this] {
        auto caps =
            job->data().value(QStringLiteral("ocs")).toObject().value(QStringLiteral("data")).toObject().value(QStringLiteral("capabilities")).toObject();
        qCInfo(lcfetchserversettings) << "Server capabilities" << caps;
        if (job->ocsSuccess()) {
            // Record that the server supports HTTP/2
            // Actual decision if we should use HTTP/2 is done in AccessManager::createRequest
            // TODO: http2 support is currently disabled in the client code
            if (auto reply = job->reply()) {
                _account->setHttp2Supported(reply->attribute(QNetworkRequest::Http2WasUsedAttribute).toBool());
            }
            _account->setCapabilities(caps.toVariantMap());
            if (checkServerInfo()) {
                auto *userJob = new JsonApiJob(_account, QStringLiteral("ocs/v2.php/cloud/user"), SimpleNetworkJob::UrlQuery{}, QNetworkRequest{}, this);
                job->setAuthenticationJob(isAuthJob());
                job->setTimeout(timeoutC);
                QObject::connect(userJob, &JsonApiJob::finishedSignal, this, [userJob, this] {
                    const auto userData = userJob->data().value(QStringLiteral("ocs")).toObject().value(QStringLiteral("data")).toObject();
                    const QString user = userData.value(QStringLiteral("id")).toString();
                    if (!user.isEmpty()) {
                        _account->setDavUser(user);
                    }
                    const QString displayName = userData.value(QStringLiteral("display-name")).toString();
                    if (!displayName.isEmpty()) {
                        _account->setDavDisplayName(displayName);
                    }
                    runAsyncUpdates();
                    Q_EMIT finishedSignal();
                });
                userJob->start();
            }
        }
    });
    job->start();
}

void FetchServerSettingsJob::runAsyncUpdates()
{
    // those jobs are:
    // - never auth jobs
    // - might get queued
    // - have the default timeout
    // - must not be parented by this object

    // ideally we would parent them to the account, but as things are messed up by the shared pointer stuff we can't at the moment
    // so we just set them free
    if (_account->capabilities().avatarsAvailable()) {
        auto *avatarJob = new AvatarJob(_account, _account->davUser(), 128, nullptr);
        connect(avatarJob, &AvatarJob::avatarPixmap, this, [this](const QPixmap &img) { _account->setAvatar(img); });
        avatarJob->start();
    };

    if (_account->capabilities().appProviders().enabled) {
        auto *jsonJob = new JsonJob(_account, _account->url(), _account->capabilities().appProviders().appsUrl, "GET");
        connect(jsonJob, &JsonJob::finishedSignal, this, [jsonJob, this] { _account->setAppProvider(AppProvider{jsonJob->data()}); });
        jsonJob->start();
    }
}
bool FetchServerSettingsJob::checkServerInfo()
{
    // We cannot deal with servers < 10.0.0
    switch (_account->serverSupportLevel()) {
    case Account::ServerSupportLevel::Supported:
        break;
    case Account::ServerSupportLevel::Unknown:
        Q_EMIT unknownServerDetected();
        break;
    case Account::ServerSupportLevel::Unsupported:
        Q_EMIT unsupportedServerDetected();
        return false;
    }
    return true;
}

bool FetchServerSettingsJob::isAuthJob() const
{
    return qobject_cast<ConnectionValidator *>(parent());
}
