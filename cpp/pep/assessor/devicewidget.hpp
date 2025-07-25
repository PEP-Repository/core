#pragma once

#include <pep/structure/GlobalConfiguration.hpp>
#include <QWidget>

namespace Ui {
class DeviceWidget;
}

class DeviceWidget : public QWidget {
  Q_OBJECT

 private:
  Ui::DeviceWidget* ui;
  pep::DeviceRegistrationDefinition mDefinition;
  QString mDeviceId;

 public:
  explicit DeviceWidget(const pep::DeviceRegistrationDefinition& definition, QWidget* parent);
  ~DeviceWidget() override;

  QString getColumnName() const;
  void setDeviceId(QString deviceId);

signals:
  void deviceRegistered(QString columnName, QString deviceId);
  void deviceDeregistered(QString columnName, QString deviceId);

private slots:
  void manageDevices();
  void applyDeviceUpdate();
  void cancelDeviceUpdate();

private:
  bool currentlyHasDevice() const;
  QString getDeviceDescription() const;

  void toggleDeviceManagement(bool show);
};
