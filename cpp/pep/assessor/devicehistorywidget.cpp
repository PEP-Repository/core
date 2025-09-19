#include <pep/assessor/devicehistorywidget.hpp>
#include <pep/assessor/ui_devicehistorywidget.h>

#include <pep/assessor/QDate.hpp>

#include <cassert>

DeviceHistoryWidget::DeviceHistoryWidget(const pep::DeviceRegistrationDefinition& definition, QWidget *parent) :
  QWidget(parent),
  ui(new Ui::DeviceHistoryWidget),
  mDefinition(definition)
{
  ui->setupUi(this);
  ui->retranslateUi(this);

  if (!mDefinition.description.empty()) {
    ui->devices_history_subheader->setText(QString::fromStdString(mDefinition.description));
  }
  QObject::connect(ui->device_history_listWidget, &QListWidget::itemActivated, this, &DeviceHistoryWidget::listItemActivated);
}

QString DeviceHistoryWidget::getColumnName() const {
  return QString::fromStdString(mDefinition.columnName);
}

void DeviceHistoryWidget::listItemActivated(QListWidgetItem* item) {
  assert(item != nullptr);
  assert(item->isSelected());
  assert(item->listWidget() == ui->device_history_listWidget);
  auto selectedIndexes = ui->device_history_listWidget->selectionModel()->selectedIndexes();

  assert(selectedIndexes.begin() != selectedIndexes.end());
  assert(selectedIndexes.begin() + 1 == selectedIndexes.end());
  auto row = selectedIndexes.begin()->row();
  assert(row >= 0);
  auto index = static_cast<size_t>(row);

  emit itemActivated(getColumnName(), index);
}

DeviceHistoryWidget::~DeviceHistoryWidget()
{
    delete ui;
}

void DeviceHistoryWidget::setHistory(const pep::ParticipantDeviceHistory& history) {
  ui->device_history_listWidget->clear();
  for (auto i = history.begin(); i != history.end(); ++i) {
    auto timestamp = QLocale().toString(TimeTToLocalQDateTime(pep::DateTime::FromDeviceRecordTimestamp(i->time).toTimeT()), QLocale::FormatType::LongFormat);
    ui->device_history_listWidget->addItem(QString::fromStdString(i->serial) + " " + (i->isActive() ? tr("deviceRegisteredOn") : tr("deviceUnregisteredOn")) + " " + timestamp);
  }
}
