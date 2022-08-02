#include "serverurlsetupwizardpage.h"
#include "ui_serverurlsetupwizardpage.h"

#include "theme.h"

#include <QIcon>


namespace OCC::Wizard {

ServerUrlSetupWizardPage::ServerUrlSetupWizardPage(const QUrl &serverUrl)
    : _ui(new ::Ui::ServerUrlSetupWizardPage)
{
    _ui->setupUi(this);

    _ui->welcomeTextLabel->setText(tr("Welcome to %1").arg(Theme::instance()->appNameGUI()));

    _ui->urlLineEdit->setText(serverUrl.toString());

    connect(this, &AbstractSetupWizardPage::pageDisplayed, this, [this]() {
        _ui->urlLineEdit->setFocus();
    });

    _ui->logoLabel->setText(QString());
    _ui->logoLabel->setPixmap(Theme::instance()->wizardHeaderLogo().pixmap(200, 200));

    if (!Theme::instance()->wizardUrlPostfix().isEmpty()) {
        _ui->urlLineEdit->setPostfix(Theme::instance()->wizardUrlPostfix());
    }
}

QString ServerUrlSetupWizardPage::userProvidedUrl() const
{
    QString url = _ui->urlLineEdit->text().simplified();

    if (!Theme::instance()->wizardUrlPostfix().isEmpty()) {
        url += Theme::instance()->wizardUrlPostfix();
    }

    return url;
}

ServerUrlSetupWizardPage::~ServerUrlSetupWizardPage()
{
    delete _ui;
}

bool ServerUrlSetupWizardPage::validateInput()
{
    return !_ui->urlLineEdit->text().isEmpty();
}
}
