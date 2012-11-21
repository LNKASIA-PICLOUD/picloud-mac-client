/*
 * Copyright (C) by Dominik Schmidt <dev@dominik-schmidt.de>
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


#include "socketapi.h"

#include "../mirallconfigfile.h"
#include "../folderman.h"
#include "../folder.h"
#include "../owncloudfolder.h"

#include <attica/providermanager.h>

#include <QDebug>
#include <QUrl>
#include <QLocalSocket>
#include <QLocalServer>
#include <QMetaObject>
#include <QStringList>
#include <QFile>
#include <QDir>
#include <QApplication>
#include <QClipboard>

using namespace Mirall;

#define DEBUG qDebug() << "SocketApi: "

SocketApi::SocketApi(QObject* parent, const QUrl& localFile, FolderMan* folderMan)
    : QObject(parent)
    , _localServer(0)
    , _folderMan(folderMan)
{
    QString socketPath = localFile.toLocalFile();
    DEBUG << "ctor: " << socketPath;

    // setup socket
    _localServer = new QLocalServer(this);
    QLocalServer::removeServer(socketPath);
    if(!_localServer->listen(socketPath))
        DEBUG << "can't start server";
    else
        DEBUG << "server started";
    connect(_localServer, SIGNAL(newConnection()), this, SLOT(onNewConnection()));

    // folder watcher
    connect(_folderMan, SIGNAL(folderSyncStateChange(QString)), SLOT(onSyncStateChanged(QString)));

    // setup attica
    QString tmp(QLatin1String("%1ocs/providers.php"));
    QUrl providerFile(tmp.arg(MirallConfigFile().ownCloudUrl()));

    _atticaManager = new Attica::ProviderManager();
    DEBUG << "ctor: Add provider file: " << providerFile;
    _atticaManager->addProviderFile(providerFile);
    connect(_atticaManager, SIGNAL(providerAdded(Attica::Provider)), SLOT(onProviderAdded(Attica::Provider)));
}

SocketApi::~SocketApi()
{
    DEBUG << "dtor";
    _localServer->close();
}

void SocketApi::onNewConnection()
{
    QLocalSocket* socket = _localServer->nextPendingConnection();
    DEBUG << "New connection " << socket;
    connect(socket, SIGNAL(readyRead()), this, SLOT(onReadyRead()));
    connect(socket, SIGNAL(disconnected()), this, SLOT(onLostConnection()));
    Q_ASSERT(socket->readAll().isEmpty());

    _listeners.append(socket);
}

void SocketApi::onLostConnection()
{
    DEBUG << "Lost connection " << sender();

    QLocalSocket* socket = qobject_cast< QLocalSocket* >(sender());
    _listeners.removeAll(socket);
}


void SocketApi::onReadyRead()
{
    QLocalSocket* socket = qobject_cast<QLocalSocket*>(sender());
    Q_ASSERT(socket);

    while(socket->canReadLine())
    {
        QString line = QString(socket->readLine()).trimmed();
        QString command = line.split(":").first();
        QString function = QString(QLatin1String("command_")).append(command);

        QString functionWithArguments = function + QLatin1String("(QString,QLocalSocket*)");
        int indexOfMethod = this->metaObject()->indexOfMethod(functionWithArguments.toAscii());

        QString argument = line.remove(0, command.length()+1).trimmed();
        if(indexOfMethod != -1)
        {
            QMetaObject::invokeMethod(this, function.toAscii(), Q_ARG(QString, argument), Q_ARG(QLocalSocket*, socket));
        }
        else
        {
            DEBUG << "The command is not supported by this version of the client:" << command << "with argument:" << argument;
        }
    }
}

void SocketApi::onSyncStateChanged(const QString&)
{
    DEBUG << "Sync state changed";

    broadcastMessage("UPDATE_VIEW");
}


void SocketApi::sendMessage(QLocalSocket* socket, const QString& message)
{
    DEBUG << "Sending message: " << message;
    QString localMessage = message;
    socket->write(localMessage.append("\n").toUtf8());
}

void SocketApi::broadcastMessage(const QString& message)
{
    DEBUG << "Broadcasting to" << _listeners.count() << "listeners: " << message;
    foreach(QLocalSocket* current, _listeners)
    {
        sendMessage(current, message);
    }
}

void SocketApi::command_RETRIEVE_FOLDER_STATUS(const QString& argument, QLocalSocket* socket)
{
    qDebug() << Q_FUNC_INFO << argument;
    //TODO: do security checks?!
    ownCloudFolder* folder = qobject_cast< ownCloudFolder* >(_folderMan->folderForPath( argument ));
    // this can happen in offline mode e.g.: nothing to worry about
    if(!folder)
    {
        DEBUG << "folder offline or not watched:" << argument;
        return;
    }

    QDir dir(argument);
    foreach(QString entry, dir.entryList())
    {
        QString absoluteFilePath = dir.absoluteFilePath(entry);
        QString statusString;
        SyncFileStatus fileStatus = folder->fileStatus(absoluteFilePath);
        switch(fileStatus)
        {
            case STATUS_NONE:
                statusString = QLatin1String("STATUS_NONE");
                break;
            case STATUS_EVAL:
                statusString = QLatin1String("STATUS_EVAL");
                break;
            case STATUS_REMOVE:
                statusString = QLatin1String("STATUS_REMOVE");
                break;
            case STATUS_RENAME:
                statusString = QLatin1String("STATUS_RENAME");
                break;
            case STATUS_NEW:
                statusString = QLatin1String("STATUS_NEW");
                break;
            case STATUS_CONFLICT:
                statusString = QLatin1String("STATUS_CONFLICT");
                break;
            case STATUS_IGNORE:
                statusString = QLatin1String("STATUS_IGNORE");
                break;
            case STATUS_SYNC:
                statusString = QLatin1String("STATUS_SYNC");
                break;
            case STATUS_STAT_ERROR:
                statusString = QLatin1String("STATUS_STAT_ERROR");
                break;
            case STATUS_ERROR:
                statusString = QLatin1String("STATUS_ERROR");
                break;
            case STATUS_UPDATED:
                statusString = QLatin1String("STATUS_UPDATED");
                break;
            default:
                qWarning() << "not all SyncFileStatus items checked!";
                Q_ASSERT(false);
                statusString = QLatin1String("STATUS_NONE");

        }
        QString message("%1:%2:%3");
        message = message.arg("STATUS").arg(statusString).arg(absoluteFilePath);
        sendMessage(socket, message);
    }
}

void SocketApi::command_PUBLIC_SHARE_LINK(const QString& argument, QLocalSocket* socket)
{
    DEBUG << "copy public share link for" << argument;

    if(!_atticaProvider.isEnabled()) {
        qWarning() << "Attica Provider is not enabled!";
        return;
    }


    ownCloudFolder* folder = qobject_cast< ownCloudFolder* >(_folderMan->folderForPath( argument ));
    if(!folder)
        return;

    int lastChars = argument.length()-folder->path().length();
    QString remotePath;
    remotePath = argument.right(lastChars);
    remotePath.prepend("/");
    remotePath.prepend(folder->secondPath());

    qDebug() << "remote path: " << remotePath;
    _remotePath = remotePath;

    Attica::ItemPostJob<Attica::Link>* job = _atticaProvider.requestPublicShareLink(_remotePath);
    connect(job, SIGNAL(finished(Attica::BaseJob*)), SLOT(onGotPublicShareLink(Attica::BaseJob*)));
    job->start();
}

void SocketApi::onProviderAdded(const Attica::Provider& provider)
{
    qDebug() << Q_FUNC_INFO << provider.name() << provider.baseUrl() << provider.isValid() << provider.isEnabled();
    _atticaProvider = provider;

    MirallConfigFile config;
    _atticaProvider.saveCredentials(config.ownCloudUser(), config.ownCloudPasswd());
}

void SocketApi::onGotPublicShareLink(Attica::BaseJob* job)
{
    Attica::ItemPostJob<Attica::Link>* itemJob = static_cast<Attica::ItemPostJob<Attica::Link>*>(job);
    QString publicLink = itemJob->result().url().toString();

    if(!publicLink.isEmpty()) {
        QClipboard *clipboard = QApplication::clipboard();
        clipboard->setText(publicLink);
        DEBUG << "copied " << publicLink << "to clipboard";
    } else {
        DEBUG << "public link empty, not copying to clipboard";
    }
}