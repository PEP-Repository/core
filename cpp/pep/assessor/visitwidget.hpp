#pragma once

#include <pep/structure/GlobalConfiguration.hpp>
#include <pep/assessor/UserRole.hpp>

#include <QHBoxLayout>
#include <QPushButton>
#include <QWidget>
#include <QLabel>
#include <pep/assessor/ButtonBar.hpp>

namespace Ui {
class VisitWidget;
}

class VisitWidget : public QWidget
{
  Q_OBJECT

public:
  VisitWidget(const std::vector<pep::AssessorDefinition>& assessors, pep::UserRole& currentPEPRole, pep::StudyContext& studyContext, QWidget *parent);
  ~VisitWidget() override;

  void disablePrinting();
  void disableAssessorSelection();

  QLabel& getPseudonymButtonCaption();
  ButtonBar& getPseudonymButtonBar();
  QSpacerItem& getPseudonymButtonSpacer();

  QLabel& getPseudonymCaption();
  QLabel& getPseudonymLabel();

  QSpacerItem& getPseudonymSpacerForOtherVisits();
  QLabel& getPseudonymCaptionForOtherVisits();
  QLabel& getPseudonymLabelForOtherVisits();

  QPushButton& getPrintAllButton();
  QPushButton& getPrintOneButton();

  void setCurrentAssessor(const std::optional<unsigned>& id);

  void openEditAssessor();

signals:
  void updateVisitAssessor(QString id);
  void printAllStickers();
  void printSingleSticker();
  void printSummary();
  void locateBartender();

private:
  Ui::VisitWidget *ui;
  ButtonBar* print_buttons;
  QPushButton* print_stickers_button; 
  QPushButton* print_oneSticker_button; 
  ButtonBar* data_castor_buttons;
  
  const std::vector<pep::AssessorDefinition>& assessors;
  pep::UserRole& currentPEPRole;
  pep::StudyContext& studyContext;
  std::optional<unsigned> currentAssessorId;
};
