#pragma once

#include <pep/assessor/UserRole.hpp>
#include <pep/assessor/ExportableItem.hpp>
#include <pep/assessor/VisitCaptions.hpp>
#include <pep/core-client/CoreClient_fwd.hpp>
#include <pep/utils/Log.hpp>

#include <QListWidget>
#include <QWidget>

#include <rxcpp/rx-lite.hpp>

namespace Ui {
class ExportWidget;
}

class ExportWidget : public QWidget
{
  using ExportDataTableIterator = std::vector<ExportDataTable>::const_iterator;
  Q_OBJECT

public:
  ExportWidget(const pep::GlobalConfiguration& configuration, const pep::StudyContext& studyContext, const pep::UserRole& role, VisitCaptionsByContext visitCaptionsByContext, const std::shared_ptr<pep::CoreClient> client, QWidget* parent = nullptr);
  ~ExportWidget() override;

  void doFocus();

signals:
  void sendMessage(QString message, pep::severity_level severity);

private slots:
  void on_selectedItemChanged();
  void on_itemChanged(QListWidgetItem* item);
  void on_exportButton_clicked();
  void on_itemActivated(QListWidgetItem* item);

private:
  std::vector<std::shared_ptr<ExportableItem>> getAllExportableItems(const pep::GlobalConfiguration& configuration, const pep::StudyContext& studyContext);
  rxcpp::observable<std::map<std::string, std::string>> getParticipantData(const QList<std::shared_ptr<ExportableItem>>& items);

  QString createCaption(const std::shared_ptr<ExportableItem> item);
  QString getVisitCaption(unsigned visitNumber);
  std::string getExportFilename(const QList<std::shared_ptr<ExportableItem>>& items);

  QList<std::shared_ptr<ExportableItem>> getSelectedItems() const;
  void updateSelectionState();

  void doExport(); 

  static void WriteParticipantData(const QList<std::shared_ptr<ExportableItem>>& exportableItems, const std::map<std::string, std::string>& data, std::ostream& destination, bool expandDetails);
  static void WriteParticipantDataCartesian(ExportDataTable& destination, const ExportDataRow& parentData, ExportDataTableIterator own, ExportDataTableIterator end);
  static void WriteCartesianToDestination(std::ostream& destination, const ExportDataTable& cartesian);

private:
  Ui::ExportWidget* ui;
  std::vector<std::shared_ptr<ExportableItem>> mAllItems;
  std::shared_ptr<pep::CoreClient> mPepClient;
  const VisitCaptions mVisitCaptions;
  const pep::StudyContext mStudyContext;
  bool mMultiSelect = false;
};
