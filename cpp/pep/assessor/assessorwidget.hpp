#pragma once

#include <pep/structure/GlobalConfiguration.hpp>

#include <QWidget>

namespace Ui {
class AssessorWidget;
}

class AssessorWidget : public QWidget
{
  Q_OBJECT

public:
  explicit AssessorWidget(QWidget *parent = nullptr);
  ~AssessorWidget() override;

  void setAssessors(const std::vector<pep::AssessorDefinition>& assessors, const pep::StudyContext& studyContext);
  void setCurrentAssessor(const std::optional<unsigned>& id);

signals:
  void updateIssued(QString selected);

private slots:
  void onAssessorComboIndexChanged(int newIndex);

private:
  void enableDisableUpdateButton();
  void closeWidget();

private:
  std::vector<pep::AssessorDefinition> mAssessors;
  int mStoredAssessorIndex = 0;
  Ui::AssessorWidget *ui;
};
