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

#include "spacesmanager.h"

#include "libsync/account.h"
#include "libsync/creds/abstractcredentials.h"
#include "libsync/graphapi/jobs/drives.h"


#include <QTimer>

#include <chrono>

using namespace std::chrono_literals;

using namespace OCC;
using namespace GraphApi;

namespace {
constexpr auto refreshTimeoutC = 30s;
}

SpacesManager::SpacesManager(Account *parent)
    : QObject(parent)
    , _account(parent)
    , _refreshTimer(new QTimer(this))
{
    _refreshTimer->setInterval(refreshTimeoutC);
    // the timer will be restarted once we received drives data
    _refreshTimer->setSingleShot(true);

    connect(_refreshTimer, &QTimer::timeout, this, &SpacesManager::refresh);
    connect(_account, &Account::credentialsFetched, this, &SpacesManager::refresh);
    if (_account->accessManager()) {
        // If we don't have a nam yet, Account::credentialsFetched will can refresh
        refresh();
    }
}

void SpacesManager::refresh()
{
    auto drivesJob = new Drives(_account->sharedFromThis(), this);
    connect(drivesJob, &Drives::finishedSignal, this, [drivesJob, this] {
        drivesJob->deleteLater();
        if (drivesJob->httpStatusCode() == 200) {
            auto oldKeys = _spacesMap.keys();
            for (const auto &dr : drivesJob->drives()) {
                auto *space = this->space(dr.getId());
                oldKeys.removeAll(dr.getId());
                if (!space) {
                    space = new Space(this, dr);
                    _spacesMap.insert(dr.getId(), space);
                } else {
                    space->setDrive(dr);
                    Q_EMIT spaceChanged(space);
                }
            }
            for (const QString &id : oldKeys) {
                auto *oldSpace = _spacesMap.take(id);
                oldSpace->deleteLater();
            }
            if (!_ready) {
                _ready = true;
                Q_EMIT ready();
            }
        }
        Q_EMIT updated();
        _refreshTimer->start();
    });
    _refreshTimer->stop();
    drivesJob->start();
}

Space *SpacesManager::space(const QString &id) const
{
    return _spacesMap.value(id);
}

Space *SpacesManager::spaceByUrl(const QUrl &url) const
{
    auto it = std::find_if(_spacesMap.cbegin(), _spacesMap.cend(),
        [url](const auto *space) { return OCC::Utility::urlEqual(QUrl(space->drive().getRoot().getWebDavUrl()), url); });
    if (it != _spacesMap.cend()) {
        return *it;
    }
    return {};
}

Account *SpacesManager::account() const
{
    return _account;
}

QVector<Space *> SpacesManager::spaces() const
{
    return {_spacesMap.begin(), _spacesMap.end()};
}

void SpacesManager::checkReady() const
{
    // see constructor for calls to refresh
    if (_ready) {
        Q_EMIT ready();
    }
}
