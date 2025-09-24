#pragma once

#include <pep/content/ParticipantPersonalia.hpp>

#include <QWidget>

namespace Ui {
class ParticipantEditor;
}

class MainWindow;

class ParticipantEditor : public QWidget {
  Q_OBJECT

 public:
  explicit ParticipantEditor(QWidget* parent);
  ~ParticipantEditor() override;

  pep::ParticipantPersonalia getPersonalia() const;
  void setPersonalia(const pep::ParticipantPersonalia& data);

  bool getIsTestParticipant() const;
  void setIsTestParticipant(bool isTest);

  void doFocus();

 signals:
  void cancelled();
  void confirmed();

 private:
  Ui::ParticipantEditor* ui;
  QString participantFirstName;
  QString participantTussenvoegsels;
  QString participantLastName;
  QString participantDOB;
};
