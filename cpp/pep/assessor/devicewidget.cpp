#include <pep/assessor/devicewidget.hpp>

#include <pep/assessor/ui_devicewidget.h>

#include <pep/assessor/InputValidationTooltip.hpp>
#include <QRegularExpression>
#include <QRegularExpressionValidator>

DeviceWidget::DeviceWidget(const pep::DeviceRegistrationDefinition& definition, QWidget* parent)
  : QWidget(parent), ui(new Ui::DeviceWidget), mDefinition(definition) {
  ui->setupUi(this);
  ui->retranslateUi(this);

  QRegularExpression devicesRegExp(QRegularExpression::anchoredPattern(QString::fromStdString(definition.serialNumberFormat)));
  ui->deviceIdInput->setValidator(new QRegularExpressionValidator(devicesRegExp, this));

  if (!mDefinition.tooltip.empty()) {
    SetInputValidationTooltip(ui->deviceIdInput, QString::fromStdString(mDefinition.tooltip));
  }
  if (!mDefinition.placeholder.empty()) {
    ui->deviceIdInput->setPlaceholderText(QString::fromStdString(mDefinition.placeholder));
  }
  QObject::connect(ui->deviceIdInput, &QLineEdit::textChanged, [this]() {
    ui->deviceOk->setEnabled(currentlyHasDevice() || ui->deviceIdInput->hasAcceptableInput());
  });

  toggleDeviceManagement(false);
}

DeviceWidget::~DeviceWidget() {
  delete ui;
}

QString DeviceWidget::getColumnName() const {
  return QString::fromStdString(mDefinition.columnName);
}

void DeviceWidget::setDeviceId(QString deviceId) {
  mDeviceId = deviceId;
  ui->deviceIdInput->clear();
  toggleDeviceManagement(false);
}

bool DeviceWidget::currentlyHasDevice() const {
  return !mDeviceId.isEmpty();
}

void DeviceWidget::manageDevices() {
  this->toggleDeviceManagement(true);
}

void DeviceWidget::applyDeviceUpdate() {
  this->toggleDeviceManagement(false);
  ui->manageDevices->setFocus();
  if (currentlyHasDevice()) {
    emit deviceDeregistered(getColumnName(), mDeviceId);
  }
  else {
    emit deviceRegistered(getColumnName(), ui->deviceIdInput->text());
  }
}

void DeviceWidget::cancelDeviceUpdate() {
  this->toggleDeviceManagement(false);
  ui->manageDevices->setFocus();
}

QString DeviceWidget::getDeviceDescription() const {
  auto result = QString::fromStdString(mDefinition.description);
  if (result.isEmpty()) {
    result = tr("device");
  }
  return result;
}

void DeviceWidget::toggleDeviceManagement(bool show) {
  auto description = getDeviceDescription();
  if (show) {
    ui->deviceCancel->show();
    ui->deviceOk->show();
    ui->deviceOk->setEnabled(currentlyHasDevice() || ui->deviceIdInput->hasAcceptableInput());
    ui->manageDevices->hide();
    if (!currentlyHasDevice()) {
      ui->device_info->setText(tr("Enter new ID for %1:").arg(description));
      ui->deviceIdInput->show();
      ui->deviceIdInput->setFocus();
    }
    else {
      ui->device_info->setText(tr("Deregister %1 '%2'?").arg(description, mDeviceId));
    }
  }
  else {
    ui->deviceIdInput->hide();
    ui->deviceCancel->hide();
    ui->deviceOk->hide();
    ui->deviceOk->setEnabled(currentlyHasDevice() || ui->deviceIdInput->hasAcceptableInput());
    ui->manageDevices->setText(tr(currentlyHasDevice() ? "Deregister %1" : "Register %1").arg(description));
    ui->manageDevices->show();
    QString info;
    if (currentlyHasDevice()) {
      info = tr("Registered to %1 '%2'").arg(description, mDeviceId);
    }
    else {
      info = tr("No %1 registered").arg(description);
    }
    ui->device_info->setText(info);
  }
}
