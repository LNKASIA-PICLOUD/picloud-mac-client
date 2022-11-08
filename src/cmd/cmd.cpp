/*
 * Copyright (C) by Olivier Goffart <ogoffart@owncloud.com>
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 * Copyright (C) by Daniel Heule <daniel.heule@gmail.com>
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
#include "httpcredentialstext.h"
#include "netrcparser.h"

#include "account.h"
#include "common/syncjournaldb.h"
#include "common/version.h"
#include "configfile.h" // ONLY ACCESS THE STATIC FUNCTIONS!
#include "csync_exclude.h"
#include "libsync/logger.h"
#include "libsync/theme.h"
#include "networkjobs/checkserverjobfactory.h"
#include "networkjobs/jsonjob.h"
#include "syncengine.h"

#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDebug>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkProxy>
#include <QStringList>
#include <QUrl>

#include <iostream>
#include <random>


using namespace OCC;

namespace {

struct CmdOptions
{
    QString source_dir;
    QUrl target_url;
    QUrl server_url;

    QString remoteFolder;
    QString config_directory;
    QString user;
    QString password;
    QString proxy;
    bool silent = false;
    bool trustSSL = false;
    bool useNetrc = false;
    bool interactive = true;
    bool ignoreHiddenFiles = true;
    QString exclude;
    QString unsyncedfolders;
    int restartTimes = 3;
    int downlimit = 0;
    int uplimit = 0;
};

struct SyncCTX
{
    explicit SyncCTX(const CmdOptions &cmdOptions)
        : options { cmdOptions }
    {
    }
    CmdOptions options;
    AccountPtr account;
    QString user;
};

/* If the selective sync list is different from before, we need to disable the read from db
  (The normal client does it in SelectiveSyncDialog::accept*)
 */
void selectiveSyncFixup(OCC::SyncJournalDb *journal, const QStringList &newList)
{
    SqlDatabase db;
    if (!db.openOrCreateReadWrite(journal->databaseFilePath())) {
        return;
    }

    bool ok;

    auto oldBlackListSet = journal->getSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, &ok).toSet();
    if (ok) {
        auto blackListSet = newList.toSet();
        const auto changes = (oldBlackListSet - blackListSet) + (blackListSet - oldBlackListSet);
        for (const auto &it : changes) {
            journal->schedulePathForRemoteDiscovery(it);
        }

        journal->setSelectiveSyncList(SyncJournalDb::SelectiveSyncBlackList, newList);
    }
}


void sync(const SyncCTX &ctx)
{
    QStringList selectiveSyncList;
    if (!ctx.options.unsyncedfolders.isEmpty()) {
        QFile f(ctx.options.unsyncedfolders);
        if (!f.open(QFile::ReadOnly)) {
            qCritical() << "Could not open file containing the list of unsynced folders: " << ctx.options.unsyncedfolders;
        } else {
            // filter out empty lines and comments
            selectiveSyncList = QString::fromUtf8(f.readAll()).split(QLatin1Char('\n')).filter(QRegExp(QStringLiteral("\\S+"))).filter(QRegExp(QStringLiteral("^[^#]")));

            for (int i = 0; i < selectiveSyncList.count(); ++i) {
                if (!selectiveSyncList.at(i).endsWith(QLatin1Char('/'))) {
                    selectiveSyncList[i].append(QLatin1Char('/'));
                }
            }
        }
    }

    const QString dbPath = ctx.options.source_dir + SyncJournalDb::makeDbName(ctx.options.source_dir);
    auto db = new SyncJournalDb(dbPath, qApp);
    if (!selectiveSyncList.empty()) {
        selectiveSyncFixup(db, selectiveSyncList);
    }

    SyncOptions opt { QSharedPointer<Vfs>(createVfsFromPlugin(Vfs::Off).release()) };
    opt.fillFromEnvironmentVariables();
    opt.verifyChunkSizes();
    auto engine = new SyncEngine(
        ctx.account, ctx.options.target_url, ctx.options.source_dir, ctx.options.remoteFolder, db);
    engine->setSyncOptions(opt);
    engine->setParent(db);

    QObject::connect(engine, &SyncEngine::finished, engine, [engine, ctx, restartCount = std::make_shared<int>(0)](bool result) {
        if (!result) {
            qWarning() << "Failed to sync";
            qApp->exit(EXIT_FAILURE);
        } else {
            if (engine->isAnotherSyncNeeded() != NoFollowUpSync) {
                if (*restartCount < ctx.options.restartTimes) {
                    (*restartCount)++;
                    qDebug() << "Restarting Sync, because another sync is needed" << *restartCount;
                    engine->startSync();
                    return;
                }
                qWarning() << "Another sync is needed, but not done because restart count is exceeded" << *restartCount;
            } else {
                qApp->quit();
            }
        }
    });
    QObject::connect(engine, &SyncEngine::aboutToRemoveAllFiles, engine, [ctx](OCC::SyncFileItem::Direction dir, const std::function<void(bool)> &abort) {
        if (!ctx.options.interactive) {
            abort(false);
        } else {
            if (dir == SyncFileItem::Down) {
                std::cout << "All files in the sync folder '" << qPrintable(ctx.options.remoteFolder) << "' folder were deleted on the server.\n"
                          << "These deletes will be synchronized to your local sync folder, making such files "
                          << "unavailable unless you have a right to restore. \n"
                          << "If you decide to keep the files, they will be re-synced with the server if you have rights to do so.\n"
                          << "If you decide to delete the files, they will be unavailable to you, unless you are the owner."
                          << std::endl;


            } else {
                std::cout << "All the files in your local sync folder '" << qPrintable(ctx.options.source_dir) << "' were deleted. These deletes will be "
                          << "synchronized with your server, making such files unavailable unless restored.\n"
                          << "Are you sure you want to sync those actions with the server?\n"
                          << "If this was an accident and you decide to keep your files, they will be re-synced from the server."
                          << std::endl;
            }
            std::string s;
            while (true) {
                std::cout << "Remove all files?[y,n]";
                std::getline(std::cin, s);
                if (s == "y") {
                    abort(false);
                } else if (s == "n") {
                    abort(true);
                } else {
                    continue;
                }
                return;
            }
        }
    });
    QObject::connect(engine, &SyncEngine::syncError, engine,
        [](const QString &error) { qWarning() << "Sync error:" << error; });
    engine->setIgnoreHiddenFiles(ctx.options.ignoreHiddenFiles);
    engine->setNetworkLimits(ctx.options.uplimit, ctx.options.downlimit);


    // Exclude lists

    bool hasUserExcludeFile = !ctx.options.exclude.isEmpty();
    QString systemExcludeFile = ConfigFile::excludeFileFromSystem();

    // Always try to load the user-provided exclude list if one is specified
    if (hasUserExcludeFile) {
        engine->excludedFiles().addExcludeFilePath(ctx.options.exclude);
    }
    // Load the system list if available, or if there's no user-provided list
    if (!hasUserExcludeFile || QFile::exists(systemExcludeFile)) {
        engine->excludedFiles().addExcludeFilePath(systemExcludeFile);
    }

    if (!engine->excludedFiles().reloadExcludeFiles()) {
        qFatal("Cannot load system exclude list or list supplied via --exclude");
    }
    engine->startSync();
}

void setupCredentials(SyncCTX &ctx)
{
    // Order of retrieval attempt (later attempts override earlier ones):
    // 1. From URL
    // 2. From options
    // 3. From netrc (if enabled)
    // 4. From prompt (if interactive)

    const auto &url = ctx.options.target_url;
    ctx.user = url.userName();
    QString password = url.password();

    if (!ctx.options.user.isEmpty()) {
        ctx.user = ctx.options.user;
    }

    if (!ctx.options.password.isEmpty()) {
        password = ctx.options.password;
    }

    if (ctx.options.useNetrc) {
        NetrcParser parser;
        if (parser.parse()) {
            NetrcParser::LoginPair pair = parser.find(url.host());
            ctx.user = pair.first;
            password = pair.second;
        }
    }

    if (!ctx.options.proxy.isNull()) {
        QString host;
        uint32_t port = 0;
        bool ok;

        QStringList pList = ctx.options.proxy.split(QLatin1Char(':'));
        if (pList.count() == 3) {
            // http: //192.168.178.23 : 8080
            //  0            1            2
            host = pList.at(1);
            if (host.startsWith(QLatin1String("//")))
                host.remove(0, 2);

            port = pList.at(2).toUInt(&ok);
            if (!ok || port > std::numeric_limits<uint16_t>::max()) {
                qFatal("Invalid port number");
            }

            QNetworkProxyFactory::setUseSystemConfiguration(false);
            QNetworkProxy::setApplicationProxy(QNetworkProxy(QNetworkProxy::HttpProxy, host, static_cast<uint16_t>(port)));
        } else {
            qFatal("Could not read httpproxy. The proxy should have the format \"http://hostname:port\".");
        }
    }

    // Pre-flight check: verify that the file specified by --unsyncedfolders can be read by us:
    if (!ctx.options.unsyncedfolders.isNull()) { // yes, isNull and not isEmpty because...:
        // ... if the user entered "--unsyncedfolders ''" on the command-line, opening that will
        // also fail
        QFile f(ctx.options.unsyncedfolders);
        if (!f.open(QFile::ReadOnly)) {
            qFatal("Cannot read unsyncedfolders file '%s': %s",
                qPrintable(ctx.options.unsyncedfolders),
                qPrintable(f.errorString()));
        }
        f.close();
    }

    ctx.account->setCredentials(HttpCredentialsText::create(ctx.options.interactive, ctx.user, password));
    if (ctx.options.trustSSL) {
        QObject::connect(ctx.account->accessManager(), &QNetworkAccessManager::sslErrors, [](QNetworkReply *reply, const QList<QSslError> &errors) {
            reply->ignoreSslErrors(errors);
        });
    } else {
        QObject::connect(ctx.account->accessManager(), &QNetworkAccessManager::sslErrors, [](QNetworkReply *reply, const QList<QSslError> &errors) {
            Q_UNUSED(reply)

            qCritical() << "SSL error encountered";
            for (const auto &e : errors) {
                qCritical() << e.errorString();
            }
            qFatal("If you trust the certificate and want to ignore the errors, use the --trust option.");
        });
    }
}
}

CmdOptions parseOptions(const QStringList &app_args)
{
    CmdOptions options;
    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("%1 version %2 - command line client tool").arg(QCoreApplication::instance()->applicationName(), OCC::Version::displayString()));

    // this little snippet saves a few lines below
    auto addOption = [&parser](const QCommandLineOption &option) {
        parser.addOption(option);
        return option;
    };

    auto silentOption = addOption({ { QStringLiteral("s"), QStringLiteral("silten") }, QStringLiteral("Don't be so verbose.") });
    auto httpproxyOption = addOption({ { QStringLiteral("httpproxy") }, QStringLiteral("Specify a http proxy to use."), QStringLiteral("http://server:port") });
    auto trustOption = addOption({ { QStringLiteral("trust") }, QStringLiteral("Trust the SSL certification") });
    auto excludeOption = addOption({ { QStringLiteral("exclude") }, QStringLiteral("Path to an exclude list [file]"), QStringLiteral("file") });
    auto unsyncedfoldersOption = addOption({ { QStringLiteral("unsyncedfolders") }, QStringLiteral("File containing the list of unsynced remote folders (selective sync)"), QStringLiteral("file") });

    auto serverOption = addOption({ { QStringLiteral("server") }, QStringLiteral("Use [url] as the location of the server. OCIS only (server location and spaces url can differ)"), QStringLiteral("url") });
    auto userOption = addOption({ { QStringLiteral("u"), QStringLiteral("user") }, QStringLiteral("Use [name] as the login name"), QStringLiteral("name") });
    auto passwordOption = addOption({ { QStringLiteral("p"), QStringLiteral("password") }, QStringLiteral("Use [pass] as password"), QStringLiteral("password") });
    auto useNetrcOption = addOption({ { QStringLiteral("n") }, QStringLiteral("Use netrc (5) for login") });

    auto nonInterActiveOption = addOption({ { QStringLiteral("non-interactive") }, QStringLiteral("Do not block execution with interaction") });
    auto maxRetriesOption = addOption({ { QStringLiteral("max-sync-retries") }, QStringLiteral("Retries maximum n times (default to 3)"), QStringLiteral("n") });
    auto uploadLimitOption = addOption({ { QStringLiteral("uplimit") }, QStringLiteral("Limit the upload speed of files to n KB/s"), QStringLiteral("n") });
    auto downloadLimitption = addOption({ { QStringLiteral("downlimit") }, QStringLiteral("Limit the download speed of files to n KB/s"), QStringLiteral("n") });

    auto logdebugOption = addOption({ { QStringLiteral("logdebug") }, QStringLiteral("More verbose logging") });

    parser.addHelpOption();
    parser.addVersionOption();

    parser.addPositionalArgument(QStringLiteral("source_dir"), QStringLiteral("The source dir"));
    parser.addPositionalArgument(QStringLiteral("server_url"), QStringLiteral("The url to the server"));
    parser.addPositionalArgument(QStringLiteral("remote_folder"), QStringLiteral("A remote folder"));

    parser.process(app_args);


    const QStringList args = parser.positionalArguments();
    if (args.size() < 2 || args.size() > 3) {
        parser.showHelp();
        exit(1);
    }

    options.source_dir = [arg = args[0]] {
        QFileInfo fi(arg);
        if (!fi.exists()) {
            std::cerr << "Source dir '" << qPrintable(arg) << "' does not exist." << std::endl;
            exit(1);
        }
        QString sourceDir = fi.absoluteFilePath();
        if (!sourceDir.endsWith(QLatin1Char('/'))) {
            sourceDir.append(QLatin1Char('/'));
        }
        return sourceDir;
    }();
    options.target_url = QUrl::fromUserInput(args[1]);
    if (args.size() == 3) {
        options.remoteFolder = args[2];
    }

    if (parser.isSet(httpproxyOption)) {
        options.proxy = parser.value(httpproxyOption);
    }
    if (parser.isSet(silentOption)) {
        options.silent = true;
    }
    if (parser.isSet(trustOption)) {
        options.trustSSL = true;
    }
    if (parser.isSet(useNetrcOption)) {
        options.useNetrc = true;
    }
    if (parser.isSet(nonInterActiveOption)) {
        options.interactive = false;
    }
    if (parser.isSet(serverOption)) {
        options.server_url = QUrl::fromUserInput(parser.value(serverOption));
    }
    if (parser.isSet(userOption)) {
        options.user = parser.value(userOption);
    }
    if (parser.isSet(passwordOption)) {
        options.password = parser.value(passwordOption);
    }
    if (parser.isSet(excludeOption)) {
        options.exclude = parser.value(excludeOption);
    }
    if (parser.isSet(unsyncedfoldersOption)) {
        options.unsyncedfolders = parser.value(unsyncedfoldersOption);
    }
    if (parser.isSet(maxRetriesOption)) {
        options.restartTimes = parser.value(maxRetriesOption).toInt();
    }
    if (parser.isSet(uploadLimitOption)) {
        options.uplimit = parser.value(maxRetriesOption).toInt() * 1000;
    }
    if (parser.isSet(downloadLimitption)) {
        options.downlimit = parser.value(downloadLimitption).toInt() * 1000;
    }
    if (parser.isSet(logdebugOption)) {
        Logger::instance()->setLogFile(QStringLiteral("-"));
        Logger::instance()->setLogDebug(true);
    }
    return options;
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    app.setApplicationVersion(Theme::instance()->versionSwitchOutput());

#ifdef Q_OS_WIN
    // Ensure OpenSSL config file is only loaded from app directory
    QString opensslConf = QCoreApplication::applicationDirPath() + QStringLiteral("/openssl.cnf");
    qputenv("OPENSSL_CONF", opensslConf.toLocal8Bit());
#endif
    SyncCTX ctx { parseOptions(app.arguments()) };

    if (ctx.options.silent) {
        qInstallMessageHandler([](QtMsgType, const QMessageLogContext &, const QString &) {});
    } else {
        qSetMessagePattern(Logger::loggerPattern());
    }

    ctx.account = Account::create();

    if (!ctx.account) {
        qFatal("Could not initialize account!");
    }

    setupCredentials(ctx);

    if (ctx.options.server_url.isEmpty()) {
        ctx.options.server_url = ctx.options.target_url;
        // guess dav path
        if (!ctx.options.target_url.path().contains(ctx.account->davPath())) {
            ctx.options.target_url = OCC::Utility::concatUrlPath(ctx.options.target_url, ctx.account->davPath());
        }
    }

    // don't leak credentials more than needed
    ctx.options.server_url = ctx.options.server_url.adjusted(QUrl::RemoveUserInfo);
    ctx.options.target_url = ctx.options.target_url.adjusted(QUrl::RemoveUserInfo);

    const QUrl baseUrl = [&ctx] {
        auto tmp = ctx.options.server_url;
        // Find the folder and the original owncloud url
        QStringList splitted = tmp.path().split(ctx.account->davPath());
        tmp.setPath(splitted.value(0));
        tmp.setScheme(tmp.scheme().replace(QLatin1String("owncloud"), QLatin1String("http")));
        return tmp;
    }();


    ctx.account->setUrl(baseUrl);

    auto *checkServerJob = CheckServerJobFactory(ctx.account->accessManager()).startJob(ctx.account->url());

    QObject::connect(checkServerJob, &CoreJob::finished, [ctx, checkServerJob] {
        if (checkServerJob->success()) {
            // Perform a call to get the capabilities.
            auto *capabilitiesJob = new JsonApiJob(ctx.account, QStringLiteral("ocs/v1.php/cloud/capabilities"), {}, {}, nullptr);
            QObject::connect(capabilitiesJob, &JsonApiJob::finishedSignal, qApp, [capabilitiesJob, ctx] {
                auto caps = capabilitiesJob->data().value(QStringLiteral("ocs")).toObject().value(QStringLiteral("data")).toObject().value(QStringLiteral("capabilities")).toObject();
                qDebug() << "Server capabilities" << caps;
                ctx.account->setCapabilities(caps.toVariantMap());

                if (ctx.account->serverVersionUnsupported()) {
                    qFatal("Error unsupported server");
                }

                if (capabilitiesJob->reply()->error() != QNetworkReply::NoError) {
                    qFatal("Error connecting to server");
                }

                auto userJob = new JsonApiJob(ctx.account, QStringLiteral("ocs/v1.php/cloud/user"), {}, {}, nullptr);
                QObject::connect(userJob, &JsonApiJob::finishedSignal, qApp, [userJob, ctx] {
                    const QJsonObject data = userJob->data().value(QStringLiteral("ocs")).toObject().value(QStringLiteral("data")).toObject();
                    ctx.account->setDavUser(data.value(QStringLiteral("id")).toString());
                    ctx.account->setDavDisplayName(data.value(QStringLiteral("display-name")).toString());

                    // much lower age than the default since this utility is usually made to be run right after a change in the tests
                    SyncEngine::minimumFileAgeForUpload = std::chrono::milliseconds(0);
                    sync(ctx);
                });
                userJob->start();
            });
            capabilitiesJob->start();
        } else {
            switch (checkServerJob->reply()->error()) {
            case QNetworkReply::OperationCanceledError:
                qFatal("Looking up %s timed out.", qPrintable(ctx.account->url().toString()));
                break;
            default:
                qFatal("Failed to resolve %s Error: %s.", qPrintable(ctx.account->url().toString()), qPrintable(checkServerJob->reply()->errorString()));
            }
        }
    });

    return app.exec();
}
