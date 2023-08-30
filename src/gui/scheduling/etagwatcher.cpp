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

#include "gui/scheduling/etagwatcher.h"

#include "accountstate.h"
#include "gui/folderman.h"
#include "libsync/configfile.h"
#include "libsync/graphapi/spacesmanager.h"
#include "libsync/syncengine.h"


using namespace std::chrono_literals;

using namespace OCC;

namespace {
constexpr auto pollTimeoutC = 30s;
}

ETagWatcher::ETagWatcher(FolderMan *folderMan, QObject *parent)
    : QObject(parent)
    , _folderMan(folderMan)
{
    connect(folderMan, &FolderMan::folderListChanged, this, [this] {
        decltype(_lastEtagJob) intersection;
        for (auto *f : _folderMan->folders()) {
            if (f->isReady()) {
                auto it = _lastEtagJob.find(f);
                if (it != _lastEtagJob.cend()) {
                    intersection[f] = std::move(it->second);
                } else {
                    intersection[f] = {};
                    connect(&f->syncEngine(), &SyncEngine::rootEtag, this, [f, this](const QString &etag, const QDateTime &time) {
                        auto &info = _lastEtagJob[f];
                        info.etag = etag;
                        info.lastUpdate.reset();
                        f->accountState()->tagLastSuccessfullETagRequest(time);
                    });
                }
            }
        }
        _lastEtagJob = std::move(intersection);
    });

    auto *pollTimer = new QTimer(this);
    pollTimer->setInterval(pollTimeoutC);
    // check wheter we need to query the etag for oc10 servers
    connect(pollTimer, &QTimer::timeout, this, [this] {
        for (auto &info : _lastEtagJob) {
            // for spaces we use the etag provided by the SpaceManager
            if (info.first->accountState()->supportsSpaces()) {
                // we could also connect to the spaceChanged signal but for now this will keep it closer to oc10
                // ensure we already know about the space (startup)
                if (auto *space = info.first->accountState()->account()->spacesManager()->space(info.first->spaceId())) {
                    updateEtag(info.first, space->drive().getRoot().getETag());
                }
            } else {
                startOC10EtagJob(info.first);
            }
        }
    });
    pollTimer->start();
}

void ETagWatcher::updateEtag(Folder *f, const QString &etag)
{
    // the server must provide a valid etag but there might be bugs
    // https://github.com/owncloud/ocis/issues/7160
    if (OC_ENSURE(etag.isEmpty())) {
        auto &info = _lastEtagJob[f];
        if (info.etag != etag) {
            info.etag = etag;
            _folderMan->scheduler()->enqueueFolder(f);
        }
        info.lastUpdate.reset();
    }
}

void ETagWatcher::startOC10EtagJob(Folder *f)
{
    if (f->accountState()->state() == AccountState::State::Connected) {
        ConfigFile cfg;
        const auto account = f->accountState()->account();
        const auto polltime = cfg.remotePollInterval(account->capabilities().remotePollInterval());
        if (_lastEtagJob[f].lastUpdate.duration() > polltime) {
            auto *requestEtagJob = new RequestEtagJob(account, f->webDavUrl(), f->remotePath(), f);
            requestEtagJob->setTimeout(pollTimeoutC);
            connect(requestEtagJob, &RequestEtagJob::finishedSignal, this, [requestEtagJob, f, this] {
                if (requestEtagJob->httpStatusCode() == 207) {
                    updateEtag(f, requestEtagJob->etag());
                    f->accountState()->tagLastSuccessfullETagRequest(requestEtagJob->responseQTimeStamp());
                }
            });
            requestEtagJob->start();
        }
    }
}
