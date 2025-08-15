#pragma once

#include <pep/assessor/UserRole.hpp>
#include <pep/oauth-client/OAuthClient.hpp>
#include <pep/assessor/Branding.hpp>

#include <QWidget>
#include <filesystem>

#if defined(__APPLE__) && defined(__MACH__)
# include <pep/assessor/SparkleUpdater.h>
#endif

namespace Ui {
class LoginWidget;
}

class LoginWidget : public QWidget {
  Q_OBJECT
  std::shared_ptr<pep::OAuthClient> authy;
  std::filesystem::path exeDirectory;
  std::optional<std::string> adminAccountSampleFormat;

 public:
  explicit LoginWidget(std::shared_ptr<boost::asio::io_context> io_context, const pep::Configuration& projectConfig, const pep::Configuration& config, const std::filesystem::path& exeDirectory);
  ~LoginWidget() override;

signals:
  void loginSuccess(QString token);
  void version(QString);

#if defined(__APPLE__) && defined(__MACH__)
public slots:
    void provideUpdate(bool updateFound);
public:
  void provideUpdateIfAvailable(bool updateFound);
  SparkleUpdater* updater;
#else
public:
  void provideUpdateIfAvailable();
#endif

private slots:
  void on_loginButton_clicked();
  void on_userLoggedin(QString token);
  void on_loginFailure(QString announcement, std::exception_ptr error);

private:
  void on_updateStarted(std::exception_ptr error);

private:
  Ui::LoginWidget* ui;
};
