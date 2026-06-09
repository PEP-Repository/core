#include <pep/assessor/notconnectedwidget.hpp>

#include <pep/auth/ServerTraits.hpp>

#include <pep/assessor/ui_notconnectedwidget.h>

void NotConnectedWidget::appendConnectionStatus(QString& destination, const std::string& server, pep::ConnectionStatus status) const {
  if (!status.connected) {
    const char *format{};

    switch (status.error.value()) {
    case boost::system::errc::wrong_protocol_type:
      format = "Cannot connect to %1 because it has a different version.";
      break;

    default:
      format = "Not connected to %1.";
      break;
    }

    if (!destination.isEmpty()) {
      destination += "\r\n\r\n";
    }
    destination += tr(format).arg(QString::fromStdString(server).toLower());
  }
}

NotConnectedWidget::NotConnectedWidget(pep::ConnectionStatus accessManager, pep::ConnectionStatus keyServer, pep::ConnectionStatus storageFacility, QWidget* parent) :
  QWidget(parent), accessManager(accessManager), keyServer(keyServer), storageFacility(storageFacility), ui(new Ui::NotConnectedWidget) {
  ui->setupUi(this);
  QString text;

  appendConnectionStatus(text, pep::ServerTraits::AccessManager().description(), accessManager);
  appendConnectionStatus(text, pep::ServerTraits::KeyServer().description(), keyServer);
  appendConnectionStatus(text, pep::ServerTraits::StorageFacility().description(), storageFacility);

  if (text.isEmpty()) {
    text = tr("Your session has expired and you have been logged out. Please restart the application.");
  }

  ui->label->setText(text);
}

NotConnectedWidget::~NotConnectedWidget() {
  delete ui;
}
