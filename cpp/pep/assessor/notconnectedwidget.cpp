#include <pep/assessor/notconnectedwidget.hpp>

#include <pep/assessor/ui_notconnectedwidget.h>

void NotConnectedWidget::appendConnectionStatus(QString& destination, QString facility, pep::ConnectionStatus status) const {
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
    destination += tr(format).arg(facility);
  }
}

NotConnectedWidget::NotConnectedWidget(pep::ConnectionStatus accessManager, pep::ConnectionStatus keyServer, pep::ConnectionStatus storageFacility, QWidget* parent) :
  QWidget(parent), accessManager(accessManager), keyServer(keyServer), storageFacility(storageFacility), ui(new Ui::NotConnectedWidget) {
  ui->setupUi(this);
  QString text;

  appendConnectionStatus(text, "access manager", accessManager);
  appendConnectionStatus(text, "keyserver", keyServer);
  appendConnectionStatus(text, "storage facility", storageFacility);

  if (text.isEmpty()) {
    text = tr("Your session has expired and you have been logged out. Please restart the application.");
  }

  ui->label->setText(text);
}

NotConnectedWidget::~NotConnectedWidget() {
  delete ui;
}
