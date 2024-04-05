/*
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

#ifndef ACCOUNTSETTINGS_H
#define ACCOUNTSETTINGS_H


#include "folder.h"
#include "loginrequireddialog.h"
#include "owncloudgui.h"
#include "progressdispatcher.h"


#include <QSortFilterProxyModel>
#include <QWidget>

class QModelIndex;
class QNetworkReply;
class QLabel;

namespace OCC {
class AccountModalWidget;

namespace Ui {
    class AccountSettings;
}

class FolderMan;

class Account;
class AccountState;
class FolderStatusModel;
class FolderStatusDelegate;

/**
 * @brief The AccountSettings class
 * @ingroup gui
 */
class AccountSettings : public QWidget
{
    Q_OBJECT
    Q_PROPERTY(AccountStatePtr accountState MEMBER _accountState)
    Q_PROPERTY(QSortFilterProxyModel *model MEMBER _sortModel CONSTANT)

public:
    enum class ModalWidgetSizePolicy { Minimum = QSizePolicy::Minimum, Expanding = QSizePolicy::Expanding };
    Q_ENUM(ModalWidgetSizePolicy);

    explicit AccountSettings(const AccountStatePtr &accountState, QWidget *parent = nullptr);
    ~AccountSettings() override;

    AccountStatePtr accountsState() const { return _accountState; }

    void addModalLegacyDialog(QWidget *widget, ModalWidgetSizePolicy sizePolicy);
    void addModalWidget(AccountModalWidget *widget);

    auto model() { return _sortModel; }

Q_SIGNALS:
    void folderChanged();
    void showIssuesList();

public Q_SLOTS:
    void slotAccountStateChanged();

protected Q_SLOTS:
    void slotAddFolder();
    void slotEnableCurrentFolder(Folder *folder, bool terminate = false);
    void slotForceSyncCurrentFolder(Folder *folder);
    void slotRemoveCurrentFolder(Folder *folder);
    void slotEnableVfsCurrentFolder(Folder *folder);
    void slotDisableVfsCurrentFolder(Folder *folder);
    void slotFolderWizardAccepted();
    void slotDeleteAccount();
    void slotToggleSignInState();
    void slotCustomContextMenuRequested(Folder *folder);

private:
    void showSelectiveSyncDialog(Folder *folder);
    void showConnectionLabel(const QString &message,
        QStringList errors = QStringList());

    bool event(QEvent *) override;
    void createAccountToolbox();
    void doForceSyncCurrentFolder(Folder *selectedFolder);

    Ui::AccountSettings *ui;

    FolderStatusModel *_model;
    QSortFilterProxyModel *_sortModel;
    bool _wasDisabledBefore;
    AccountStatePtr _accountState;
    QAction *_toggleSignInOutAction;
    QAction *_toggleReconnect;
    // are we already in the destructor
    bool _goingDown = false;
};

} // namespace OCC

#endif // ACCOUNTSETTINGS_H
