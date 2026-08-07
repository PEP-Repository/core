#include <pep/assessor/dateeditor.hpp>

#include <pep/assessor/ui_dateeditor.h>

#include <QCalendarWidget>
#include <QDateEdit>
#include <QDebug>

const QString twoDigitYear("yy");
const QString fourDigitYear("yyyy");

DateEditor::DateEditor(QWidget *parent) :
    QWidget(parent),
    ui_(new Ui::DateEditor)
{
  ui_->setupUi(this);
  setFocusProxy(ui_->dateEdit);

  auto locale = QLocale();
  auto calendar = ui_->dateEdit->calendarWidget();
  if (calendar != nullptr) {
    calendar->setLocale(locale);
  }

  // Use four-digit year input, even if the locale specifies a two-digit year
  auto dateFormat = locale.dateFormat(QLocale::FormatType::ShortFormat);
  if (dateFormat.contains(twoDigitYear, Qt::CaseSensitive) && !dateFormat.contains(fourDigitYear, Qt::CaseSensitive)) {
    dateFormat.replace(twoDigitYear, fourDigitYear, Qt::CaseSensitive);
    ui_->dateEdit->setDisplayFormat(dateFormat);
  }

  QObject::connect(ui_->dateEdit, &QDateEdit::dateChanged, this, &DateEditor::valueChanged);
}

DateEditor::~DateEditor()
{
  delete ui_;
}

QDate DateEditor::getValue() const {
  return ui_->dateEdit->date();
}

void DateEditor::setValue(const QDate& value) {
  if (value != this->getValue()) {
    ui_->dateEdit->setDate(value);
    emit valueChanged();
  }
}

bool DateEditor::hasAcceptableInput() const {
  return ui_->dateEdit->hasAcceptableInput();
}
