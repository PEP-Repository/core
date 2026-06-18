#include <pep/assessor/participanteditor.hpp>
#include <pep/assessor/InputValidationTooltip.hpp>
#include <pep/assessor/ui_participanteditor.h>
#include <pep/assessor/QDate.hpp>
#include <pep/content/Date.hpp>

#include <QRegularExpression>
#include <QRegularExpressionValidator>

ParticipantEditor::ParticipantEditor(QWidget* parent)
  : QWidget(parent), ui_(new Ui::ParticipantEditor) {
  ui_->setupUi(this);
  ui_->retranslateUi(this);

  ui_->firstnameInput->setAttribute(Qt::WA_MacShowFocusRect, false);
  ui_->tussenvoegselsInput->setAttribute(Qt::WA_MacShowFocusRect, false);
  ui_->lastnameInput->setAttribute(Qt::WA_MacShowFocusRect, false);
  ui_->dateOfBirthInput->setAttribute(Qt::WA_MacShowFocusRect, false);

  QRegularExpression nonEmptyRegEx(".+");

  ui_->firstnameInput->setValidator(new QRegularExpressionValidator(nonEmptyRegEx, ui_->firstnameInput));
  ui_->lastnameInput->setValidator(new QRegularExpressionValidator(nonEmptyRegEx, ui_->lastnameInput));

  auto inputValidator = [this]() {
    ui_->confirmButton->setEnabled(ui_->dateOfBirthInput->hasAcceptableInput()
                                  && ui_->firstnameInput->hasAcceptableInput()
                                  && ui_->lastnameInput->hasAcceptableInput()
                                 );
  };

  SetInputValidationTooltip(ui_->firstnameInput, tr("firstname-tooltip"));
  SetInputValidationTooltip(ui_->lastnameInput, tr("lastname-tooltip"));

  /* Show the placeholder for the "tussenvoegsel" text box in a tooltip, since the text box isn't wide enough.
   * We'd like to do this dynamically when the placeholder is too wide, but a QLineEdit's net width
   * 1. is complicated to calculate, and
   * 2. depends on hidden values.
   * See https://stackoverflow.com/a/23103682 and the QT code it links to at https://code.qt.io/cgit/qt/qtbase.git/tree/src/widgets/widgets/qlineedit.cpp?h=v5.13.0#n1933
   */
  ui_->tussenvoegselsInput->setToolTip(ui_->tussenvoegselsInput->placeholderText());

  QObject::connect(ui_->dateOfBirthInput, &DateEditor::valueChanged, this, inputValidator);
  QObject::connect(ui_->firstnameInput, &QLineEdit::textChanged, this, inputValidator);
  QObject::connect(ui_->lastnameInput, &QLineEdit::textChanged, this, inputValidator);

  QObject::connect(ui_->cancelButton, &QPushButton::clicked, this, &ParticipantEditor::cancelled);
  QObject::connect(ui_->confirmButton, &QPushButton::clicked, this, &ParticipantEditor::confirmed);
}

/*! \brief Destructor
 *
 * Clears out the UI object.
 */
ParticipantEditor::~ParticipantEditor() {
  delete ui_;
}

pep::ParticipantPersonalia ParticipantEditor::getPersonalia() const {
  pep::ParticipantPersonalia result(
    ui_->firstnameInput->text().toStdString(),
    ui_->tussenvoegselsInput->text().toStdString(),
    ui_->lastnameInput->text().toStdString(),
    pep::ToDdMonthAbbrevYyyyDate(pep::QDateToStd(ui_->dateOfBirthInput->getValue())));

  return result;
}

void ParticipantEditor::setPersonalia(const pep::ParticipantPersonalia& data) {
  ui_->firstnameInput->setText(QString::fromStdString(data.getFirstName()));
  ui_->tussenvoegselsInput->setText(QString::fromStdString(data.getMiddleName()));
  ui_->lastnameInput->setText(QString::fromStdString(data.getLastName()));
  ui_->dateOfBirthInput->setValue(pep::QDateFromStd(pep::ParticipantPersonalia::ParseDateOfBirth(data.getDateOfBirth())));
}

bool ParticipantEditor::getIsTestParticipant() const {
  return ui_->isTestParticipant->isChecked();
}

void ParticipantEditor::setIsTestParticipant(bool isTest) {
  ui_->isTestParticipant->setChecked(isTest);
}

void ParticipantEditor::doFocus() {
  ui_->firstnameInput->setFocus();
}
