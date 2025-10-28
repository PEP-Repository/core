#include <pep/assessor/participantwidget.hpp>
#include <pep/assessor/ui_participantwidget.h>

#include <pep/assessor/datetimeeditor.hpp>
#include <pep/assessor/participanteditor.hpp>
#include <pep/assessor/assessorwidget.hpp>

#include <pep/assessor/QDate.hpp>
#include <pep/client/Client.hpp>
#include <pep/utils/Defer.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/utils/Log.hpp>
#include <pep/utils/MiscUtil.hpp>
#include <pep/utils/Shared.hpp>
#include <pep/utils/Win32Api.hpp>
#include <pep/assessor/InputValidationTooltip.hpp>
#include <pep/gui/QTrxGui.hpp>

#include <boost/asio/time_traits.hpp>
#include <boost/format.hpp>
#include <boost/algorithm/string/find.hpp>

#include <QDateEdit>
#include <QDebug>
#include <QDesktopServices>
#include <QFileDialog>
#include <QFormLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPrintDialog>
#include <QPrinter>
#include <QRegularExpression>
#include <QSettings>
#include <QTextDocument>
#include <QString>
#include <QScrollBar>

#ifdef _WIN32
#include <fstream>
#include <Windows.h>
#endif /*_WIN32*/

#define LOG_TAG "Participant widget"

namespace {
  class ParticipantDataAggregator {
  private:
    const pep::GlobalConfiguration mGlobalConfig;
    std::vector<const pep::ShortPseudonymDefinition *> mUnfilledShortPseudonyms;
    std::map<std::string, pep::EnumerateAndRetrieveResult> mDeviceHistory;
    std::optional<pep::EnumerateAndRetrieveResult> mParticipantInfo;
    std::optional<pep::EnumerateAndRetrieveResult> mIsTestParticipant;
    bool mParticipantIdentifierIsSet;
    std::map<std::string, std::string> mShortPseudonyms;
    std::string mStudyContexts;
    std::unordered_map<std::string, std::unordered_map<unsigned, std::optional<unsigned>>> mVisitAssessors;

  private:
    const pep::ShortPseudonymDefinition *getShortPseudonymDefinition(const std::string& shortPseudonymTag) const;

    void processDeviceHistory(const pep::EnumerateAndRetrieveResult& result);
    void processParticipantInfo(const pep::EnumerateAndRetrieveResult& result);
    void processIsTestParticipant(const pep::EnumerateAndRetrieveResult& result);
    void processParticipantIdentifier(const pep::EnumerateAndRetrieveResult& result);
    void processShortPseudonym(const pep::EnumerateAndRetrieveResult& result);
    void processStudyContexts(const pep::EnumerateAndRetrieveResult& result);
    void processVisitAssessor(const pep::EnumerateAndRetrieveResult& result);

    bool infoPseudonymsIsSet() const noexcept;

    bool isDeviceHistoryColumn(const std::string& columnName) const;
    bool isVisitAssessorColumn(const std::string& columnName, std::string* context = nullptr, unsigned *visitNumber = nullptr) const;

  public:
    ParticipantDataAggregator(const pep::GlobalConfiguration& globalConfig) noexcept;

    // Prevent copying, which would have the copy's mUnfilledShortPseudonyms point to the original's mShortPseudonymDefinitions
    ParticipantDataAggregator(const ParticipantDataAggregator&) = delete;
    ParticipantDataAggregator& operator =(const ParticipantDataAggregator&) = delete;

    void process(const pep::EnumerateAndRetrieveResult& result);

    bool hasParticipantData() const noexcept;
    bool hasCompleteParticipantData() const noexcept;

    ParticipantData getData() const;
    const std::string& getStudyContexts() const noexcept { return mStudyContexts; }
  };

  bool ContainsMultipleVisits(const std::vector<pep::ShortPseudonymDefinition>& sps) {
    std::optional<uint32_t> visit;
    for (const auto& sp : sps) {
      auto spVisit = sp.getColumn().getVisitNumber();
      if (spVisit.has_value()) {
        if (visit.value_or(*spVisit) != *spVisit) {
          return true;
        }
        visit = spVisit;
      }
    }

    return false;
  }

  std::filesystem::path StoreConfiguredBartenderPath(const std::filesystem::path& path) {
    QSettings().setValue("BartenderPath", QString::fromStdString(path.string()));
    return path;
  }

  std::optional<std::filesystem::path> ReadConfiguredBartenderPath(const std::optional<pep::Configuration>& configuration) {
    // If we've already stored it in local settings (in a previous run), return that value
    auto setting = QSettings().value("BartenderPath");
    if (!setting.isNull()) {
      return setting.toString().toStdString();
    }

    /* If configuration contained no "BartenderPath", we'd feed an unqualified "bartend.exe" into the
     * std::system call when we want to invoke it. Presumably the shell would then (be expected to) locate
     * the executable on the system's path. But
     * - we don't want to invoke the .exe here, and
     * - there does not seem to be a portable way to have C++ find the .exe on the path, and
     * - I don't think that many systems would (be configured to) have bartend.exe on their paths anyway.
     * So since finding an unqualified "bartend.exe" would be difficult and yield little benefit, we don't
     * use that anymore as a fallback.
     */

    return std::nullopt;
  }
}

const QString ParticipantWidget::NO_PARTICIPANT_SID = QString();

ParticipantWidget::ParticipantWidget(MainWindow* parent,
                                     std::shared_ptr<pep::Client> client,
                                     QString SID,
                                     const pep::Configuration& configuration,
                                     const pep::GlobalConfiguration& globalConfiguration,
                                     const pep::StudyContexts& allContexts,
                                     const Branding& branding,
                                     unsigned spareStickerCount,
                                     const pep::StudyContext& studyContext,
                                     const VisitCaptions* visitCaptions,
                                     pep::UserRole role)
  : QWidget(parent), pepClient(client), ui(new Ui::ParticipantWidget), mainWindow(parent), globalConfig(globalConfiguration), mAllContexts(allContexts), mStudyContext(studyContext), currentPEPRole(role), mProjectName(branding.getProjectName()), mSpareStickerCount(spareStickerCount), mVisitCaptions(visitCaptions) {
  baseUrl = QString::fromStdString(configuration.get<std::string>("Castor.BaseURL"));
  stickerFilePath = configuration.get<std::optional<std::filesystem::path>>("StickerFilePath").value_or(QCoreApplication::applicationDirPath().toStdString() + "/pepStickerTemplate.btw");
  bartenderPath = ReadConfiguredBartenderPath(configuration);
  currentUserPP = pepClient->generateParticipantPolymorphicPseudonym(SID.toStdString()); // TODO: accept as a parameter: most (or all?) callers will already have a PP
  participantSID = SID;

  setAttribute(Qt::WA_DeleteOnClose);

  QObject::connect(this, &ParticipantWidget::participantDataReceived, this, &ParticipantWidget::onParticipantDataReceived);

  ui->setupUi(this);
  ui->retranslateUi(this);

  auto hasDevice = false;

  for (const auto& deviceDefinition : globalConfig.getDevices()) {
    auto deviceWidget = new DeviceWidget(deviceDefinition, this);
    mDeviceWidgets.push_back(deviceWidget);
    ui->verticalLayout_devices->addWidget(deviceWidget);

    QObject::connect(deviceWidget, &DeviceWidget::deviceDeregistered, this, &ParticipantWidget::updateDevice);
    QObject::connect(deviceWidget, &DeviceWidget::deviceRegistered, this, &ParticipantWidget::updateDevice);

    auto historyWidget = new DeviceHistoryWidget(deviceDefinition, this);
    mDeviceHistoryWidgets.push_back(historyWidget);
    ui->verticalLayout_deviceHistories->addWidget(historyWidget);

    QObject::connect(historyWidget, &DeviceHistoryWidget::itemActivated, this, &ParticipantWidget::editDeviceHistoryEntry);
    if (mStudyContext.matches(deviceDefinition.studyContext)) {
      hasDevice = true;
    }
    else {
      deviceWidget->setVisible(false);
      historyWidget->setVisible(false);
    }
  }

  if (!hasDevice) {
    ui->devices_header->setVisible(false);
    ui->verticalSpacer_2->changeSize(0, 0);
    ui->tabWidget_left->removeTab(1);
  }

  //initializes a participant button bar
  participant_buttons = new ButtonBar(this);
  ui->participant_buttonBar_layout->addWidget(participant_buttons);

  edit_participant_button = participant_buttons->addButton(tr("edit-participant"), std::bind(&ParticipantWidget::openEditParticipant, this),  currentPEPRole.canEditParticipantPersonalia());
  release_participant_button = participant_buttons->addButton(tr("release-participant"), std::bind(&ParticipantWidget::releaseParticipant, this),  currentPEPRole.canSetParticipantContext());

  //initializes a ops castor button bar
  castor_buttons = new ButtonBar(this);
  ui->ops_castorButtonBar_layout->addWidget(castor_buttons);

  //initializes a print button bar
  print_buttons = new ButtonBar(this);
  ui->print_buttonBar_layout->addWidget(print_buttons);

  print_stickers_button = print_buttons->addButton(tr("print-stickers"), std::bind(&ParticipantWidget::printAllParticipantStickers, this), currentPEPRole.canPrintStickers());
  print_oneSticker_button = print_buttons->addButton(tr("print-one-sticker"), std::bind(&ParticipantWidget::printSingleParticipantSticker, this), currentPEPRole.canPrintStickers());
  print_buttons->addButton(tr("locate-bartender"), std::bind(&ParticipantWidget::locateBartender, this), currentPEPRole.canPrintStickers());

  QObject::connect(ui->tabWidget_right, SIGNAL(currentChanged(int)), this, SLOT(setCurrentVisitNumber(int))); // Track visit number for printing

  auto numberOfVisits = globalConfig.getNumberOfVisits(mStudyContext.getIdIfNonDefault());
  for (auto visitIndex = 0U; visitIndex < numberOfVisits; ++visitIndex) {
    auto visitWidget = new VisitWidget(globalConfig.getAssessors(), currentPEPRole, mStudyContext, this);
    mVisitWidgets.push_back(visitWidget);
    auto caption = getVisitCaption(visitIndex + 1).replace('&', "&&"); // Escape ampersands to prevent them from announcing a shortcut key / hotkey / mnemonic. See e.g. https://stackoverflow.com/a/16666135
    ui->tabWidget_right->addTab(visitWidget, caption);

    if (currentPEPRole.canPrintStickers()) {
      QObject::connect(visitWidget, &VisitWidget::printAllStickers, this, &ParticipantWidget::printAllVisitStickers);
      QObject::connect(visitWidget, &VisitWidget::printSingleSticker, this, &ParticipantWidget::printSingleVisitSticker);
      QObject::connect(visitWidget, &VisitWidget::printSummary, this, &ParticipantWidget::printSummary);
      QObject::connect(visitWidget, &VisitWidget::locateBartender, this, &ParticipantWidget::locateBartender);
    }

    if (currentPEPRole.canEditVisitAdministeringAssessor()) {
      QObject::connect(visitWidget, &VisitWidget::updateVisitAssessor, this, &ParticipantWidget::updateVisitAssessor);
    }
    else {
      visitWidget->disableAssessorSelection();
    }
  }

  if (!currentPEPRole.canPrintStickers()) {
    this->disablePrinting();
  }

  //Disable stuff based on user role
  for (auto device : mDeviceWidgets) {
    device->setEnabled(currentPEPRole.canManageDevices());
  }
  if (currentPEPRole.canSeeParticipantPersonalia()) {
    ui->info_header->show();
    ui->info1->show();
    ui->info2->show();
    ui->info_spacer->changeSize(20, 20);
  } else {
    ui->info_header->hide();
    ui->info1->hide();
    ui->info2->hide();
    ui->info_spacer->changeSize(0, 0);
  }


  if (mAllContexts.getItems().size() <= 1U) {
    release_participant_button->hide();
  }
}

void ParticipantWidget::disablePrinting() {
  print_buttons->setEnabled(false);

  for (auto visit : mVisitWidgets) {
    visit->disablePrinting();
  }
}

void ParticipantWidget::runQuery() {
  runQuery(true);
}

void ParticipantWidget::runQuery(bool completeRegistration) {
  std::vector<std::string> cols = {
    "ParticipantIdentifier",
    "IsTestParticipant",
    "StudyContexts"
  };

  for (const auto& device : globalConfig.getDevices()) {
    cols.push_back(device.columnName);
  }

  if (currentPEPRole.canSeeParticipantPersonalia()) {
    cols.push_back("ParticipantInfo");
  }

  auto aggregator = std::make_shared<ParticipantDataAggregator>(globalConfig);

  pep::enumerateAndRetrieveData2Opts opts;
  opts.pps = {currentUserPP};
  opts.columnGroups = {"ShortPseudonyms", "VisitAssessors"};
  opts.columns = cols;
  pepClient->enumerateAndRetrieveData2(opts)
  .observe_on(observe_on_gui())
  .subscribe([aggregator](pep::EnumerateAndRetrieveResult result) {
    aggregator->process(result);
  }, [this](std::exception_ptr ep) {
    emit participantLookupError(tr("Error while retrieving participant information: %1")
        .arg(QString::fromStdString(pep::GetExceptionMessage(ep))), pep::error);
  }, [this, aggregator, completeRegistration] {
    if (!aggregator->hasParticipantData()) {
      std::cerr << tr("No participant with ID %1 found").arg(participantSID).toStdString() << std::endl;
      emit participantLookupError(tr("No participant with ID %1 found").arg(participantSID), pep::error);
    }
    else if (completeRegistration && currentPEPRole.canRegisterParticipants() && !aggregator->hasCompleteParticipantData()) {
      emit statusMessage(tr("Participant registration is not complete. Attempting to complete registration..."), pep::warning);
      pepClient->completeParticipantRegistration(participantSID.toStdString())
        .observe_on(observe_on_gui())
        .subscribe([](pep::FakeVoid) {
      }, [this, aggregator](std::exception_ptr ep) {
        qDebug() << "Exception occured";
        emit statusMessage(tr("Completing registration failed."), pep::error);
        emit participantDataReceived(aggregator->getData(), aggregator->getStudyContexts()); // Show data even though it's incomplete
      }, [this] {
        emit statusMessage(tr("Registration completed succesfully"), pep::info);
        runQuery(false);
      });
    }
    else {
      emit participantDataReceived(aggregator->getData(), aggregator->getStudyContexts());
    }
  });
}

void ParticipantWidget::onParticipantDataReceived(ParticipantData data, std::string studyContexts) {
  //At this point all network IO is done. Emit an all clear signal and continue
  emit queryComplete();

  participantData = data;
  participantStudyContexts = mAllContexts.parse(studyContexts);

  if (participantData.mPersonalia.has_value() != currentPEPRole.canSeeParticipantPersonalia()) {
    LOG(LOG_TAG, pep::warning) << "Participant personalia viewer received no data";
  }

  processData();
}

void ParticipantWidget::setReadOnly(bool readOnly) {
  // Remember scroll state so that we can restore it at the end of this method
  auto vertical = ui->scrollArea->verticalScrollBar()->value();
  auto horizontal = ui->scrollArea->horizontalScrollBar()->value();

  this->readOnly = readOnly;

  for (auto device : mDeviceWidgets) {
    device->setEnabled(currentPEPRole.canManageDevices() && !readOnly);
  }

  edit_participant_button->setEnabled(currentPEPRole.canEditParticipantPersonalia() && !readOnly);
  release_participant_button->setEnabled(currentPEPRole.canSetParticipantContext() && (participantStudyContexts.getItems().size() > 1) && !readOnly);

  // Restore scroll state that may have been updated by disabling our button(s): see https://gitlab.pep.cs.ru.nl/pep/core/-/issues/2494#note_40659
  ui->scrollArea->verticalScrollBar()->setValue(vertical);
  ui->scrollArea->horizontalScrollBar()->setValue(horizontal);
}

/*! \brief Update device in pep infrastructure
 *
 * Once a user has changed the participant device through the ParticipantWidget::manageDevices() function the change needs to be sent to the pep infrastructure.
 * This function mangages changing the device ID in the infrastructure. This function is tied to a ui button defined in participantwidget.ui
 */
void ParticipantWidget::updateDevice(QString columnName, QString deviceId) {
  std::string serial = deviceId.toStdString();
  std::string type;
  auto timestamp = pep::TimeNow();

  const pep::ParticipantDeviceRecord* current = nullptr;
  std::vector<pep::ParticipantDeviceRecord> records;
  auto previous = participantData.mParticipantDeviceHistory.find(columnName.toStdString());
  if (previous != participantData.mParticipantDeviceHistory.cend()) {
    current = previous->second.getCurrent();
    std::copy(previous->second.begin(), previous->second.end(), std::back_inserter(records));
  }
  if (current) {
    assert(current->serial == serial);
    if (timestamp < current->time) {
      emit statusMessage(tr("Cannot deregister device with a scheduled (future) registration."), pep::error);
      return;
    }
    type = "stop";
  }
  else {
    std::transform(serial.begin(), serial.end(), serial.begin(), ::toupper);
    type = "start";
  }

  records.push_back(pep::ParticipantDeviceRecord(type, serial, std::string(), timestamp));
  pep::ParticipantDeviceHistory history;
  try {
    history = pep::ParticipantDeviceHistory(records, true);
  }
  catch (const std::exception &error) {
    emit statusMessage(tr("Error updating device registration: %1").arg(error.what()), pep::error);
    return;
  }

  this->setReadOnly(true);

  pepClient->storeData2(currentUserPP, columnName.toStdString(),
          std::make_shared<std::string>(history.toJson()), { pep::MetadataXEntry::MakeFileExtension(".json") })
  .observe_on(observe_on_gui())
  .subscribe([this, current](pep::DataStorageResult2 result) {
    emit statusMessage(current ? tr("Device deregistered.") : tr("Device registered."), pep::info);
    runQuery(false);
  }, [this](std::exception_ptr ep) {
    emit statusMessage(tr("Device registration failed: %1").arg(QString::fromStdString(pep::GetExceptionMessage(ep))), pep::error);
    this->setReadOnly(false);
  });
}

std::vector<pep::ShortPseudonymDefinition> ParticipantWidget::getPrintableShortPseudonyms(const std::optional<unsigned int>& visit) const {
  auto all = globalConfig.getShortPseudonyms(mStudyContext.getIdIfNonDefault(), visit);
  std::vector<pep::ShortPseudonymDefinition> retval;
  std::copy_if(all.cbegin(), all.cend(), std::back_inserter(retval), [](const pep::ShortPseudonymDefinition& entry) {
    return entry.getStickers() > 0;
  });
  return retval;
}

void ParticipantWidget::updateVisitAssessor(QString id) {
  qDebug() << "Setting assessor for visit " << currentVisitNumber << " to ID " << id;
  auto column = mStudyContext.getAdministeringAssessorColumnName(static_cast<uint32_t>(currentVisitNumber));
  this->pepClient->storeData2(currentUserPP, column,
    std::make_shared<std::string>(id.toStdString()), {pep::MetadataXEntry::MakeFileExtension(".txt")})
    .observe_on(observe_on_gui())
    .subscribe([](pep::DataStorageResult2 result) {
    // Do nothing
  }, [this](std::exception_ptr ep) {
    emit statusMessage(tr("Storage error: %1").arg(QString::fromStdString(pep::GetExceptionMessage(ep))), pep::error);
  }, [this]() {
    emit statusMessage(tr("Data stored"), pep::info);
    runQuery(false);
  });
}

/*! \brief Prints visit stickers via the bartender application
 *
 * This function requires the external bartender application. Prints all stickers for the current participant visit based on a template loaded from the client application directory.
 */
void ParticipantWidget::printAllVisitStickers() {
  this->invokeBartender(getPrintableShortPseudonyms(currentVisitNumber));
}

void ParticipantWidget::printAllParticipantStickers() {
  this->invokeBartender(getPrintableShortPseudonyms(std::nullopt));
}

void ParticipantWidget::invokeBartender(const std::vector<pep::ShortPseudonymDefinition>& printPseudonyms) {
  if(!currentPEPRole.canPrintStickers()) {
    return;
  }
#ifndef _WIN32
  emit statusMessage(tr("Printing requires BarTender and is only supported on Windows."), pep::error);
#else
  qDebug() << "Printing stickers";

  if (!this->provideBartenderPath()) {
    return;
  }
  assert(bartenderPath != std::nullopt);
  assert(std::filesystem::exists(*bartenderPath));

  if (!std::filesystem::exists(stickerFilePath)) {
    emit statusMessage(tr("The sticker layout file \"%1\" does not exist. Please add it at this location or update its path in the configuration file.").arg(QString::fromStdString(stickerFilePath.string())), pep::error);
    return;
  }

  QPrinter printer;
  QPrintDialog dialog(&printer, this);
  if (dialog.exec() != QDialog::Accepted) {
    emit statusMessage(tr("Printing cancelled."), pep::warning);
    return;
  }
  emit statusMessage(tr("Printing..."), pep::info);

  std::string printername = printer.printerName().toStdString();
  auto copies = static_cast<unsigned>(printer.copyCount());
  auto tempPath = pep::win32api::GetUniqueTemporaryPath().string();
  PEP_DEFER(std::filesystem::remove(tempPath));
  std::string stickersXml;
  std::string xmlTemplate = "<?xml version=\"1.0\" encoding=\"utf-8\"?><XMLScript Version=\"2.0\">%s</XMLScript>";
  // TODO: rename project specific "pomCode" variable to something more generic, e.g. "pseudonym"
  std::string stickerTemplate = ""
                                "<Command Name=\"Job%d\">"
                                "<Print>"
                                "<Format CloseAtEndOfJob=\"true\">%s</Format>"
                                "<NamedSubString Name=\"pomCode\">"
                                "<Value>%s</Value>"
                                "</NamedSubString>"
                                "<NamedSubString Name=\"dataType\">"
                                "<Value>%s</Value>"
                                "</NamedSubString>"
                                "<PrintSetup>"
                                "<Printer>%s</Printer>"
                                "<IdenticalCopiesOfLabel>%d</IdenticalCopiesOfLabel>"
                                "</PrintSetup>"
                                "</Print>"
                                "</Command>";

  // Iterate over the pseudonyms of the participant, lookup how many stickers
  // we should print of them and what their label should be, and add a print <Command>
  int i = 0;
  for (const auto& p : printPseudonyms) {
    const auto pseudonymName = p.getColumn().getFullName();
    const auto pseudonymHuman = describeShortPseudonymDefinition(p, true); // Always include visit description on sticker to ensure consistent output, regardless of the (set of) stickers in this run

    auto count = p.getStickers();
    if (!p.getSuppressAdditionalStickers()) {
      count += mSpareStickerCount;
    }
    count *= copies;

    const std::string& label = pseudonymHuman.toStdString();
    const std::string& pseudonym = participantData.mShortPseudonyms.at(pseudonymName);
    stickersXml += str(boost::format(stickerTemplate)
                       % ++i % stickerFilePath.string() % pseudonym  % label % printername % count);
  }

  // Write print xml file
  std::string xml = str(boost::format(xmlTemplate) % stickersXml);
  std::ofstream out(tempPath);
  if (out.bad()) {
    emit statusMessage(tr("Printing failed: could not open temporary print file"), pep::error);
    return;
  }
  out << xml;
  out.close();

  // o_O ... https://stackoverflow.com/a/27976653 ... =_=
  std::string bartendCommand = str(boost::format("\"\"%s\" /XMLScript=\"%s\" /X\"") % bartenderPath->string() % tempPath);
  int ret = std::system(bartendCommand.c_str());
  if (ret != 0) {
    // TODO: capture and display command line output
    emit statusMessage(tr("BarTender return error %1").arg(ret), pep::error);
  } else {
    emit statusMessage(tr("Printing succeeded."), pep::info);
  }
#endif
}

QString ParticipantWidget::describeShortPseudonymDefinition(const pep::ShortPseudonymDefinition& sp, bool includeVisitDescription) const {
  auto description = QString::fromStdString(sp.getDescription());
  auto visit = sp.getColumn().getVisitNumber();
  if (!includeVisitDescription || !visit.has_value()) {
    return description;
  }

  return description + " " + this->getVisitCaption(*visit);
}

/*! \brief Print a single sticker via the bartender application
 *
 * This function requires the external bartender application. Prints a test sticker for the current participant based on a template loaded from the client application directory.
 */
void ParticipantWidget::printSingleVisitSticker() {
  this->printSingleSticker(currentVisitNumber);
}

void ParticipantWidget::printSingleParticipantSticker() {
  this->printSingleSticker(std::nullopt);
}

void ParticipantWidget::printSingleSticker(const std::optional<unsigned int>& visit) {
  auto entries = pep::MakeSharedCopy(getPrintableShortPseudonyms(visit));

  auto dialog = new QDialog(this);
  dialog->setStyleSheet(infoEditStyle);
  dialog->setModal(true);
  dialog->setWindowTitle(tr("Select sticker to print"));

  auto list = new QListWidget(dialog);
  list->setMinimumSize(300, 300);
  auto acceptButton = new QPushButton(tr("accept"));
  auto cancelButton = new QPushButton(tr("cancel"));

  for (auto entry : *entries) {
    list->addItem(QString::fromStdString(entry.getColumn().getFullName()));
  }

  auto print = [this, entries, dialog, list]() {
    auto row = list->currentRow();
    assert(row >= 0);
    auto selected = (*entries)[static_cast<unsigned int>(row)];
    auto print = pep::ShortPseudonymDefinition(selected.getColumn().getFullName(), selected.getPrefix(), selected.getLength(), selected.getCastor(), 1, true, selected.getConfiguredDescription(), selected.getStudyContext());
    invokeBartender({ print });
    dialog->close();
  };

  QObject::connect(list, &QListWidget::itemSelectionChanged, this, [list, acceptButton]() {acceptButton->setEnabled(!list->selectedItems().empty()); });
  QObject::connect(list, &QListWidget::itemActivated, this, print);
  QObject::connect(acceptButton, &QPushButton::clicked, this, print);
  QObject::connect(cancelButton, &QPushButton::clicked, this, [dialog]() {
    dialog->close();
  });

  auto layout = new QVBoxLayout(this);
  layout->addWidget(list);
  layout->addWidget(acceptButton);
  layout->addWidget(cancelButton);
  dialog->setLayout(layout);

  dialog->show();
}

void ParticipantWidget::appendShortPseudonymsHtmlTable(QString& htmlFormattedSummary, const std::optional<unsigned int>& visitNumber,
  const QString& header, const std::function<bool(const pep::ShortPseudonymDefinition&)>& includeSp) {
  QString rows;

  auto sps = globalConfig.getShortPseudonyms(mStudyContext.getIdIfNonDefault(), visitNumber);
  auto includeVisitNumber = ContainsMultipleVisits(sps);
  for (const auto& p : sps) {
    if (includeSp(p)) {
      const auto& pseudonymName = p.getColumn().getFullName();
      const auto& pseudonymHuman = describeShortPseudonymDefinition(p, includeVisitNumber);

      rows.append(QString("<tr>"));
      rows.append("<td>" + pseudonymHuman + "</td>");
      rows.append(QString::fromStdString("<td>" + participantData.mShortPseudonyms[pseudonymName] + "</td>"));
      rows.append(QString("</tr>\n"));
    }
  }

  if (!rows.isEmpty()) {
    htmlFormattedSummary.append("<h2>" + header + "</h2>");
    htmlFormattedSummary.append(QString("<table style=\"border:solid; text-align:left; font-size:large\">"));
    htmlFormattedSummary.append(rows);
    htmlFormattedSummary.append(QString("</table>\n"));
  }
}

bool ParticipantWidget::provideBartenderPath() {
  auto getConfiguredPathError = [this]() -> const char* {
    if (bartenderPath == std::nullopt) {
      return "Bartender path not configured.";
    }
    if (!std::filesystem::exists(*bartenderPath)) {
      return "Bartender not found at configured location.";
    }
    return nullptr;
  };

  auto msg = getConfiguredPathError();
  if (msg == nullptr) {
    return true;
  }

  emit statusMessage(tr(msg), pep::error);
  this->locateBartender();
  return getConfiguredPathError() == nullptr;
}

/*! \brief Print summary of current participant information
 *
 * Used to aid the assessors.
 */
void ParticipantWidget::printSummary() {
#ifdef _WIN32
  if(!currentPEPRole.canPrintSummary()) {
    return;
  }

  QString htmlFormattedSummary = "<html>";

  htmlFormattedSummary.append("<p><b>" + mProjectName + "</b></p>");

  if (participantData.mPersonalia) {
    htmlFormattedSummary.append(QString::fromStdString("<h1>" + participantData.mPersonalia->getFullName() + "</h1>"));
    htmlFormattedSummary.append(QString::fromStdString("<h4>" + participantData.mPersonalia->getDateOfBirth() + "</h4>"));
  }
  if (participantData.mIsTestParticipant) {
    htmlFormattedSummary.append("<h4>" + tr("This is a test participant") + "</h4>");
  }
  htmlFormattedSummary.append(QString("<h4>")+participantSID+"</h4>");

  appendShortPseudonymsHtmlTable(htmlFormattedSummary, std::nullopt, tr("Participant pseudonyms"),
    [](const pep::ShortPseudonymDefinition& sp) {return true; });
  auto visitNumber = static_cast<unsigned int>(currentVisitNumber);
  appendShortPseudonymsHtmlTable(htmlFormattedSummary, visitNumber, tr("%1 pseudonyms").arg(this->getVisitCaption(visitNumber)),
    [nr = visitNumber](const pep::ShortPseudonymDefinition& sp) {return sp.getColumn().getVisitNumber() == nr; });
  appendShortPseudonymsHtmlTable(htmlFormattedSummary, visitNumber, tr("Pseudonyms for other visits"),
    [nr = visitNumber](const pep::ShortPseudonymDefinition& sp) {return sp.getColumn().getVisitNumber() != nr; });

  htmlFormattedSummary.append(QString("</html>\n"));

  QTextDocument summary;
  summary.setDefaultStyleSheet(summaryPrintStyle);
  summary.setHtml(htmlFormattedSummary);

  QPrinter printer(QPrinter::PrinterResolution);
  printer.setPageSize(QPageSize(QPageSize::A4));
  QPrintDialog dialog(&printer, this);
  dialog.setWindowTitle(tr("Print Document"));
  if (dialog.exec() != QDialog::Accepted) {
    emit statusMessage(tr("Printing cancelled."), pep::warning);
  } else {
    summary.print(&printer);
  }
#else
  emit statusMessage(tr("Printing is only supported on Windows."), pep::error);
#endif
}

void ParticipantWidget::locateBartender() {
  std::optional<std::filesystem::path> bestDir;
  std::optional<std::string> bestFile;

  if (bartenderPath != std::nullopt) {
    bestDir = bartenderPath->parent_path();
    bestFile = bartenderPath->filename().string();
  }

#ifdef _WIN32

  if (bestDir == std::nullopt) {
    // Try to read directory from the Windows Registry: see https://gitlab.pep.cs.ru.nl/pep/core/-/issues/1891
    HKEY key;
    if (::RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Seagull Scientific\\BarTender", 0, KEY_READ, &key) == ERROR_SUCCESS) {
      PEP_DEFER(RegCloseKey(key));

      DWORD dwType;
      const size_t BUFFER_SIZE = 256U;
      BYTE regvalue[BUFFER_SIZE];
      DWORD size = BUFFER_SIZE;
      if (::RegQueryValueExA(key, "Last Execution Directory", 0, &dwType, regvalue, &size) == ERROR_SUCCESS) { // TODO: deal with ERROR_MORE_DATA: see https://docs.microsoft.com/en-us/windows/win32/api/winreg/nf-winreg-regqueryvalueexa
        if (dwType == REG_SZ) { // TODO: support other string types as well
          bestDir = std::string((char*)regvalue, size - 1);
        }
      }
    }
  }

  if (bestDir == std::nullopt) { // Try to locate a (single) directory named "*seagull*\*bartend*" under ProgramFiles

    // Lambda finds a single subdirectory under "dir" with a particular "partialSubdirName"
    auto setToSingleExistingSubdir = [](std::filesystem::path& dir, const std::string& partialSubdirName) {
      assert(std::filesystem::is_directory(dir));

      std::optional<std::filesystem::path> subdir;
      for (std::filesystem::directory_iterator i(dir); i != std::filesystem::directory_iterator(); ++i) {
        if (std::filesystem::is_directory(*i)) {
          // Case insensitive substring matching: https://stackoverflow.com/a/26733890
          auto name = i->path().filename().string();
          if (boost::ifind_first(name, partialSubdirName)) {
            if (subdir != std::nullopt) {
              // Multiple subdirectories match "partialSubdirName"
              return false;
            }
            subdir = *i;
          }
        }
      }
      if (subdir != std::nullopt) {
        // Single subdirectory matches "partialSubdirName"
        dir = *subdir;
        return true;
      }

      // No subdirectory matches "partialSubdirName"
      return false;
    };

    auto installDir = pep::win32api::GetKnownFolderPath(pep::win32api::KnownFolder::ProgramFiles);
    if (setToSingleExistingSubdir(installDir, "seagull")) {
      setToSingleExistingSubdir(installDir, "bartend");
    }
    bestDir = installDir;
  }

#endif

  auto dir = bestDir.value_or(std::filesystem::path());
  while (!dir.empty() && !std::filesystem::exists(dir)) {
    dir = dir.parent_path();
  }

  QFileDialog dialog(this);
  dialog.setWindowTitle(tr("Locate bartender executable"));
  dialog.setFileMode(QFileDialog::FileMode::ExistingFile);
  dialog.setDirectory(QString::fromStdString(dir.string()));

  QStringList filters;
  filters.push_back("Bartender (bartend.exe)");
  filters.push_back(QString("%1 (*.exe)").arg(tr("All executables")));
  filters.push_back(QString("%1 (*.*)").arg(tr("All files")));
  dialog.setNameFilter(filters.join(";;"));

  auto file = bestFile.value_or("bartend.exe");
  if (std::filesystem::exists(dir / file)) {
    dialog.selectFile(QString::fromStdString(file));
    if (file == "bartend.exe") {
      dialog.selectNameFilter(filters[0]);
    }
    else if (std::filesystem::path(file).extension() == ".exe") {
      dialog.selectNameFilter(filters[1]);
    }
    else {
      dialog.selectNameFilter(filters[2]);
    }
  }

  if (dialog.exec()) {
    assert(dialog.selectedFiles().size() == 1);
    bartenderPath = StoreConfiguredBartenderPath(dialog.selectedFiles()[0].toStdString());
  }
}

/*! \brief Close current participant
 *
 * Sets current object to be deleted.
 */
void ParticipantWidget::closeParticipant() {
  //Should also clear out current patient data
  //std::cout << ">>>ParticipantWidget on_exitPatient_clicked called" << std::endl;
  mainWindow->changeActiveTab(0);
  mainWindow->mOpenedParticipants.remove(participantSID);
  deleteLater();
  parent()->deleteLater();
}

/*! \brief Function called when translation button is pressed
 *
 * When a translation signal hits this object this code block will be executed.
 * At time of writing (1-16-2018) only one translation call is made and this happens on construction.
 */
void ParticipantWidget::onTranslation() {
  ui->retranslateUi(this);
  this->processData();
}

/*! \brief Process current information
 *
 * This code block does a lot. All of the current UI is configured in this function. No arguments are taken, but many class variables are used.
 */
void ParticipantWidget::processData() {
  ui->participant->setText(tr("participant '%1'").arg(participantSID));

  if (!participantStudyContexts.contains(mStudyContext)) {
    ui->label_unavailable->setText(tr("This participant is unavailable in the current (%1) context.").arg(QString::fromStdString(mStudyContext.getId())));
    ui->state->setCurrentWidget(ui->acquire);
    return;
  }

  if (currentPEPRole.canSeeParticipantPersonalia()) {
    if (participantData.mPersonalia) {
      ui->info1->setText(QString::fromStdString(participantData.mPersonalia->getFullName()));
      ui->info2->setText(QString::fromStdString(participantData.mPersonalia->getDateOfBirth()));
    }
    else {
      ui->info1->setText(QString());
      ui->info2->setText(QString());
    }
    this->setReadOnly(false);
  }

  ui->infoIsTestParticipant->setVisible(participantData.mIsTestParticipant);

  release_participant_button->setEnabled(currentPEPRole.canSetParticipantContext() && participantStudyContexts.getItems().size() > 1);

  for (auto i = 0U; i < mDeviceWidgets.size(); ++i) {
    auto widget = mDeviceWidgets[i];
    auto columnName = widget->getColumnName();

    const auto& history = participantData.mParticipantDeviceHistory[columnName.toStdString()];
    auto current = history.getCurrent();
    widget->setDeviceId(current ? QString::fromStdString(current->serial) : QString());

    auto historyWidget = std::find_if(mDeviceHistoryWidgets.begin(), mDeviceHistoryWidgets.end(), [&columnName](DeviceHistoryWidget *candidate) {return candidate->getColumnName() == columnName; });
    (*historyWidget)->setHistory(history);

    if (mStudyContext.matches(globalConfig.getDevices()[i].studyContext)) {
      std::string historyInvalidReason;
      if (!history.isValid(&historyInvalidReason)) {
        emit statusMessage(tr("Device history for column %1 is invalid: %2. Please correct the device history.").arg(columnName, historyInvalidReason.c_str()), pep::error);
      }
    }
  }

  const std::unordered_map<unsigned, unsigned>* visitAssessors = nullptr;
  auto assessorsForContext = participantData.mVisitAssessors.find(mStudyContext.getIdIfNonDefault());
  if (assessorsForContext != participantData.mVisitAssessors.cend()) {
    visitAssessors = &assessorsForContext->second;
  }
  //Fill in ui elements with the appropriate short pseudonyms
  for (auto i = 0U; i < mVisitWidgets.size(); i++) {
    auto widget = mVisitWidgets[i];
    initializeShortPseudonymsUi(i + 1,
      widget->getPseudonymButtonCaption(), widget->getPseudonymButtonBar(), widget->getPseudonymButtonSpacer(),
      widget->getPseudonymCaption(), widget->getPseudonymLabel(),
      widget->getPrintAllButton(), widget->getPrintOneButton(),
      &widget->getPseudonymSpacerForOtherVisits(), &widget->getPseudonymCaptionForOtherVisits(), &widget->getPseudonymLabelForOtherVisits());

    std::optional<unsigned> assessorId;
    if (visitAssessors != nullptr) {
      auto position = visitAssessors->find(i + 1);
      if (position != visitAssessors->cend()) {
        assessorId = position->second;
      }
    }
    widget->setCurrentAssessor(assessorId);
  }
  initializeShortPseudonymsUi(std::nullopt,
    *ui->ops_header, *castor_buttons, *ui->verticalSpacer_3,
    *ui->pseudo_header, *ui->pseudo_participant,
    *print_stickers_button, *print_oneSticker_button);

  ui->state->setCurrentWidget(ui->editor);
}

void ParticipantWidget::acquireParticipant() {
  auto updated = participantStudyContexts;
  updated.add(mStudyContext);
  pepClient->storeData2(currentUserPP, "StudyContexts",
          std::make_shared<std::string>(updated.toString()), {pep::MetadataXEntry::MakeFileExtension(".csv")})
    .subscribe(
      [](pep::DataStorageResult2) { /* ignore */ },
      [this](std::exception_ptr ep) { emit statusMessage(tr("Adding participant to context failed: ") + QString::fromStdString(pep::GetExceptionMessage(ep)), pep::error); },
      [this]() { runQuery(); }
    );
}

QString ParticipantWidget::getShortPseudonymListText(const std::vector<ShortPseudonymListEntry> entries, bool includeVisitNumber) const {
  QStringList lines;

  for (const auto& entry : entries) {
    lines += QString("%1: %2").arg(describeShortPseudonymDefinition(entry.definition, includeVisitNumber), entry.value.c_str());
  }

  return lines.join("\n");
}

QString ParticipantWidget::getVisitCaption(unsigned visitNumber) const {
  if (visitNumber < 1) {
    throw std::runtime_error("Please provide a 1-based visit number (as opposed to a 0-based index)");
  }

  if (mVisitCaptions != nullptr) {
    auto index = visitNumber - 1;
    if (index < mVisitCaptions->size()) {
      return QString::fromStdString(mVisitCaptions->at(index));
    }
  }
  return tr("Visit %1").arg(visitNumber);
}

void ParticipantWidget::initializeShortPseudonymsUi(const std::optional<uint32_t>& visitNumber,
  QLabel& buttonBarCaption, ButtonBar& buttonBar, QSpacerItem& spacer,
  QLabel& pseudonymsCaption, QLabel& pseudonymsLabel,
  QPushButton& printAllButton, QPushButton& printOneButton,
  QSpacerItem* pseudonymSpacerForOtherVisits, QLabel* pseudonymsCaptionForOtherVisits, QLabel* pseudonymsLabelForOtherVisits) {
  assert((pseudonymSpacerForOtherVisits == nullptr) == (pseudonymsCaptionForOtherVisits == nullptr));
  assert((pseudonymSpacerForOtherVisits == nullptr) == (pseudonymsLabelForOtherVisits == nullptr));

  buttonBar.clear();
  std::vector<ShortPseudonymListEntry> ownVisit;
  std::vector<ShortPseudonymListEntry> otherVisits;
  auto hasCastorButton = false;
  auto hasSticker = false;

  for (auto p : globalConfig.getShortPseudonyms(mStudyContext.getIdIfNonDefault(), visitNumber)) {
    if (p.getStickers() > 0) {
      hasSticker = true;
    }

    auto sp = participantData.mShortPseudonyms[p.getColumn().getFullName()];

    if (p.getColumn().getVisitNumber() == visitNumber) {
      ownVisit.push_back({ p, sp });
      auto castor = p.getCastor();
      if (castor) {
        hasCastorButton = true;
        auto url = baseUrl.arg(castor->getStudySlug().c_str(), sp.c_str());
        auto slot = [url]() {QDesktopServices::openUrl(QUrl(url));};
        buttonBar.addButton(QString::fromStdString(p.getDescription()), slot, !sp.empty());
      }
    }
    else {
      otherVisits.push_back({ p, sp });
    }
  }

  buttonBarCaption.setVisible(hasCastorButton);
  if (hasCastorButton) {
    spacer.changeSize(20, 20);
  }
  else {
    spacer.changeSize(0, 0);
  }

  auto pseudonymTextMain = this->getShortPseudonymListText(ownVisit, !otherVisits.empty());
  auto pseudonymTextOtherVisits = this->getShortPseudonymListText(otherVisits, true);

  if (pseudonymSpacerForOtherVisits != nullptr) {
    if (pseudonymTextMain.isEmpty() || pseudonymTextOtherVisits.isEmpty()) {
      pseudonymSpacerForOtherVisits->changeSize(0, 0);
    }
    else {
      pseudonymSpacerForOtherVisits->changeSize(20, 20);
    }
    pseudonymsCaptionForOtherVisits->setVisible(!pseudonymTextOtherVisits.isEmpty());
    pseudonymsLabelForOtherVisits->setVisible(!pseudonymTextOtherVisits.isEmpty());
    pseudonymsLabelForOtherVisits->setText(pseudonymTextOtherVisits);
  }
  else if (!pseudonymTextOtherVisits.isEmpty()) {
    std::string widgetDescription;
    if (visitNumber) {
      widgetDescription = this->getVisitCaption(*visitNumber).toStdString();
    }
    else {
      widgetDescription = "Participant";
    }
    LOG(LOG_TAG, pep::warning) << widgetDescription << " widget: no separate label available for pseudonyms for other visits";
    pseudonymTextMain.append("\n\n");
    pseudonymTextMain.append(pseudonymTextOtherVisits);
  }

  pseudonymsCaption.setVisible(!pseudonymTextMain.isEmpty());
  pseudonymsLabel.setVisible(!pseudonymTextMain.isEmpty());
  pseudonymsLabel.setText(pseudonymTextMain);

  if (currentPEPRole.canPrintStickers()) {
    printAllButton.setEnabled(hasSticker);
    printOneButton.setEnabled(hasSticker);
  }
}

void ParticipantWidget::openEditParticipant() {
  if (!currentPEPRole.canEditParticipantPersonalia()) {
    return;
  }

  QDialog* participantInfoEdit = new QDialog(this);
  participantInfoEdit->setStyleSheet( // TODO: centralize
    "QLabel {"
    "  border: 0.05em solid transparent;"
    "  padding: 0.25em;"
    "  font-size: 14pt;"
    "}"
    ""
    "QLineEdit {"
    "        border: 0.05em solid black;"
    "        border-radius: 0.25em;"
    "        padding: 0.25em;"
    "        font-size: 14pt;"
    "        outline: none;"
    "}"
    "QLineEdit:focus {"
    "        border: 0.05em solid #CA0B5E;"
    "}"
    "QLineEdit[error=true] {"
    "        color: black;"
    "        background-color: rgb(255, 230, 230);"
    "}"
    "QDateEdit {"
    "        border: 0.05em solid black;"
    "        border-radius: 0.25em;"
    "        padding: 0.25em;"
    "        font-size: 14pt;"
    "        outline: none;"
    "}"
    "QDateEdit:focus {"
    "        border: 0.05em solid #CA0B5E;"
    "}"
    "QDateEdit[error=true] {"
    "        color: black;"
    "        background-color: rgb(255, 230, 230);"
    "}"
    "QPushButton {"
    "        border: 0.05em solid #CA0B5E;"
    "        border-radius: 0.25em;"
    "        color: #CA0B5E;"
    "        padding: 0.5em;"
    "        font-size: 14pt;"
    "        outline: none;"
    "}"
    "QPushButton:hover,QPushButton:focus {"
    "        background-color: rgba(202,11,94,0.8);"
    "        color: white;"
    "}"
    "QPushButton:disabled {"
    "        color: grey;"
    "        border-color: grey;"
    "}"
  );
  participantInfoEdit->setModal(true);
  QVBoxLayout* infoEditLayout = new QVBoxLayout(this);

  auto editor = new ParticipantEditor(participantInfoEdit);
  if (participantData.mPersonalia) {
    editor->setPersonalia(*participantData.mPersonalia);
  }
  editor->setIsTestParticipant(participantData.mIsTestParticipant);

  QObject::connect(editor, &ParticipantEditor::cancelled, participantInfoEdit, &QDialog::close);
  QObject::connect(editor, &ParticipantEditor::confirmed, this, [this, participantInfoEdit, editor]() {
    auto pp = pep::MakeSharedCopy(currentUserPP);
    std::vector<pep::StoreData2Entry> entries;
    auto personalia = editor->getPersonalia();
    if (personalia != this->participantData.mPersonalia) {
      entries.push_back(pep::StoreData2Entry(pp, "ParticipantInfo", pep::MakeSharedCopy(personalia.toJson()), {pep::MetadataXEntry::MakeFileExtension(".json")}));
    }
    auto isTestParticipant = editor->getIsTestParticipant();
    if (isTestParticipant != this->participantData.mIsTestParticipant) {
      entries.push_back(pep::StoreData2Entry(pp, "IsTestParticipant", pep::MakeSharedCopy(pep::BoolToString(isTestParticipant)), { pep::MetadataXEntry::MakeFileExtension(".txt") }));
    }

    if (entries.empty()) {
      emit statusMessage(tr("Unchanged data not stored"), pep::info);
      return;
    }

    //Store updated data
    this->setReadOnly(true);
    participantInfoEdit->close();

    this->pepClient->storeData2(entries)
      .observe_on(observe_on_gui())
      .subscribe([](pep::DataStorageResult2 result) {
      // Do nothing
    }, [this](std::exception_ptr ep) {
      emit statusMessage(tr("Storage error: %1").arg(QString::fromStdString(pep::GetExceptionMessage(ep))), pep::error);
      this->setReadOnly(false);
    }, [this]() {
      emit statusMessage(tr("Data stored"), pep::info);
      runQuery(false);
    });
  });

  infoEditLayout->addWidget(editor);
  participantInfoEdit->setLayout(infoEditLayout);
  infoEditLayout->setSizeConstraint(QLayout::SetFixedSize);

  participantInfoEdit->show();
}

void ParticipantWidget::releaseParticipant() {
  if ((!currentPEPRole.canSetParticipantContext()) || (participantStudyContexts.getItems().size() <= 1)) {
    return;
  }

  QMessageBox confirm;
  confirm.setText(tr("Remove participant from %1 context?").arg(QString::fromStdString(mStudyContext.getId())));
  confirm.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);
  confirm.setDefaultButton(QMessageBox::Cancel);
  confirm.setIcon(QMessageBox::Warning);
  if (confirm.exec() == QMessageBox::Ok) {
    auto updated = participantStudyContexts;
    updated.remove(mStudyContext);

    setReadOnly(true);
    this->pepClient->storeData2(currentUserPP, "StudyContexts",
            std::make_shared<std::string>(updated.toString()), {pep::MetadataXEntry::MakeFileExtension(".csv")})
      .subscribe(
        [](pep::DataStorageResult2) { /* ignore */ },
        [this](std::exception_ptr ep) {
      setReadOnly(false);
      emit statusMessage(tr("Removing participant from context failed: ") + QString::fromStdString(pep::GetExceptionMessage(ep)), pep::error);
    },
        [this]() { runQuery(); }
    );
  }
}

void ParticipantWidget::editDeviceHistoryEntry(QString columnName, size_t index) {
  if (readOnly || !currentPEPRole.canManageDevices()) {
    return;
  }
  const auto& history = participantData.mParticipantDeviceHistory[columnName.toStdString()];

  auto isLastRecord = (index + 1 == history.size());

  assert(index < history.size());
  auto record = *std::next(history.begin(), static_cast<ptrdiff_t>(index));

  auto timestamp = record.time;
  std::optional<pep::Timestamp> previousEntry;
  if (index > 0) {
    previousEntry = std::next(history.begin(), static_cast<ptrdiff_t>(index) - 1)->time;
  }
  std::optional<pep::Timestamp> nextEntry;
  if (!isLastRecord) {
    nextEntry = std::next(history.begin(), static_cast<ptrdiff_t>(index) + 1)->time;
  }

  auto dialog = new QDialog(this);
  dialog->setStyleSheet(
    "QPushButton#nowButton, QPushButton#okButton, QPushButton#cancelButton {\n"
    " border: 0.05em solid #CA0B5E;\n"
    " border-radius: 0.25em;\n"
    " color: #CA0B5E;\n"
    " padding: 0.5em;\n"
    " font-size: 13pt;\n"
    " outline: none;\n"
    "}\n"
    "QPushButton#nowButton:pressed, QPushButton#okButton:pressed, QPushButton#cancelButton:pressed {\n"
    " color: black;\n"
    " border-color: grey;\n"
    "}\n"
    "QPushButton#nowButton:disabled, QPushButton#okButton:disabled, QPushButton#cancelButton:disabled {\n"
    "color: grey;\n"
    " border-color: grey;\n"
    "}\n"
    "QPushButton#nowButton:hover, QPushButton#nowButton:focus, QPushButton#okButton:hover, QPushButton#okButton:focus, QPushButton#cancelButton:hover, QPushButton#cancelButton:focus { background-color: rgba(202,11,94,0.8); color: white; }\n"
    "QLabel#topLabel { font-size: 14pt; color: black; border: none; }\n"
    "QLabel#topLabel:hover { color: black; }\n"
  );
  dialog->setMinimumSize(400, 275);
  dialog->setModal(true);

  auto layout = new QFormLayout(this);
  dialog->setLayout(layout);
  layout->setSizeConstraint(QLayout::SetFixedSize);

  auto topLabel = new QLabel(QString::fromStdString(record.serial) + " " + (record.isActive() ? tr("deviceRegisteredOn") : tr("deviceUnregisteredOn")));
  topLabel->setObjectName("topLabel");
  layout->addRow(topLabel);

  auto editor = new DateTimeEditor();
  layout->addRow(editor);
  editor->setValue(pep::LocalQDateTimeFromStdTimestamp(timestamp));

  auto nowButton = new QPushButton(tr("set-timestamp-to-now"), this);
  nowButton->setObjectName("nowButton");
  QObject::connect(nowButton, &QPushButton::clicked, [editor]() {
    editor->setValue(QDateTime::currentDateTime());
  });
  layout->addRow(nowButton);
  nowButton->setEnabled(isLastRecord);

  auto buttonLayout = new QHBoxLayout();
  layout->addRow(buttonLayout);

  auto okButton = new QPushButton(tr("OK"), this);
  okButton->setObjectName("okButton");
  okButton->setDefault(true);
  buttonLayout->addWidget(okButton);

  auto cancelButton = new QPushButton(tr("Cancel"), this);
  cancelButton->setObjectName("cancelButton");
  QObject::connect(cancelButton, &QPushButton::clicked, [dialog]() { dialog->close(); });
  buttonLayout->addWidget(cancelButton);

  auto updateOkButtonEnabled = [=]() {
    auto entered = editor->getValue();
    auto valid = entered.isValid(); // Valid timestamp was entered
    if (previousEntry) {
      valid &= (entered > pep::LocalQDateTimeFromStdTimestamp(*previousEntry)); // Is not earlier than the previous entry
    }
    if (nextEntry) {
      valid &= (entered < pep::LocalQDateTimeFromStdTimestamp(*nextEntry)); // Is not later than the next entry
    }

    okButton->setEnabled(valid);
  };

  QObject::connect(editor, &DateTimeEditor::valueChanged, updateOkButtonEnabled);

  QObject::connect(okButton, &QPushButton::clicked, [=, this /* Inclusion of "this" in capture-everything is deprecated in C++20 */]() {
    auto records = std::vector<pep::ParticipantDeviceRecord>(history.begin(), history.end());

    pep::ParticipantDeviceHistory history;
    try {
      records[index].time = pep::QDateTimeToStdTimestamp(editor->getValue());
      history = pep::ParticipantDeviceHistory(records, true);
    }
    catch (const std::exception& error) {
      emit statusMessage(tr("Input error: %1").arg(error.what()), pep::error);
      return;
    }

    setReadOnly(true);
    okButton->setEnabled(false);
    cancelButton->setEnabled(false);
    auto cancelReadOnly = [this, okButton, cancelButton]() {
      setReadOnly(false);
      okButton->setEnabled(true);
      cancelButton->setEnabled(true);
    };

    pepClient->storeData2(currentUserPP, columnName.toStdString(),
            std::make_shared<std::string>(history.toJson()), {pep::MetadataXEntry::MakeFileExtension(".json")})
      .observe_on(observe_on_gui())
      .subscribe(
        [](pep::DataStorageResult2 result) { /* ignore */ },
        [this, cancelReadOnly](std::exception_ptr error) { cancelReadOnly(); emit statusMessage(tr("Storage error: %1").arg(QString::fromStdString(pep::GetExceptionMessage(error))), pep::error); },
        [this, dialog]() { dialog->close(); emit statusMessage(tr("Device record updated"), pep::info); runQuery(); }
    );
  });

  dialog->show();
}

/*! \brief Helper function to make the visit number visible class wide.
 *
 * This function takes a zero based visit number (0, 1, 2 at time of writing and hopefully forever) and sets the current visit number to a one based number (1, 2, 3).
 * In case it's not clear the mapping should be {0->1, 1->2, 2->3}
 *
 * \param visitNumber The zero based visit number.
 */

void ParticipantWidget::setCurrentVisitNumber(int visitNumber) {
  ////std::cout << "Visit number changed to " << visitNumber << std::endl;
  currentVisitNumber = visitNumber +1; //Input in 0 indexed and labels are 1 indexed.
}

/*! \brief Destructor
 *
 * Unsubscribes from shared resources and deletes Qt UI.
 */

ParticipantWidget::~ParticipantWidget() {
  delete ui;
}

const pep::ShortPseudonymDefinition *ParticipantDataAggregator::getShortPseudonymDefinition(const std::string& shortPseudonymTag) const {
  const auto& spDefinitions = mGlobalConfig.getShortPseudonyms();
  auto position = std::find_if(std::begin(spDefinitions), std::end(spDefinitions),
    [shortPseudonymTag](pep::ShortPseudonymDefinition definition) {
    return definition.getColumn().getFullName() == shortPseudonymTag;
  });
  if (position == std::end(spDefinitions)) {
    return nullptr;
  }
  return &*position;
}

void ParticipantDataAggregator::processDeviceHistory(const pep::EnumerateAndRetrieveResult& result) {
  assert(mDeviceHistory.find(result.mColumn) == mDeviceHistory.cend());
  mDeviceHistory[result.mColumn] = result;
}

void ParticipantDataAggregator::processParticipantInfo(const pep::EnumerateAndRetrieveResult& result) {
  assert(mParticipantInfo == std::nullopt);
  mParticipantInfo = result;
}

void ParticipantDataAggregator::processIsTestParticipant(const pep::EnumerateAndRetrieveResult& result) {
  assert(mIsTestParticipant == std::nullopt);
  mIsTestParticipant = result;
}

void ParticipantDataAggregator::processParticipantIdentifier(const pep::EnumerateAndRetrieveResult& result) {
  mParticipantIdentifierIsSet = true;
}

void ParticipantDataAggregator::processShortPseudonym(const pep::EnumerateAndRetrieveResult& result) {
  auto shortPseudonymTag = result.mColumn;
  auto definition = getShortPseudonymDefinition(shortPseudonymTag);
  if (definition != nullptr) {
    mShortPseudonyms[shortPseudonymTag] = result.mData;
    mUnfilledShortPseudonyms.erase(std::remove(mUnfilledShortPseudonyms.begin(), mUnfilledShortPseudonyms.end(), definition), mUnfilledShortPseudonyms.end());
  }
}

void ParticipantDataAggregator::processStudyContexts(const pep::EnumerateAndRetrieveResult& result) {
  mStudyContexts = result.mData;
}

void ParticipantDataAggregator::processVisitAssessor(const pep::EnumerateAndRetrieveResult& result) {
  std::string context;
  unsigned visit;
  [[maybe_unused]] auto match = isVisitAssessorColumn(result.mColumn, &context, &visit);
  assert(match);

  std::optional<unsigned int> assessorId;
  if (!result.mData.empty()) {
    assessorId = static_cast<unsigned int>(std::stoul(result.mData));
  }
  auto inserted = mVisitAssessors[context].insert(std::make_pair(visit, assessorId)).second;
  if (!inserted) {
    throw std::runtime_error("Participant has multiple assessors defined for visit " + std::to_string(visit));
  }
}

bool ParticipantDataAggregator::infoPseudonymsIsSet() const noexcept {
  return mUnfilledShortPseudonyms.empty();
}

bool ParticipantDataAggregator::isVisitAssessorColumn(const std::string& columnName, std::string* context, unsigned* visitNumber) const {
  const QRegularExpression regex(R"(^(?:(.+)\.)?Visit(\d)\.Assessor$)");
  auto result = regex.match(QString::fromStdString(columnName));
  if (result.hasMatch()) {
    if (context != nullptr) {
      *context = result.captured(1).toStdString();
    }
    if (visitNumber != nullptr) {
      *visitNumber = static_cast<unsigned int>(std::stoul(result.captured(2).toStdString()));
    }
  }
  return result.hasMatch();
}

bool ParticipantDataAggregator::isDeviceHistoryColumn(const std::string& columnName) const {
  const auto& devices = mGlobalConfig.getDevices();
  auto end = devices.cend();
  return std::find_if(devices.cbegin(), end, [&columnName](const pep::DeviceRegistrationDefinition& definition) {return definition.columnName == columnName; }) != end;
}

ParticipantDataAggregator::ParticipantDataAggregator(const pep::GlobalConfiguration& globalConfig) noexcept
  : mGlobalConfig(globalConfig), mParticipantIdentifierIsSet(false) {
  auto inserter = std::back_inserter(mUnfilledShortPseudonyms);
  std::transform(mGlobalConfig.getShortPseudonyms().begin(), mGlobalConfig.getShortPseudonyms().end(), inserter, [](const pep::ShortPseudonymDefinition& definition) {return &definition; });
}

void ParticipantDataAggregator::process(const pep::EnumerateAndRetrieveResult& result) {
  if (result.mColumn.starts_with("ShortPseudonym.")) {
    this->processShortPseudonym(result);
  }
  else if (isDeviceHistoryColumn(result.mColumn)) {
    this->processDeviceHistory(result);
  }
  else if (result.mColumn == "ParticipantInfo") {
    this->processParticipantInfo(result);
  }
  else if (result.mColumn == "IsTestParticipant") {
    this->processIsTestParticipant(result);
  }
  else if (result.mColumn == "ParticipantIdentifier") {
    this->processParticipantIdentifier(result);
  }
  else if (result.mColumn == "StudyContexts") {
    this->processStudyContexts(result);
  }
  else if (isVisitAssessorColumn(result.mColumn)) {
    this->processVisitAssessor(result);
  }
}

bool ParticipantDataAggregator::hasParticipantData() const noexcept {
  return !mDeviceHistory.empty() || mParticipantInfo || mParticipantIdentifierIsSet || !mShortPseudonyms.empty() || !mStudyContexts.empty();
}

bool ParticipantDataAggregator::hasCompleteParticipantData() const noexcept {
  return hasParticipantData() && mParticipantIdentifierIsSet && infoPseudonymsIsSet();
}

ParticipantData ParticipantDataAggregator::getData() const {
  if (!hasParticipantData()) {
    throw std::runtime_error("No participant data aggregated");
  }

  ParticipantData participantData;
  if (mParticipantInfo) {
    auto personalia = pep::ParticipantPersonalia::FromJson(mParticipantInfo->mData);
    if (personalia.getFullName().empty() && personalia.getDateOfBirth().empty()) {
      LOG(LOG_TAG, pep::warning) << "Received empty participant personalia";
    }
    participantData.mPersonalia = personalia;
  }
  if (mIsTestParticipant) {
    participantData.mIsTestParticipant = pep::StringToBool(mIsTestParticipant->mData);
  }
  participantData.mShortPseudonyms = mShortPseudonyms;
  for (const auto& history: mDeviceHistory) {
    participantData.mParticipantDeviceHistory[history.first] = pep::ParticipantDeviceHistory::Parse(history.second.mData, false);
  }

  for (const auto& context : mVisitAssessors) {
    for (const auto& visit: context.second) {
      if (visit.second.has_value()) { // Only copy if nonempty assessor ID has been retrieved from storage
        participantData.mVisitAssessors[context.first][visit.first] = *visit.second;
      }
    }
  }

  return participantData;
}
