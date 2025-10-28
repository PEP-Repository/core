#include <pep/assessor/enrollmentwidget.hpp>
#include <pep/assessor/InputValidationTooltip.hpp>
#include <pep/assessor/ui_enrollmentwidget.h>
#include <pep/assessor/ui_confirmenrollmentwidget.h>
#include <pep/assessor/UserRole.hpp>
#include <pep/gui/QTrxGui.hpp>
#include <pep/assessor/mainwindow.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/client/Client.hpp>

#include <QGridLayout>
#include <QClipboard>
#include <QDialog>

#include <boost/property_tree/json_parser.hpp>

#include <rxcpp/operators/rx-delay.hpp>
#include <rxcpp/operators/rx-take_until.hpp>

#include <pep/versioning/Version.hpp>

EnrollmentWidget::EnrollmentWidget(std::shared_ptr<pep::Client> client, MainWindow* parent, const pep::StudyContext& studyContext)
  : QStackedWidget(parent), ui(new Ui::EnrollmentWidget), mainWindow(parent), pepClient(client), mStudyContext(studyContext), doneCompletingRegistration(false), continueButtonPressed(false) {
  ui->setupUi(this);
  ui->retranslateUi(this);

  // Allow types to be passed as arguments in signal->slot
  qRegisterMetaType<std::shared_ptr<pep::ParticipantPersonalia>>("std::shared_ptr<pep::ParticipantPersonalia>");

  QObject::connect(this, &EnrollmentWidget::participantRegistered, this, &EnrollmentWidget::showRegisteredParticipant);
  QObject::connect(this, &EnrollmentWidget::registrationProceeding, this, &EnrollmentWidget::onRegistrationProceeding);


  QObject::connect(ui->editor, &ParticipantEditor::cancelled, this, &EnrollmentWidget::cancelled);
  QObject::connect(ui->editor, &ParticipantEditor::confirmed, [this]() {
    auto personalia = std::make_shared<pep::ParticipantPersonalia>(ui->editor->getPersonalia());
    auto isTest = ui->editor->getIsTestParticipant();

    setCurrentIndex(1);

    participantSID = QString("");
    registerParticipantSubscription = pepClient->registerParticipant(*personalia, isTest, mStudyContext.getIdIfNonDefault(), false)
    .subscribe(
    [this](const std::string &id) {
      participantSID = QString::fromStdString(id);
    },
    [this](std::exception_ptr ep) {
      emit enrollFailed(QString::fromStdString(pep::GetExceptionMessage(ep)), pep::error);
    },
    [this, personalia]() {
      if (participantSID == QString("")) {
        emit enrollFailed(tr("Generated duplicate participant identifier. Please try again."), pep::error);
        return;
      }
      emit participantRegistered(personalia);
    }
    );
  });

  QObject::connect(this, &EnrollmentWidget::enrollConfirmed, this, &EnrollmentWidget::showEditor);
  QObject::connect(this, &EnrollmentWidget::enrollFailed, this, &EnrollmentWidget::showEditor);
}

void EnrollmentWidget::showEditor() {
  setCurrentIndex(0);
}

/*! \brief Confirms user input with the user
 */
void EnrollmentWidget::showRegisteredParticipant(std::shared_ptr<pep::ParticipantPersonalia> personalia) {
  QWidget* confirmWidget = new QWidget(this);
  Ui::ConfirmEnrollmentWidget confirmUi;
  confirmUi.setupUi(confirmWidget);

  confirmUi.pepIdField->setText(participantSID);
  confirmUi.participantNameField->setText(QString::fromStdString(personalia->getFullName()));
  confirmUi.dateOfBirthField->setText(QString::fromStdString(personalia->getDateOfBirth()));

  QObject::connect(confirmUi.copyButton, &QPushButton::clicked, [this, confirmWidget]() {
    QApplication::clipboard()->setText(participantSID);
    confirmWidget->findChild<QPushButton*>("continueButton")->setEnabled(true);
  });
  QObject::connect(confirmUi.continueButton, &QPushButton::clicked, [this]() {
    continueButtonPressed = true;
    emit registrationProceeding();
  });


  completeParticipantRegistrationSubscription = pepClient->completeParticipantRegistration(participantSID.toStdString(), true)
  .subscribe(
  [](pep::FakeVoid) {},
  [this](std::exception_ptr ep) {
    auto message = QString::fromStdString(pep::GetExceptionMessage(ep));
    if (message.isEmpty()) {
      message = tr("Completing registration failed.");
    }
    emit enrollFailed(message, pep::error);
    doneCompletingRegistration = true;
    emit registrationProceeding();
  },
  [this]() {
    doneCompletingRegistration = true;
    emit registrationProceeding();
  });

  mainWindow->showRegistrationWidget(confirmWidget);
}

void EnrollmentWidget::onRegistrationProceeding() {
  if (continueButtonPressed) {
    emit enrollConfirmed(participantSID.toStdString());
  }
  if (doneCompletingRegistration && continueButtonPressed) {
    mainWindow->closeWidget(this);
    emit enrollComplete(participantSID.toStdString());
  }
}

/*! \brief Destructor
 *
 * Clears out the UI object.
 */
EnrollmentWidget::~EnrollmentWidget() {
  completeParticipantRegistrationSubscription.unsubscribe();
  registerParticipantSubscription.unsubscribe();
  delete ui;
}

/*! \brief Set UI focus to the personalia editor
 */
void EnrollmentWidget::doFocus() {
  ui->editor->doFocus();
}
