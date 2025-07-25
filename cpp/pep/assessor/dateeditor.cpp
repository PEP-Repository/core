#include <pep/assessor/dateeditor.hpp>

#include <pep/assessor/ui_dateeditor.h>

#include <QCalendarWidget>
#include <QDateEdit>
#include <QDebug>

const QString twoDigitYear("yy");
const QString fourDigitYear("yyyy");

DateEditor::DateEditor(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::DateEditor)
{
  ui->setupUi(this);
  setFocusProxy(ui->dateEdit);

  auto locale = QLocale();
  auto calendar = ui->dateEdit->calendarWidget();
  if (calendar != nullptr) {
    calendar->setLocale(locale);
  }

  // Use four-digit year input, even if the locale specifies a two-digit year
  auto dateFormat = locale.dateFormat(QLocale::FormatType::ShortFormat);
  if (dateFormat.contains(twoDigitYear, Qt::CaseSensitive) && !dateFormat.contains(fourDigitYear, Qt::CaseSensitive)) {
    dateFormat.replace(twoDigitYear, fourDigitYear, Qt::CaseSensitive);
    ui->dateEdit->setDisplayFormat(dateFormat);
  }

  QObject::connect(ui->dateEdit, &QDateEdit::dateChanged, this, &DateEditor::valueChanged);
}

DateEditor::~DateEditor()
{
  delete ui;
}

QDate DateEditor::getValue() const {
  return ui->dateEdit->date();
}

void DateEditor::setValue(const QDate& value) {
  if (value != this->getValue()) {
    ui->dateEdit->setDate(value);
    emit valueChanged();
  }
}

bool DateEditor::hasAcceptableInput() const {
  return ui->dateEdit->hasAcceptableInput();
}
