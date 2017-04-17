/*
 * Copyright (C) by Olivier Goffart <ogoffart@woboq.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 */

#pragma once

#include "account.h"
#include "accountstate.h"

namespace OCC {

typedef QSharedPointer<AccountState> AccountStatePtr;

/**
   @brief The AccountManager class
   @ingroup gui
*/
class AccountManager : public QObject {
    Q_OBJECT
public:
    static AccountManager *instance();
    ~AccountManager() {}

    /**
     * Saves the accounts to a given settings file
     */
    void save(bool saveCredentials = true);

    /**
     * Creates account objects from a given settings file.
     * return true if the account was restored
     */
    bool restore();

    /**
     * Add this account in the list of saved accounts.
     * Typically called from the wizard
     */
    AccountState *addAccount(const AccountPtr &newAccount);

    /**
     * remove all accounts
     */
    void shutdown();

    /**
     * Return a list of all accounts.
     * (this is a list of QSharedPointer for internal reasons, one should normally not keep a copy of them)
     */
    QList<AccountStatePtr> accounts() { return _accounts; }

    /**
     * Delete the AccountState
     */
    void deleteAccount(AccountState *account);


    /**
     * Creates an account and sets up some basic handlers.
     * Does *not* add the account to the account manager just yet.
     */
    static AccountPtr createAccount();

private:
    void save(const AccountPtr& account, QSettings& settings, bool saveCredentials = true);
    AccountPtr load(QSettings& settings);
    bool restoreFromLegacySettings();

    bool isAccountIdAvailable(const QString& id) const;
    QString generateFreeAccountId() const;

public slots:
    void wantsAccountSavedSlot(AccountPtr a);


Q_SIGNALS:
    void accountAdded(AccountState *account);
    void accountRemoved(AccountState *account);

private:
    AccountManager() {}
    QList<AccountStatePtr> _accounts;
};

}
