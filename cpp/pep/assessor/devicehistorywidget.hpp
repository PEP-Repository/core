#pragma once

#include <pep/content/ParticipantDeviceHistory.hpp>
#include <pep/structure/GlobalConfiguration.hpp>

#include <QListWidgetItem>
#include <QWidget>

#include <cstddef>

namespace Ui {
class DeviceHistoryWidget;
}

class DeviceHistoryWidget : public QWidget
{
  Q_OBJECT

private:
  Ui::DeviceHistoryWidget *ui_;
  pep::DeviceRegistrationDefinition definition_;

public:
  explicit DeviceHistoryWidget(const pep::DeviceRegistrationDefinition& definition, QWidget *parent = nullptr);
  ~DeviceHistoryWidget() override;

  QString getColumnName() const;
  void setHistory(const pep::ParticipantDeviceHistory& history);

signals:
  void itemActivated(QString columnName, size_t index);

private slots:
  void listItemActivated(QListWidgetItem* item);
};
