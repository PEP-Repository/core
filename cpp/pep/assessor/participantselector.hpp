#pragma once

#include <pep/structure/GlobalConfiguration.hpp>

#include <QWidget>

namespace Ui {
class ParticipantSelector;
}

class ParticipantSelector : public QWidget {
  Q_OBJECT

 public:
  explicit ParticipantSelector(QWidget* parent, const pep::GlobalConfiguration& config);
  ~ParticipantSelector() override;

  void doFocus();

 signals:
  void cancelled();
  void participantSidSelected(std::string pepId);
  void participantShortPseudonymSelected(std::string shortPseudonym);

 private:
  Ui::ParticipantSelector* ui;
};
