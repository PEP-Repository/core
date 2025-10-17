#include <pep/assessor/exportwidget.hpp>
#include <pep/assessor/ui_exportwidget.h>
#include <pep/async/RxMoveIterate.hpp>
#include <pep/core-client/CoreClient.hpp>
#include <pep/utils/Exceptions.hpp>

#include <cassert>
#include <fstream>
#include <QFileDialog>
#include <QStandardPaths>
#include <pep/gui/QTrxGui.hpp>
#include <pep/utils/Log.hpp>

#include <boost/algorithm/string.hpp>

#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/operators/rx-reduce.hpp>
#include <rxcpp/operators/rx-map.hpp>
#include <rxcpp/operators/rx-filter.hpp>

namespace {

const QString ALL_FILES_WILDCARD =
#ifdef _WIN32
"*.*"
#else
"*"
#endif
;

void SortAndInsert(std::vector<std::shared_ptr<ExportableItem>>& dest, std::vector<std::shared_ptr<ExportableShortPseudonymItem>>& source) {
  std::sort(source.begin(), source.end(), [](const std::shared_ptr<ExportableItem>& lhs, const std::shared_ptr<ExportableItem>& rhs) { // Sort SPs to make them easier to find in the UI
    return lhs->getDescription() < rhs->getDescription();
            });
  dest.insert(dest.end(), source.cbegin(), source.cend());
}

}

void ExportWidget::WriteParticipantData(const QList<std::shared_ptr<ExportableItem>>& exportableItems, const std::map<std::string, std::string>& data, std::ostream& destination, bool expandDetails) {
  auto columnUnfound = data.cend();

  std::vector<ExportDataTable> itemTables;
  for (const auto& exportableItem : exportableItems) {
    // Find the participant's raw value for this item
    std::optional<std::string> cellContent;
    auto entry = data.find(exportableItem->getSourceColumnName());
    if (entry != columnUnfound) {
      cellContent = entry->second;
    }

    // Get the (tabular) data for this item
    auto& table = itemTables.emplace_back();
    auto expander = exportableItem->getDetailExpander();
    if (expandDetails && expander) {
      (*expander)(table, cellContent);
      assert(!table.empty());
      assert(std::find_if(table.cbegin(), table.cend(), [](const ExportDataRow& row) {return row.empty(); }) == table.cend());
    }
    else {
      auto& row = table.emplace_back();
      row.emplace_back(cellContent);
    }
  }

  // Fill a single table with cartesian product of item data
  ExportDataTable cartesian;
  WriteParticipantDataCartesian(cartesian, ExportDataRow(), itemTables.cbegin(), itemTables.cend());
  WriteCartesianToDestination(destination, cartesian);
}

void ExportWidget::WriteParticipantDataCartesian(ExportDataTable& destination, const ExportDataRow& parentData, ExportDataTableIterator own, ExportDataTableIterator end) {
  if (own == end) {
    if (!parentData.empty()) {
      destination.push_back(parentData);
    }
  }
  else {
    for (const auto& row : *own) {
      ExportDataRow values(parentData);
      values.insert(values.cend(), row.cbegin(), row.cend());
      WriteParticipantDataCartesian(destination, values, own + 1, end);
    }
  }
}

void ExportWidget::WriteCartesianToDestination(std::ostream& destination, const ExportDataTable& cartesian) {

  for (const auto& row : cartesian) {
    std::string prefix{""}; //Explicitly emptily initialised so the first time it is added to the stream, an empty string is written.

    for (const auto& cell : row) {
      // Announce new column
      destination << prefix;
      prefix = ",";

      if (cell) {
        const auto& cellContent = *cell;

        // Escape value if needed
        auto escape = std::find_if(cellContent.begin(), cellContent.end(), [](char c) {
          return (c == '"') || (c == ',');
                                   });
        if (escape != cellContent.end()) {
          destination << '"' << boost::replace_all_copy(cellContent, "\"", "\"\"") << '"';
        }
        else {
          destination << cellContent;
        }
      }
    }

    // Force CRLF line breaks, which are required according to RFC 4180
#ifdef _WIN32
  // Microsoft C++ runtime already produces CR+LF when streaming "\n", so we don't stream "\r\n" to prevent the output from containing CR+CR+LF.
    destination << "\n";
#else
    destination << "\r\n";
#endif
  }
}


ExportWidget::ExportWidget(const pep::GlobalConfiguration& configuration, const pep::StudyContext& studyContext, const pep::UserRole& role, VisitCaptionsByContext visitCaptionsByContext, std::shared_ptr<pep::CoreClient> client, QWidget* parent) :
  QWidget(parent),
  ui(new Ui::ExportWidget),
  mStudyContext(studyContext),
  mMultiSelect(role.canCrossTabulate())
{
  mPepClient = client;
  mAllItems = this->getAllExportableItems(configuration, studyContext);

  ui->setupUi(this);

  //NOLINTNEXTLINE(clang-analyzer-cplusplus.NewDeleteLeaks) QListWidget takes ownership of QListWidgetItem
  for (const auto& item : mAllItems) {
    auto caption = createCaption(item);
    auto listItem = new QListWidgetItem(caption, ui->listWidget);
    if (mMultiSelect) {
      listItem->setFlags(listItem->flags() | Qt::ItemIsUserCheckable);
      listItem->setCheckState(Qt::Unchecked);
    }
  }

  if (!mMultiSelect) {
    QObject::connect(ui->listWidget, &QListWidget::itemSelectionChanged, this, &ExportWidget::on_selectedItemChanged);
    QObject::connect(ui->listWidget, &QListWidget::itemActivated, this, &ExportWidget::on_itemActivated);
  }
  else {
    QObject::connect(ui->listWidget, &QListWidget::itemChanged, this, &ExportWidget::on_itemChanged);
  }
}

ExportWidget::~ExportWidget()
{
  delete ui;
}

void ExportWidget::doFocus() {
  ui->listWidget->setFocus();
}


std::vector<std::shared_ptr<ExportableItem>> ExportWidget::getAllExportableItems(const pep::GlobalConfiguration& configuration, const pep::StudyContext& studyContext) {
  std::vector<std::shared_ptr<ExportableItem>> entries;

  // Participant identifier
  entries.insert(entries.begin(), std::make_shared<ExportableParticipantIdentifierItem>());

  // Device history columns
  for (const auto& def : configuration.getDevices())
  {
    if (studyContext.matches(def.studyContext)) {
      std::optional<std::string> description = def.description.empty() ? std::nullopt : std::optional<std::string>(def.description);
      entries.push_back(std::make_shared<ExportableDeviceHistoryItem>(def.columnName, description));
    }
  }

  // Short pseudonyms
  std::vector<std::shared_ptr<ExportableShortPseudonymItem>> singleSps;
  std::vector<std::shared_ptr<ExportableShortPseudonymItem>> visitSps;
  for (auto sp : configuration.getShortPseudonyms()) {
    if (studyContext.matchesShortPseudonym(sp)) {
      auto item = std::make_shared<ExportableShortPseudonymItem>(sp);

      if (item->getVisitNumber().has_value()) {
        visitSps.push_back(item);
      }
      else {
        singleSps.push_back(item);
      }
    }
  }
  SortAndInsert(entries, singleSps);
  SortAndInsert(entries, visitSps);

  // Visit Assessors
  const auto& assessorColumns = configuration.getVisitAssessorColumns(studyContext);
  for (unsigned int i{ 0 }; i < assessorColumns.size(); i++) {
    const auto& column = assessorColumns[i];
    const auto& visitNumber = i + 1; // i + 1 keeps this in line with .getColumn().getVisitNumber().
    entries.push_back(std::make_shared<ExportableVisitAssessorItem>(column, visitNumber));
  }

  return entries;
}

QString ExportWidget::createCaption(const std::shared_ptr<ExportableItem> item) {
  auto prefix = item->getCaptionPrefix();
  auto description = item->getDescription();
  auto visitNumber = item->getVisitNumber();


  QString result = tr(prefix.c_str());
  if (description.has_value()) {
    result += " - " + QString::fromStdString(description.value());
  }
  if (visitNumber.has_value()) {
    result += " - " + this->getVisitCaption(visitNumber.value());
  }
  return result;
}

QString ExportWidget::getVisitCaption(const unsigned visitNumber) {
  if (visitNumber < 1) {
    throw std::runtime_error("Please provide a 1-based visit number (as opposed to a 0-based index)");
  }
  auto index = visitNumber - 1;
  if (index < mVisitCaptions.size()) {
    return QString::fromStdString(mVisitCaptions.at(index));
  }
  return tr("Visit %1").arg(visitNumber);
}


void ExportWidget::on_selectedItemChanged() {
  this->updateSelectionState();
}

void ExportWidget::on_itemChanged(QListWidgetItem* item) {
  this->updateSelectionState();
}

void ExportWidget::updateSelectionState() {
  auto selected = getSelectedItems();
  ui->exportButton->setEnabled(!selected.empty());
  ui->expandDetailsCheckBox->setEnabled(std::find_if(selected.cbegin(), selected.cend(), [](const std::shared_ptr<ExportableItem>& item) {return item->getDetailExpander(); }) != selected.cend());
}

QList<std::shared_ptr<ExportableItem>> ExportWidget::getSelectedItems() const {
  QList<std::shared_ptr<ExportableItem>> result;

  if (mMultiSelect) { // Selection depends on each item's check state, which must be inspected individually: see https://stackoverflow.com/a/29240727
    for (auto i = 0; i < ui->listWidget->count(); ++i) {
      if (ui->listWidget->item(i)->checkState() == Qt::Checked) {
        result.push_back(mAllItems[static_cast<unsigned>(i)]);
      }
    }
  }
  else { // Selection depends on highlight
    auto row = ui->listWidget->currentRow();
    if (row >= 0) {
      result.push_back(mAllItems[static_cast<size_t>(row)]);
    }
  }

  return result;
}


void ExportWidget::on_itemActivated(QListWidgetItem* item) {
  this->doExport();
}

void ExportWidget::on_exportButton_clicked() {
  this->doExport();
}

std::string ExportWidget::getExportFilename(const QList<std::shared_ptr<ExportableItem>>& items) {
  QString caption;
  switch (items.size()) {
  case 0:
    emit this->sendMessage(tr("Export failed: %1").arg(tr("No items are selected for export")), pep::error);
    return {};
  case 1:
    caption = this->createCaption(*items.cbegin());
    break;
  default:
    caption = "Participant data";
    break;
  }

  auto fileName = QFileDialog::getSaveFileName(this,
    tr("Export %1").arg(caption),
    QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QDir::separator() + caption + ".csv",
    tr("Comma-separated values (*.csv);;All Files (%1)").arg(ALL_FILES_WILDCARD));
  return fileName.toStdString();
}

void ExportWidget::doExport() {
  auto selected = this->getSelectedItems();
  auto fileName = this->getExportFilename(selected);
  if (fileName.empty()) { // If the dialog box asking for a fileName is cancelled, fileName will be empty. If so, do nothing.
    return;
  }
  try {
    auto expandDetails = ui->expandDetailsCheckBox->isChecked();
    auto file = std::make_shared<std::ofstream>();
    file->open(fileName);

    if (!file->is_open()) {
      emit this->sendMessage(tr("Export failed: %1").arg(tr("Could not open file for writing")), pep::error);
      return;
    }

    this->getParticipantData(selected)
      .observe_on(observe_on_gui())
      .subscribe(
        [entries = std::make_shared<QList<std::shared_ptr<ExportableItem>>>(selected), file, expandDetails](const std::map<std::string, std::string>& data) {WriteParticipantData(*entries, data, *file, expandDetails); },
        [this, file](std::exception_ptr ep) {
          emit this->sendMessage(tr("Export failed: %1").arg(QString::fromStdString(pep::GetExceptionMessage(ep))), pep::error);
    file->close();
        },
        [this, file]() {
          emit this->sendMessage(tr("Data exported"), pep::info);
        file->close();
        }
        );
  }
  catch (const std::exception& e) {
    emit this->sendMessage(tr("Export failed: %1").arg(e.what()), pep::error);
  }
}

rxcpp::observable<std::map<std::string, std::string>> ExportWidget::getParticipantData(const QList<std::shared_ptr<ExportableItem>>& items) {
  pep::enumerateAndRetrieveData2Opts opts;
  opts.groups = { "*" };
  opts.columns = { "StudyContexts" };
  for (const auto& item : items) {
    opts.columns.push_back(item->getSourceColumnName());
  }

  using ParticipantData = std::map<std::string, std::string>;
  return mPepClient->enumerateAndRetrieveData2(opts) // Get study contexts, plus values for all requested columns
      .reduce( // Associate participant indices with values for that participant
          std::make_shared<std::unordered_map<uint32_t, ParticipantData>>(),
          [](std::shared_ptr<std::unordered_map<uint32_t, ParticipantData>> entries,
          const pep::EnumerateAndRetrieveResult& result) {
            (*entries)[result.mLocalPseudonymsIndex][result.mColumn] = result.mData;
            return entries;
          })
      // Convert observable<std::unordered_map<entry>> to observable<entry>
      .flat_map([](const std::shared_ptr<std::unordered_map<uint32_t, ParticipantData>>& entries) {
        return pep::RxMoveIterate(std::move(*entries));
      })
      // Convert to std::nullopt for participants that don't match the user's context
      .map([this](std::pair<const uint32_t, ParticipantData> entry) -> std::optional<ParticipantData> {
        if (!mStudyContext.matches(entry.second["StudyContexts"])) {
          return std::nullopt;
        }
        entry.second.erase("StudyContexts");
        return std::move(entry.second);
      })
      // Exclude (std::nullopt) entries for participants that didn't match the user's context
      .filter([](const std::optional<ParticipantData>& data) { return data.has_value(); })
      // Convert optional<ParticipantData> to ParticipantData
      .map([](std::optional<ParticipantData> optional) { return std::move(*optional); });
}
