/*
 * Copyright (C) by Duncan Mac-Vicar P. <duncan@kde.org>
 * Copyright (C) by Klaas Freitag <freitag@owncloud.com>
 * Copyright (C) by Daniel Molkentin <danimo@owncloud.com>
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

#include "application.h"

#include <random>

#include "account.h"
#include "accountmanager.h"
#include "accountstate.h"
#include "clientproxy.h"
#include "common/asserts.h"
#include "common/version.h"
#include "common/vfs.h"
#include "configfile.h"
#include "connectionvalidator.h"
#include "csync_exclude.h"
#include "folder.h"
#include "folderman.h"
#include "settingsdialog.h"
#include "sharedialog.h"
#include "socketapi/socketapi.h"
#include "theme.h"
#include "translations.h"

#ifdef WITH_AUTO_UPDATER
#include "updater/ocupdater.h"
#endif

#include <QApplication>
#include <QDesktopServices>
#include <QDir>
#include <QFileOpenEvent>
#include <QLibraryInfo>
#include <QMessageBox>
#include <QPushButton>
#include <QTranslator>

#if QT_VERSION >= QT_VERSION_CHECK(6, 3, 0)
#include <QNetworkInformation>
#endif

namespace OCC {

Q_LOGGING_CATEGORY(lcApplication, "gui.application", QtInfoMsg)

QString Application::displayLanguage() const
{
    return _displayLanguage;
}

ownCloudGui *Application::gui() const
{
    return _gui;
}

Application *Application::_instance = nullptr;

Application::Application(Platform *platform, bool debugMode, QObject *parent)
    : QObject(parent)
    , _debugMode(debugMode)
{
    Q_ASSERT(!_instance);
    _instance = this;

    platform->migrate();

    setupTranslations();

    qCInfo(lcApplication) << "Plugin search paths:" << qApp->libraryPaths();

    // Check vfs plugins
    if (Theme::instance()->showVirtualFilesOption() && VfsPluginManager::instance().bestAvailableVfsMode() == Vfs::Off) {
        qCWarning(lcApplication) << "Theme wants to show vfs mode, but no vfs plugins are available";
    }
    if (VfsPluginManager::instance().isVfsPluginAvailable(Vfs::WindowsCfApi))
        qCInfo(lcApplication) << "VFS windows plugin is available";
    if (VfsPluginManager::instance().isVfsPluginAvailable(Vfs::WithSuffix))
        qCInfo(lcApplication) << "VFS suffix plugin is available";

    ConfigFile cfg;

    // this should be called once during application startup to make sure we don't miss any messages
    cfg.configureHttpLogging();

    // The timeout is initialized with an environment variable, if not, override with the value from the config
    if (AbstractNetworkJob::httpTimeout == AbstractNetworkJob::DefaultHttpTimeout) {
        AbstractNetworkJob::httpTimeout = cfg.timeout();
    }

    // Check vfs plugins
    if (Theme::instance()->showVirtualFilesOption() && VfsPluginManager::instance().bestAvailableVfsMode() == Vfs::Off) {
        qCWarning(lcApplication) << "Theme wants to show vfs mode, but no vfs plugins are available";
    }
    if (VfsPluginManager::instance().isVfsPluginAvailable(Vfs::WindowsCfApi))
        qCInfo(lcApplication) << "VFS windows plugin is available";
    if (VfsPluginManager::instance().isVfsPluginAvailable(Vfs::WithSuffix))
        qCInfo(lcApplication) << "VFS suffix plugin is available";

    _folderManager.reset(new FolderMan);

    if (!AccountManager::instance()->restore()) {
        // If there is an error reading the account settings, try again
        // after a couple of seconds, if that fails, give up.
        // (non-existence is not an error)
        Utility::sleep(5);
        if (!AccountManager::instance()->restore()) {
            qCCritical(lcApplication) << "Could not read the account settings, quitting";
            QMessageBox::critical(nullptr, tr("Error accessing the configuration file"),
                tr("There was an error while accessing the configuration "
                   "file at %1.")
                    .arg(ConfigFile::configFile()),
                QMessageBox::Close);
            QTimer::singleShot(0, qApp, &QApplication::quit);
            return;
        }
    }

    FolderMan::instance()->setSyncEnabled(true);

    qApp->setQuitOnLastWindowClosed(false);

    Theme::instance()->setSystrayUseMonoIcons(cfg.monoIcons());
    connect(Theme::instance(), &Theme::systrayUseMonoIconsChanged, this, &Application::slotUseMonoIconsChanged);

    // Setting up the gui class will allow tray notifications for the
    // setup that follows, like folder setup
    _gui = new ownCloudGui(this);

    FolderMan::instance()->setupFolders();

    connect(AccountManager::instance(), &AccountManager::accountAdded, this, &Application::slotAccountStateAdded);
    connect(AccountManager::instance(), &AccountManager::accountRemoved, this, &Application::slotAccountStateRemoved);
    for (const auto &ai : AccountManager::instance()->accounts()) {
        slotAccountStateAdded(ai);
    }

    connect(FolderMan::instance()->socketApi(), &SocketApi::shareCommandReceived, _gui.data(), &ownCloudGui::slotShowShareDialog);

#ifdef WITH_AUTO_UPDATER
    // Update checks
    UpdaterScheduler *updaterScheduler = new UpdaterScheduler(this);
    connect(updaterScheduler, &UpdaterScheduler::updaterAnnouncement, _gui.data(),
        [this](const QString &title, const QString &msg) { _gui->slotShowTrayMessage(title, msg); });
    connect(updaterScheduler, &UpdaterScheduler::requestRestart, _folderManager.data(), &FolderMan::slotScheduleAppRestart);
#endif

    // Cleanup at Quit.
    connect(qApp, &QCoreApplication::aboutToQuit, this, &Application::slotCleanup);
    qApp->installEventFilter(this);

#if QT_VERSION >= QT_VERSION_CHECK(6, 3, 0)
    if (!QNetworkInformation::loadDefaultBackend()) {
        qCWarning(lcApplication) << "Failed to load QNetworkInformation";
    } else {
        connect(QNetworkInformation::instance(), &QNetworkInformation::reachabilityChanged, this,
            [this](QNetworkInformation::Reachability reachability) { qCInfo(lcApplication) << "Connection Status changed to:" << reachability; });
    }
#endif

    if (AccountManager::instance()->accounts().isEmpty()) {
        // display the wizard if we don't have an account yet
        QTimer::singleShot(0, gui(), &ownCloudGui::runNewAccountWizard);
    }
}

Application::~Application()
{
    // Make sure all folders are gone, otherwise removing the
    // accounts will remove the associated folders from the settings.
    if (_folderManager) {
        _folderManager->unloadAndDeleteAllFolders();
    }
}

void Application::slotAccountStateRemoved() const
{
    // if there is no more account, show the wizard.
    if (_gui && AccountManager::instance()->accounts().isEmpty()) {
        // allow to add a new account if there is non any more. Always think
        // about single account theming!
        gui()->runNewAccountWizard();
    }
}

void Application::slotAccountStateAdded(AccountStatePtr accountState) const
{
    // Hook up the GUI slots to the account state's signals:
    connect(accountState.data(), &AccountState::stateChanged,
        _gui.data(), &ownCloudGui::slotAccountStateChanged);
    connect(accountState->account().data(), &Account::serverVersionChanged,
        _gui.data(), [account = accountState->account().data(), this] {
            _gui->slotTrayMessageIfServerUnsupported(account);
        });

    // Hook up the folder manager slots to the account state's signals:
    connect(accountState.data(), &AccountState::stateChanged,
        _folderManager.data(), &FolderMan::slotAccountStateChanged);
    connect(accountState->account().data(), &Account::serverVersionChanged,
        _folderManager.data(), [account = accountState->account().data()] {
            FolderMan::instance()->slotServerVersionChanged(account);
        });
    accountState->checkConnectivity();
}

void Application::slotCleanup()
{
    // unload the ui to make sure we no longer react to signals
    _gui->slotShutdown();
    delete _gui;

    AccountManager::instance()->save();
    FolderMan::instance()->unloadAndDeleteAllFolders();

    // Remove the account from the account manager so it can be deleted.
    AccountManager::instance()->shutdown();
}

void Application::slotCrash()
{
    Utility::crash();
}

void Application::slotCrashEnforce()
{
    OC_ENFORCE(1 == 0);
}


void Application::slotCrashFatal()
{
    qFatal("la Qt fatale");
}

AccountStatePtr Application::addNewAccount(AccountPtr newAccount)
{
    auto *accountMan = AccountManager::instance();

    // first things first: we need to add the new account
    auto accountStatePtr = accountMan->addAccount(newAccount);

    // if one account is configured: enable autostart
    bool shouldSetAutoStart = (accountMan->accounts().size() == 1);
#ifdef Q_OS_MAC
    // Don't auto start when not being 'installed'
    shouldSetAutoStart = shouldSetAutoStart
        && QCoreApplication::applicationDirPath().startsWith(QLatin1String("/Applications/"));
#endif
    if (shouldSetAutoStart) {
        Utility::setLaunchOnStartup(Theme::instance()->appName(), Theme::instance()->appNameGUI(), true);
    }

    // showing the UI to show the user that the account has been added successfully
    _gui->slotShowSettings();

    return accountStatePtr;
}

void Application::slotUseMonoIconsChanged(bool)
{
    _gui->slotComputeOverallSyncStatus();
}

bool Application::debugMode()
{
    return _debugMode;
}

QString substLang(const QString &lang)
{
    // Map the more appropriate script codes
    // to country codes as used by Qt and
    // transifex translation conventions.

    // Simplified Chinese
    if (lang == QLatin1String("zh_Hans"))
        return QStringLiteral("zh_CN");
    // Traditional Chinese
    if (lang == QLatin1String("zh_Hant"))
        return QStringLiteral("zh_TW");
    return lang;
}

void Application::setupTranslations()
{
    const auto trPath = Translations::translationsDirectoryPath();
    qCDebug(lcApplication) << "Translations directory path:" << trPath;

    QStringList uiLanguages = QLocale::system().uiLanguages();
    qCDebug(lcApplication) << "UI languages:" << uiLanguages;

    // the user can also set a locale in the settings, so we need to load the config file
    ConfigFile cfg;

    // we need to track the enforced languages separately, since we need to distinguish between locale-provided
    // and user-enforced ones below
    QSet<QString> enforcedLanguages;

    // note that user-enforced languages are prioritized over the theme enforced one
    // to make testing easier, --language overrides the setting from the config file
    // as we are prepending to the list of languages, the list passed to the loop must be sorted with ascending priority
    for (const auto &enforcedLocale : {cfg.uiLanguage(), _userEnforcedLanguage}) {
        if (!enforcedLocale.isEmpty()) {
            enforcedLanguages.insert(enforcedLocale);
            uiLanguages.prepend(enforcedLocale);
        }
    }

    qCDebug(lcApplication) << "Enforced languages:" << enforcedLanguages;

    QTranslator *translator = new QTranslator(this);
    QTranslator *qtTranslator = new QTranslator(this);
    QTranslator *qtkeychainTranslator = new QTranslator(this);

    for (QString lang : qAsConst(uiLanguages)) {
        lang.replace(QLatin1Char('-'), QLatin1Char('_')); // work around QTBUG-25973
        lang = substLang(lang);
        const QString trFile = Translations::translationsFilePrefix() + lang;
        if (translator->load(trFile, trPath) || lang.startsWith(QLatin1String("en"))) {
            // Permissive approach: Qt and keychain translations
            // may be missing, but Qt translations must be there in order
            // for us to accept the language. Otherwise, we try with the next.
            // "en" is an exception as it is the default language and may not
            // have a translation file provided.
            qCInfo(lcApplication) << "Using" << lang << "translation";
            _displayLanguage = lang;

            const QString qtTrPath = QLibraryInfo::path(QLibraryInfo::TranslationsPath);
            qCDebug(lcApplication) << "qtTrPath:" << qtTrPath;
            const QString qtTrFile = QLatin1String("qt_") + lang;
            qCDebug(lcApplication) << "qtTrFile:" << qtTrFile;
            const QString qtBaseTrFile = QLatin1String("qtbase_") + lang;
            qCDebug(lcApplication) << "qtBaseTrFile:" << qtBaseTrFile;

            if (!qtTranslator->load(qtTrFile, qtTrPath)) {
                if (!qtTranslator->load(qtTrFile, trPath)) {
                    if (!qtTranslator->load(qtBaseTrFile, qtTrPath)) {
                        if (!qtTranslator->load(qtBaseTrFile, trPath)) {
                            qCCritical(lcApplication) << "Could not load Qt translations";
                        }
                    }
                }
            }

            const QString qtkeychainTrFile = QLatin1String("qtkeychain_") + lang;
            if (!qtkeychainTranslator->load(qtkeychainTrFile, qtTrPath)) {
                if (!qtkeychainTranslator->load(qtkeychainTrFile, trPath)) {
                    qCCritical(lcApplication) << "Could not load qtkeychain translations";
                }
            }

            if (!translator->isEmpty() && !qApp->installTranslator(translator)) {
                qCCritical(lcApplication) << "Failed to install translator";
            }
            if (!qtTranslator->isEmpty() && !qApp->installTranslator(qtTranslator)) {
                qCCritical(lcApplication) << "Failed to install Qt translator";
            }
            if (!qtkeychainTranslator->isEmpty() && !qApp->installTranslator(qtkeychainTranslator)) {
                qCCritical(lcApplication) << "Failed to install qtkeychain translator";
            }

            // makes sure widgets with locale-dependent formatting, e.g., QDateEdit, display the correct formatting
            // if the language is provided by the system locale anyway (i.e., coming from QLocale::system().uiLanguages()), we should
            // not mess with the system locale, though
            // if we did, we would enforce a locale for no apparent reason
            // see https://github.com/owncloud/client/issues/8608 for more information
            if (enforcedLanguages.contains(lang)) {
                QLocale newLocale(lang);
                qCDebug(lcApplication) << "language" << lang << "was enforced, changing default locale to" << newLocale;
                QLocale::setDefault(newLocale);
            }

            break;
        }
    }
}

void Application::openVirtualFile(const QString &filename)
{
    QString virtualFileExt = Theme::instance()->appDotVirtualFileSuffix();
    if (!filename.endsWith(virtualFileExt)) {
        qWarning(lcApplication) << "Can only handle file ending in .owncloud. Unable to open" << filename;
        return;
    }
    QString relativePath;
    auto folder = FolderMan::instance()->folderForPath(filename, &relativePath);
    if (!folder) {
        qWarning(lcApplication) << "Can't find sync folder for" << filename;
        // TODO: show a QMessageBox for errors
        return;
    }
    folder->implicitlyHydrateFile(relativePath);
    QString normalName = filename.left(filename.size() - virtualFileExt.size());
    auto con = QSharedPointer<QMetaObject::Connection>::create();
    *con = connect(folder, &Folder::syncFinished, folder, [folder, con, normalName] {
        folder->disconnect(*con);
        if (QFile::exists(normalName)) {
            QDesktopServices::openUrl(QUrl::fromLocalFile(normalName));
        }
    });
}

void Application::tryTrayAgain()
{
    qCInfo(lcApplication) << "Trying tray icon, tray available:" << QSystemTrayIcon::isSystemTrayAvailable();
    if (!_gui->contextMenuVisible())
        _gui->hideAndShowTray();
}

bool Application::eventFilter(QObject *obj, QEvent *event)
{
#ifdef Q_OS_MAC
    if (event->type() == QEvent::FileOpen) {
        QFileOpenEvent *openEvent = static_cast<QFileOpenEvent *>(event);
        qCDebug(lcApplication) << "QFileOpenEvent" << openEvent->file();
        // virtual file, open it after the Folder were created (if the app is not terminated)
        QString fn = openEvent->file();
        QTimer::singleShot(0, this, [this, fn] { openVirtualFile(fn); });
    }
#endif
    return QObject::eventFilter(obj, event);
}
} // namespace OCC
