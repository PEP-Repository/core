#pragma once

#include <pep/messaging/ConnectionStatus.hpp>
#include <QWidget>

namespace Ui {
class NotConnectedWidget;
}

class NotConnectedWidget : public QWidget {
  Q_OBJECT

  pep::ConnectionStatus accessManager;
  pep::ConnectionStatus keyServer;
  pep::ConnectionStatus storageFacility;

 public:
  explicit NotConnectedWidget(pep::ConnectionStatus accessManager, pep::ConnectionStatus keyServer, pep::ConnectionStatus storageFacility, QWidget* parent = nullptr);
  ~NotConnectedWidget() override;

 private:
  Ui::NotConnectedWidget* ui;

  void appendConnectionStatus(QString& destination, QString facility, pep::ConnectionStatus status) const;
};
