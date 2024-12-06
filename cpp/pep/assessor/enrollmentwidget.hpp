#pragma once

#include <QPushButton>
#include <QStackedWidget>
#include <thread>
#include <pep/client/Client.hpp>
#include <pep/utils/Log.hpp>
#include <pep/structure/StudyContext.hpp>
#include <pep/assessor/participanteditor.hpp>

namespace Ui {
class EnrollmentWidget;
}

class MainWindow;

class EnrollmentWidget : public QStackedWidget {
  Q_OBJECT

 public:
  explicit EnrollmentWidget(std::shared_ptr<pep::Client> pepClient, MainWindow* parent, const pep::StudyContext& studyContext);
  ~EnrollmentWidget();

  void doFocus();

 signals:
  void cancelled();
  void enrollConfirmed(std::string pepID);
  void enrollComplete(std::string pepID);
  void enrollFailed(QString enrollmentError, pep::severity_level);
  void participantRegistered(std::shared_ptr<pep::ParticipantPersonalia> personalia);
  void registrationProceeding();

 private slots:
  void showRegisteredParticipant(std::shared_ptr<pep::ParticipantPersonalia> personalia);
  void showEditor();
  void onRegistrationProceeding();

 private:
  Ui::EnrollmentWidget* ui;
  MainWindow* mainWindow;
  std::shared_ptr<pep::Client> pepClient;
  pep::StudyContext mStudyContext;
  QString participantSID;
  rxcpp::composite_subscription registerParticipantSubscription;
  rxcpp::composite_subscription completeParticipantRegistrationSubscription;
  bool doneCompletingRegistration;
  bool continueButtonPressed;
};
