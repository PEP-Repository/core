#include <pep/assessor/devicewidget.hpp>

#include <pep/assessor/ui_devicewidget.h>

#include <pep/assessor/InputValidationTooltip.hpp>
#include <QRegularExpression>
#include <QRegularExpressionValidator>

DeviceWidget::DeviceWidget(const pep::DeviceRegistrationDefinition& definition, QWidget* parent)
  : QWidget(parent), ui_(new Ui::DeviceWidget), definition_(definition) {
  ui_->setupUi(this);
  ui_->retranslateUi(this);

  QRegularExpression devicesRegExp(QRegularExpression::anchoredPattern(QString::fromStdString(definition.serialNumberFormat)));
  ui_->deviceIdInput->setValidator(new QRegularExpressionValidator(devicesRegExp, this));

  if (!definition_.tooltip.empty()) {
    SetInputValidationTooltip(ui_->deviceIdInput, QString::fromStdString(definition_.tooltip));
  }
  if (!definition_.placeholder.empty()) {
    ui_->deviceIdInput->setPlaceholderText(QString::fromStdString(definition_.placeholder));
  }
  QObject::connect(ui_->deviceIdInput, &QLineEdit::textChanged, [this]() {
    ui_->deviceOk->setEnabled(currentlyHasDevice() || ui_->deviceIdInput->hasAcceptableInput());
  });

  toggleDeviceManagement(false);
}

DeviceWidget::~DeviceWidget() {
  delete ui_;
}

QString DeviceWidget::getColumnName() const {
  return QString::fromStdString(definition_.columnName);
}

void DeviceWidget::setDeviceId(QString deviceId) {
  deviceId_ = deviceId;
  ui_->deviceIdInput->clear();
  toggleDeviceManagement(false);
}

bool DeviceWidget::currentlyHasDevice() const {
  return !deviceId_.isEmpty();
}

void DeviceWidget::manageDevices() {
  this->toggleDeviceManagement(true);
}

void DeviceWidget::applyDeviceUpdate() {
  this->toggleDeviceManagement(false);
  ui_->manageDevices->setFocus();
  if (currentlyHasDevice()) {
    emit deviceDeregistered(getColumnName(), deviceId_);
  }
  else {
    emit deviceRegistered(getColumnName(), ui_->deviceIdInput->text());
  }
}

void DeviceWidget::cancelDeviceUpdate() {
  this->toggleDeviceManagement(false);
  ui_->manageDevices->setFocus();
}

QString DeviceWidget::getDeviceDescription() const {
  auto result = QString::fromStdString(definition_.description);
  if (result.isEmpty()) {
    result = tr("device");
  }
  return result;
}

void DeviceWidget::toggleDeviceManagement(bool show) {
  auto description = getDeviceDescription();
  if (show) {
    ui_->deviceCancel->show();
    ui_->deviceOk->show();
    ui_->deviceOk->setEnabled(currentlyHasDevice() || ui_->deviceIdInput->hasAcceptableInput());
    ui_->manageDevices->hide();
    if (!currentlyHasDevice()) {
      ui_->device_info->setText(tr("Enter new ID for %1:").arg(description));
      ui_->deviceIdInput->show();
      ui_->deviceIdInput->setFocus();
    }
    else {
      ui_->device_info->setText(tr("Deregister %1 '%2'?").arg(description, deviceId_));
    }
  }
  else {
    ui_->deviceIdInput->hide();
    ui_->deviceCancel->hide();
    ui_->deviceOk->hide();
    ui_->deviceOk->setEnabled(currentlyHasDevice() || ui_->deviceIdInput->hasAcceptableInput());
    ui_->manageDevices->setText(tr(currentlyHasDevice() ? "Deregister %1" : "Register %1").arg(description));
    ui_->manageDevices->show();
    QString info;
    if (currentlyHasDevice()) {
      info = tr("Registered to %1 '%2'").arg(description, deviceId_);
    }
    else {
      info = tr("No %1 registered").arg(description);
    }
    ui_->device_info->setText(info);
  }
}
