#include <pep/assessor/participanteditor.hpp>
#include <pep/assessor/InputValidationTooltip.hpp>
#include <pep/assessor/ui_participanteditor.h>
#include <pep/assessor/QDate.hpp>
#include <pep/content/Date.hpp>

#include <QRegularExpression>
#include <QRegularExpressionValidator>

ParticipantEditor::ParticipantEditor(QWidget* parent)
  : QWidget(parent), ui(new Ui::ParticipantEditor) {
  ui->setupUi(this);
  ui->retranslateUi(this);

  ui->firstnameInput->setAttribute(Qt::WA_MacShowFocusRect, false);
  ui->tussenvoegselsInput->setAttribute(Qt::WA_MacShowFocusRect, false);
  ui->lastnameInput->setAttribute(Qt::WA_MacShowFocusRect, false);
  ui->dateOfBirthInput->setAttribute(Qt::WA_MacShowFocusRect, false);

  QRegularExpression nonEmptyRegEx(".+");

  ui->firstnameInput->setValidator(new QRegularExpressionValidator(nonEmptyRegEx, ui->firstnameInput));
  ui->lastnameInput->setValidator(new QRegularExpressionValidator(nonEmptyRegEx, ui->lastnameInput));

  auto inputValidator = [this]() {
    ui->confirmButton->setEnabled(ui->dateOfBirthInput->hasAcceptableInput()
                                  && ui->firstnameInput->hasAcceptableInput()
                                  && ui->lastnameInput->hasAcceptableInput()
                                 );
  };

  SetInputValidationTooltip(ui->firstnameInput, tr("firstname-tooltip"));
  SetInputValidationTooltip(ui->lastnameInput, tr("lastname-tooltip"));

  /* Show the placeholder for the "tussenvoegsel" text box in a tooltip, since the text box isn't wide enough.
   * We'd like to do this dynamically when the placeholder is too wide, but a QLineEdit's net width
   * 1. is complicated to calculate, and
   * 2. depends on hidden values.
   * See https://stackoverflow.com/a/23103682 and the QT code it links to at https://code.qt.io/cgit/qt/qtbase.git/tree/src/widgets/widgets/qlineedit.cpp?h=v5.13.0#n1933
   */
  ui->tussenvoegselsInput->setToolTip(ui->tussenvoegselsInput->placeholderText());

  QObject::connect(ui->dateOfBirthInput, &DateEditor::valueChanged, this, inputValidator);
  QObject::connect(ui->firstnameInput, &QLineEdit::textChanged, this, inputValidator);
  QObject::connect(ui->lastnameInput, &QLineEdit::textChanged, this, inputValidator);

  QObject::connect(ui->cancelButton, &QPushButton::clicked, this, &ParticipantEditor::cancelled);
  QObject::connect(ui->confirmButton, &QPushButton::clicked, this, &ParticipantEditor::confirmed);
}

/*! \brief Destructor
 *
 * Clears out the UI object.
 */
ParticipantEditor::~ParticipantEditor() {
  delete ui;
}

pep::ParticipantPersonalia ParticipantEditor::getPersonalia() const {
  pep::ParticipantPersonalia result(
    ui->firstnameInput->text().toStdString(),
    ui->tussenvoegselsInput->text().toStdString(),
    ui->lastnameInput->text().toStdString(),
    pep::ToDdMonthAbbrevYyyyDate(pep::QDateToStd(ui->dateOfBirthInput->getValue())));

  return result;
}

void ParticipantEditor::setPersonalia(const pep::ParticipantPersonalia& data) {
  ui->firstnameInput->setText(QString::fromStdString(data.getFirstName()));
  ui->tussenvoegselsInput->setText(QString::fromStdString(data.getMiddleName()));
  ui->lastnameInput->setText(QString::fromStdString(data.getLastName()));
  ui->dateOfBirthInput->setValue(pep::QDateFromStd(pep::ParticipantPersonalia::ParseDateOfBirth(data.getDateOfBirth())));
}

bool ParticipantEditor::getIsTestParticipant() const {
  return ui->isTestParticipant->isChecked();
}

void ParticipantEditor::setIsTestParticipant(bool isTest) {
  ui->isTestParticipant->setChecked(isTest);
}

void ParticipantEditor::doFocus() {
  ui->firstnameInput->setFocus();
}
