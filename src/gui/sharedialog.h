/*
 * Copyright (C) by Roeland Jago Douma <roeland@famdouma.nl>
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

#ifndef SHAREDIALOG_H
#define SHAREDIALOG_H

#include "networkjobs.h"
#include "accountfwd.h"
#include "QProgressIndicator.h"
#include <QDialog>
#include <QTreeWidgetItem>
#include <QScopedPointer>

namespace OCC {

class OcsShareJob : public AbstractNetworkJob {
    Q_OBJECT
public:
    explicit OcsShareJob(const QByteArray& verb, const QUrl& url, AccountPtr account, QObject* parent = 0);

    void setPostParams(const QList<QPair<QString, QString> >& postParams);

public slots:
    void start() Q_DECL_OVERRIDE;
signals:
    void jobFinished(QString reply);
private slots:
    virtual bool finished() Q_DECL_OVERRIDE;
private:
    QByteArray _verb;
    QUrl _url;
    QList<QPair<QString, QString> > _postParams;
};

class OcsCapabilities {

public:
    OcsCapabilities(const QVariantMap &capabilities);

    bool isValid();
    bool sharingEnabled();
    bool passwordEnforced();
    bool expireEnabled();
    bool expireEnforced();
    int expireDays();

private:
    QVariantMap _capabilities;
};

namespace Ui {
class ShareDialog;
}

class AbstractCredentials;
class QuotaInfo;
class MirallAccessManager;
class SyncResult;

class ShareDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ShareDialog(AccountPtr account, const QString &sharePath, const QString &localPath, QWidget *parent = 0);
    ~ShareDialog();
    void getShares();

private slots:
    void slotSharesFetched(const QString &reply);
    void slotCreateShareFetched(const QString &reply);
    void slotDeleteShareFetched(const QString &reply);
    void slotPasswordSet(const QString &reply);
    void slotExpireSet(const QString &reply);
    void slotCalendarClicked(const QDate &date);
    void slotCheckBoxShareLinkClicked();
    void slotCheckBoxPasswordClicked();
    void slotCheckBoxExpireClicked();
    void slotPasswordReturnPressed();
    void slotPushButtonCopyLinkPressed();
    void slotThumbnailFetched(const int &statusCode, const QByteArray &reply);
private:
    void displayError(int code);
    void displayInfo( const QString& msg );

    Ui::ShareDialog *_ui;
    AccountPtr _account;
    QString _sharePath;
    QString _localPath;
    QString _folderAlias;
    int     _uploadFails;
    QString _expectedSyncFile;

    QList<QVariant> _shares;
    qulonglong _public_share_id;
    void setPassword(const QString &password);
    void setExpireDate(const QDate &date);
    int checkJsonReturnCode(const QString &reply, QString &message);

    QProgressIndicator *_pi_link;
    QProgressIndicator *_pi_password;
    QProgressIndicator *_pi_date;

    QScopedPointer<OcsCapabilities> _caps;
};

}

#endif // SHAREDIALOG_H
