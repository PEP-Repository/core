#include <pep/assessor/datetimeeditor.hpp>
#include <pep/assessor/ui_datetimeeditor.h>
#include <pep/assessor/QDate.hpp>

#include <stdexcept>
#include <QAbstractSpinBox>
#include <QCalendarWidget>
#include <QDebug>

DateTimeEditor::DateTimeEditor(QWidget *parent) :
  QWidget(parent),
  ui(new Ui::DateTimeEditor)
{
  ui->setupUi(this);
  setFocusProxy(ui->dateEdit);

  QObject::connect(ui->dateEdit, &DateEditor::valueChanged, this, &DateTimeEditor::valueChanged);
  QObject::connect(ui->timeEdit, &QDateEdit::timeChanged, this, &DateTimeEditor::valueChanged);
}

DateTimeEditor::~DateTimeEditor()
{
  delete ui;
}

QDateTime DateTimeEditor::getValue() const {
  auto date = ui->dateEdit->getValue();
  auto time = ui->timeEdit->time();
  return MakeLocalQDateTime(date, time);
}

void DateTimeEditor::setValue(const QDateTime& value) {
  if (value.timeSpec() != Qt::LocalTime) {
    throw std::runtime_error("Can only edit date/time values with local time spec");
  }
  if (value != this->getValue()) {
    ui->dateEdit->setValue(value.date());
    ui->timeEdit->setTime(value.time());
    emit valueChanged();
  }
}
