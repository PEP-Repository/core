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
  : QStackedWidget(parent), ui_(new Ui::EnrollmentWidget), mainWindow_(parent), pepClient_(client), studyContext_(studyContext), doneCompletingRegistration_(false), continueButtonPressed_(false) {
  ui_->setupUi(this);
  ui_->retranslateUi(this);

  // Allow types to be passed as arguments in signal->slot
  qRegisterMetaType<std::shared_ptr<pep::ParticipantPersonalia>>("std::shared_ptr<pep::ParticipantPersonalia>");

  QObject::connect(this, &EnrollmentWidget::participantRegistered, this, &EnrollmentWidget::showRegisteredParticipant);
  QObject::connect(this, &EnrollmentWidget::registrationProceeding, this, &EnrollmentWidget::onRegistrationProceeding);


  QObject::connect(ui_->editor, &ParticipantEditor::cancelled, this, &EnrollmentWidget::cancelled);
  QObject::connect(ui_->editor, &ParticipantEditor::confirmed, [this]() {
    auto personalia = std::make_shared<pep::ParticipantPersonalia>(ui_->editor->getPersonalia());
    auto isTest = ui_->editor->getIsTestParticipant();

    setCurrentIndex(1);

    participantSID_ = QString("");
    registerParticipantSubscription_ = pepClient_->registerParticipant(*personalia, isTest, studyContext_.getIdIfNonDefault(), false)
    .subscribe(
    [this](const std::string &id) {
      participantSID_ = QString::fromStdString(id);
    },
    [this](std::exception_ptr ep) {
      emit enrollFailed(QString::fromStdString(pep::GetExceptionMessage(ep)), pep::Severity::Error);
    },
    [this, personalia]() {
      if (participantSID_ == QString("")) {
        emit enrollFailed(tr("Generated duplicate participant identifier. Please try again."), pep::Severity::Error);
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

  confirmUi.pepIdField->setText(participantSID_);
  confirmUi.participantNameField->setText(QString::fromStdString(personalia->getFullName()));
  confirmUi.dateOfBirthField->setText(QString::fromStdString(personalia->getDateOfBirth()));

  QObject::connect(confirmUi.copyButton, &QPushButton::clicked, [this, confirmWidget]() {
    QApplication::clipboard()->setText(participantSID_);
    confirmWidget->findChild<QPushButton*>("continueButton")->setEnabled(true);
  });
  QObject::connect(confirmUi.continueButton, &QPushButton::clicked, [this]() {
    continueButtonPressed_ = true;
    emit registrationProceeding();
  });


  completeParticipantRegistrationSubscription_ = pepClient_->completeParticipantRegistration(participantSID_.toStdString(), true)
  .subscribe(
  [](pep::FakeVoid) {},
  [this](std::exception_ptr ep) {
    auto message = QString::fromStdString(pep::GetExceptionMessage(ep));
    if (message.isEmpty()) {
      message = tr("Completing registration failed.");
    }
    emit enrollFailed(message, pep::Severity::Error);
    doneCompletingRegistration_ = true;
    emit registrationProceeding();
  },
  [this]() {
    doneCompletingRegistration_ = true;
    emit registrationProceeding();
  });

  mainWindow_->showRegistrationWidget(confirmWidget);
}

void EnrollmentWidget::onRegistrationProceeding() {
  if (continueButtonPressed_) {
    emit enrollConfirmed(participantSID_.toStdString());
  }
  if (doneCompletingRegistration_ && continueButtonPressed_) {
    mainWindow_->closeWidget(this);
    emit enrollComplete(participantSID_.toStdString());
  }
}

/*! \brief Destructor
 *
 * Clears out the UI object.
 */
EnrollmentWidget::~EnrollmentWidget() {
  completeParticipantRegistrationSubscription_.unsubscribe();
  registerParticipantSubscription_.unsubscribe();
  delete ui_;
}

/*! \brief Set UI focus to the personalia editor
 */
void EnrollmentWidget::doFocus() {
  ui_->editor->doFocus();
}
