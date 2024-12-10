#include <pep/assessor/loginwidget.hpp>
#include <pep/assessor/ui_loginwidget.h>

#include <pep/application/Application.hpp>
#include <pep/versioning/Version.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/gui/QTrxGui.hpp>
#include <pep/utils/Log.hpp>
#include <pep/utils/Defer.hpp>
#include <pep/assessor/Async.hpp>
#include <pep/assessor/Installer.hpp>

#include <QDebug>
#include <QMessageBox>
#include <QProcess>
#include <QStyle>
#include <QThread>
#include <QFileDialog>

#ifdef _WIN32
#include <windows.h>
#else
#include <unistd.h>
#endif /*_WIN32*/

static const std::string LOG_TAG("LoginWidget");

#ifdef _WIN32

namespace {

std::filesystem::path GetPepAppDataPath() {
  return pep::win32api::GetKnownFolderPath(pep::win32api::KnownFolder::RoamingAppData) / "PEP";
}

}
#endif

LoginWidget::LoginWidget(std::shared_ptr<boost::asio::io_service> ioService, const pep::Configuration& projectConfig, const pep::Configuration& config, const std::filesystem::path& exeDirectory)
  : QWidget(nullptr)
  , authy(pep::OAuthClient::Create(pep::OAuthClient::Parameters{ioService, config.get_child("AuthenticationServer")}))
  , exeDirectory(exeDirectory)
  , adminAccountSampleFormat(projectConfig.get<std::optional<std::string>>("AdminAccountSampleFormat"))
  , ui(new Ui::LoginWidget) {
  Q_INIT_RESOURCE(resources);
  ui->setupUi(this);

  auto branding = Branding::Get(projectConfig, "Branding");
  setWindowTitle(branding.getProjectName());
  branding.showLogo(*ui->pepLabel);

  auto cfgVersion = pep::ConfigVersion::Current();
  if (cfgVersion != std::nullopt) {
    emit version(QString::fromStdString(cfgVersion->getSummary()));
  }

  QObject::connect(this, &LoginWidget::loginSuccess, this, &LoginWidget::close);
  QObject::connect(this, &LoginWidget::loginFailure, this, &LoginWidget::on_loginFailure);

  if (pep::ConfigVersion::Current() != std::nullopt) {
    if (!pep::ConfigVersion::Current()->exposesProductionData()) {
      setProperty("nonrelease", true);
      style()->unpolish(this);
      style()->polish(this);
    }
  }
  #ifdef _WIN32
    this->provideUpdateIfAvailable();
  #elif defined(__APPLE__) && defined(__MACH__)
    // start by making the login button unclickable and changing the text
    ui->loginButton->setText(tr("Checking for updates"));
    ui->loginButton->setEnabled(false); // Make the button unclickable

    // Create the updater, connect the SparkleUpdater signal to the provideUpdate 
    // slot, then begin update check, provideUpdate will call provideUpdateIfAvailable
    updater = new SparkleUpdater();
    QObject::connect(updater, &SparkleUpdater::updateStatusChanged, this, &LoginWidget::provideUpdate);
    updater->checkForUpdateInformation();
  #endif
}

#ifdef _WIN32 // Windows updater
void LoginWidget::provideUpdateIfAvailable() {

  auto installer = Installer::GetAvailable();
  if (installer == nullptr) {
    LOG(LOG_TAG, pep::debug) << "No available installer found: do not do update";
  }
  else if (!installer->supersedesRunningVersion()) {
    LOG(LOG_TAG, pep::debug) << "Available installer does not supersede running software version: do not do update";
  }
  else {
    LOG(LOG_TAG, pep::debug) << "Superseding installer found: providing update option";

    //Change color of LoginWidget to alert user
    this->setStyleSheet(QStringLiteral("background-color: #d3cb58;"));
    this->style()->unpolish(this);
    this->style()->polish(this);
    auto current = pep::ConfigVersion::Current();
    assert(current != std::nullopt);
    assert(current->isGitlabBuild());
    emit version(tr("Software is out of date. Current: %1/%2. Available: %3/%4.").arg(
      QString::fromStdString(current->getPipelineId()), QString::fromStdString(current->getJobId()),
      QString::number(installer->getPipelineId()), QString::number(installer->getJobId())));
    //Reconfigure login button.
    QObject::disconnect(ui->loginButton, 0, 0, 0);
    ui->loginButton->setText(tr("Update"));
    QObject::connect(ui->loginButton, &QPushButton::clicked, [this, installer]() {
      ui->loginButton->setEnabled(false);
      ui->loginButton->setText(tr("Updating..."));
      Async::Run(this, [this, installer]() {
        Installer::Context context{ GetPepAppDataPath(), this->exeDirectory / "pepElevate.exe", [this]() {
    auto message = adminAccountSampleFormat
      ? tr("Please enter credentials for an administrative account that can install the software. Use format 'user@domain.tld' for domain accounts, e.g. '%1'.")
      .arg(QString::fromStdString(*adminAccountSampleFormat))
      : tr("Please enter credentials for an administrative account that can install the software. Use format 'user@domain.tld' for domain accounts.");
    return pep::win32api::PlaintextCredentials::FromPrompt(
      reinterpret_cast<HWND>(this->winId()),
      tr("PEP Update").toStdString(),
      message.toStdString());
        } };

        installer->start(context);
      }, [this](std::exception_ptr error) {this->on_updateStarted(error); });
    });
  }
}

#elif defined(__APPLE__) && defined(__MACH__)
// if an update is available, change the login button to an update button
void LoginWidget::provideUpdate(bool updateFound) {
  this->provideUpdateIfAvailable(updateFound);
}

void LoginWidget::provideUpdateIfAvailable(bool updateFound) {
  
  // if no update is available, change the login button back to normal
  if (!updateFound) {
    LOG(LOG_TAG, pep::debug) << "No available installer found: do not do update";
    ui->loginButton->setText(tr("Login")); // Change the text back
    ui->loginButton->setEnabled(true);
  }
  // else, change the login button to an update button
  else {
    LOG(LOG_TAG, pep::debug) << "Superseding installer found: providing update option";
    this->setStyleSheet(QStringLiteral("background-color: #d3cb58;"));
    this->style()->unpolish(this);
    this->style()->polish(this);
    QObject::disconnect(ui->loginButton, 0, 0, 0);
    // Have the button actually open the sparkle updater popup
    QObject::connect(ui->loginButton, &QPushButton::clicked, updater, &SparkleUpdater::checkForUpdates);
    ui->loginButton->setText(tr("Update"));
    ui->loginButton->setEnabled(true);
  }
}
#endif

LoginWidget::~LoginWidget() {
  delete ui;
}

/*! \brief Run surfconex login
 *
 * \return none.
 */
void LoginWidget::on_loginButton_clicked() {
  std::cerr << "loginButton clicked" << std::endl;

  ui->loginButton->setEnabled(false);
  authy->run()
    .observe_on(observe_on_gui())
    .subscribe(
    [this](std::string token) {
      on_userLoggedin(QString::fromStdString(token));
      ui->loginButton->setEnabled(true);
    },
    [this](std::exception_ptr ep) {
      QMessageBox box(this);
      box.setModal(true);
      box.setWindowModality(Qt::ApplicationModal);
      box.setStyleSheet(QStringLiteral("background-color: #d3cb58;"));
      box.setWindowTitle(tr("Cannot log on"));
      box.setText(tr("Logon failed because of a technical issue. Please contact your software supplier and report the following error text:") + "\n\n" + QString::fromStdString(pep::GetExceptionMessage(ep)));
      box.setIcon(QMessageBox::Icon::Critical);
      box.setStandardButtons(QMessageBox::Close);
      box.setDefaultButton(QMessageBox::Close);

      box.exec();
      throw;
      });
}

/*! \brief Run client update
 *
 * Runs the client update helper and terminates pep client.
 * \return none.
 */
void LoginWidget::on_updateStarted(std::exception_ptr error) {
  if (error == nullptr) {
    // At this point, the update has been initiated. Terminate pepAssessor so that the .exe can be replaced
    std::exit(EXIT_SUCCESS);
  }

  auto failureReason = pep::GetExceptionMessage(error);
  QString failureMessage;
  if (!failureReason.empty()) {
    LOG(LOG_TAG, pep::error) << "Updating failed: " << failureReason;
    failureMessage = tr("Software cannot update: %1").arg(failureReason.c_str());
  }
  else {
    LOG(LOG_TAG, pep::error) << "Updating failed (no detail available)";
    failureMessage = tr("Software cannot update.");
  }

  QMessageBox warningBox(this);
  warningBox.setModal(true);
  warningBox.setWindowModality(Qt::ApplicationModal);
  warningBox.setFixedSize(warningBox.size());
  warningBox.setStyleSheet(QStringLiteral("background-color: #d3cb58;"));
  warningBox.setWindowTitle(tr("cannot-update-title"));
  warningBox.setText(failureMessage + "\n" + tr("Would you like to continue with your outdated version?"));
  warningBox.setStandardButtons(QMessageBox::Ok | QMessageBox::Close);
  warningBox.setDefaultButton(QMessageBox::Close);

  if (warningBox.exec() != QMessageBox::Ok) {
    std::exit(EXIT_FAILURE);
  }

  //Change color of LoginWidget to restore to normal
  auto style = QStringLiteral("background-color: #8db6d3;"); // Default to production color
  if (pep::ConfigVersion::Current() != std::nullopt) {
    if (!pep::ConfigVersion::Current()->exposesProductionData()) {
      style = QStringLiteral("background-color: #f44336;"); // Switch to nonproduction color if we know for sure that we're not showing production data
    }
  }
  this->setStyleSheet(style);
  this->style()->unpolish(this);
  this->style()->polish(this);

  //Reconfigure login button.
  QObject::disconnect(ui->loginButton, nullptr, nullptr, nullptr);
  ui->loginButton->setText("Login");
  QObject::connect(ui->loginButton, SIGNAL(clicked()), this, SLOT(on_loginButton_clicked()));
  ui->loginButton->setEnabled(true);
}

/*! \brief Code run once a login has been confirmed
 *
 * When a user has logged in via surfconex a token is received and needs to be passed to the rest of the program. This code passes the token and uses it to log the user into the pep infrastructure.
 *
 * \param token to use for pep login.
 * \return none.
 */
void LoginWidget::on_userLoggedin(QString token) {
  //Put actual login auth code here
  qDebug() << "OAuth token in use: " << token;
  emit loginSuccess(token);
}

/*! \brief Visually identify failed/terminated login
 *
 * Changes the UI background to red. This is done to make it immediately clear to the user that something has gone wrong.
 * \return none.
 */
void LoginWidget::on_loginFailure() {
  //Change color to alert user of error
  this->setStyleSheet(QStringLiteral("background-color: #d36358;"));
  this->style()->unpolish(this);
  this->style()->polish(this);
}
