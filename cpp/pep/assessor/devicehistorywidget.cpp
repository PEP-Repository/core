#include <pep/assessor/devicehistorywidget.hpp>
#include <pep/assessor/ui_devicehistorywidget.h>

#include <pep/assessor/QDate.hpp>

#include <cassert>

DeviceHistoryWidget::DeviceHistoryWidget(const pep::DeviceRegistrationDefinition& definition, QWidget *parent) :
  QWidget(parent),
  ui_(new Ui::DeviceHistoryWidget),
  definition_(definition)
{
  ui_->setupUi(this);
  ui_->retranslateUi(this);

  if (!definition_.description.empty()) {
    ui_->devices_history_subheader->setText(QString::fromStdString(definition_.description));
  }
  QObject::connect(ui_->device_history_listWidget, &QListWidget::itemActivated, this, &DeviceHistoryWidget::listItemActivated);
}

QString DeviceHistoryWidget::getColumnName() const {
  return QString::fromStdString(definition_.columnName);
}

void DeviceHistoryWidget::listItemActivated(QListWidgetItem* item) {
  assert(item != nullptr);
  assert(item->isSelected());
  assert(item->listWidget() == ui_->device_history_listWidget);
  auto selectedIndexes = ui_->device_history_listWidget->selectionModel()->selectedIndexes();

  assert(selectedIndexes.begin() != selectedIndexes.end());
  assert(selectedIndexes.begin() + 1 == selectedIndexes.end());
  auto row = selectedIndexes.begin()->row();
  assert(row >= 0);
  auto index = static_cast<size_t>(row);

  emit itemActivated(getColumnName(), index);
}

DeviceHistoryWidget::~DeviceHistoryWidget()
{
    delete ui_;
}

void DeviceHistoryWidget::setHistory(const pep::ParticipantDeviceHistory& history) {
  ui_->device_history_listWidget->clear();
  for (auto i = history.begin(); i != history.end(); ++i) {
    auto timestamp = QLocale().toString(pep::LocalQDateTimeFromStdTimestamp(i->time), QLocale::FormatType::LongFormat);
    ui_->device_history_listWidget->addItem(QString::fromStdString(i->serial) + " " + (i->isActive() ? tr("deviceRegisteredOn") : tr("deviceUnregisteredOn")) + " " + timestamp);
  }
}
