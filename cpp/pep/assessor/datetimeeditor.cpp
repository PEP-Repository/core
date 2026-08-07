#include <pep/assessor/datetimeeditor.hpp>
#include <pep/assessor/ui_datetimeeditor.h>
#include <pep/assessor/QDate.hpp>

#include <stdexcept>
#include <QAbstractSpinBox>
#include <QCalendarWidget>
#include <QDebug>

DateTimeEditor::DateTimeEditor(QWidget *parent) :
  QWidget(parent),
  ui_(new Ui::DateTimeEditor)
{
  ui_->setupUi(this);
  setFocusProxy(ui_->dateEdit);

  QObject::connect(ui_->dateEdit, &DateEditor::valueChanged, this, &DateTimeEditor::valueChanged);
  QObject::connect(ui_->timeEdit, &QDateEdit::timeChanged, this, &DateTimeEditor::valueChanged);
}

DateTimeEditor::~DateTimeEditor()
{
  delete ui_;
}

QDateTime DateTimeEditor::getValue() const {
  auto date = ui_->dateEdit->getValue();
  auto time = ui_->timeEdit->time();
  return pep::MakeLocalQDateTime(date, time);
}

void DateTimeEditor::setValue(const QDateTime& value) {
  if (value.timeSpec() != Qt::LocalTime) {
    throw std::runtime_error("Can only edit date/time values with local time spec");
  }
  if (value != this->getValue()) {
    ui_->dateEdit->setValue(value.date());
    ui_->timeEdit->setTime(value.time());
    emit valueChanged();
  }
}
