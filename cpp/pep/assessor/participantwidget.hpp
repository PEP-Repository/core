#pragma once

#include <pep/client/Client_fwd.hpp>
#include <pep/structure/StudyContext.hpp>
#include <pep/assessor/UserRole.hpp>
#include <pep/assessor/mainwindow.hpp>
#include <pep/assessor/Branding.hpp>
#include <pep/assessor/ParticipantData.hpp>
#include <pep/assessor/devicehistorywidget.hpp>
#include <pep/assessor/devicewidget.hpp>
#include <pep/assessor/visitwidget.hpp>
#include <pep/assessor/VisitCaptions.hpp>
#include <pep/assessor/ButtonBar.hpp>

#include <QWidget>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidgetItem>
#include <QPushButton>
#include <filesystem>

namespace Ui {
class ParticipantWidget;
}

class ParticipantWidget : public QWidget {
  Q_OBJECT
 private:
  static const QString NO_PARTICIPANT_SID;

  std::shared_ptr<pep::Client> pepClient;
  Ui::ParticipantWidget* ui;
  MainWindow* mainWindow;

  ButtonBar* castor_buttons;

  ButtonBar* participant_buttons;
  QPushButton* edit_participant_button;
  QPushButton* release_participant_button;

  ButtonBar* print_buttons;
  QPushButton* print_stickers_button;
  QPushButton* print_oneSticker_button;

  pep::GlobalConfiguration globalConfig;
  pep::StudyContexts mAllContexts;
  pep::StudyContext mStudyContext;

  pep::PolymorphicPseudonym currentUserPP;
  ParticipantData participantData;
  pep::StudyContexts participantStudyContexts;

  int currentVisitNumber = 1;
  pep::UserRole currentPEPRole;

  bool readOnly = false;
  QString participantSID = NO_PARTICIPANT_SID;
  QString baseUrl;
  std::optional<std::filesystem::path> bartenderPath;
  std::filesystem::path stickerFilePath;
  std::vector<DeviceWidget *> mDeviceWidgets;
  std::vector<DeviceHistoryWidget *> mDeviceHistoryWidgets;
  std::vector<VisitWidget *> mVisitWidgets;
  QString summaryPrintStyle = "body {margin:10; width:90%;} \\"
                              " h1 {font-size:xx-large; text-align:center;} \\"
                              " h3 {border: 2px solid #a1a1a1; border-radius: 25px; background: #8db6d3;} \\"
                              " div{font-weight:normal; font-size:medium; text-align:left;}";
  QString infoEditStyle = "QWidget {\n"
                          " border: 0.05em solid #CA0B5E;\n"
                          " border-radius: 0.25em;\n"
                          " color: #CA0B5E;\n"
                          " padding: 0.5em;\n"
                          " font-size: 13pt;\n"
                          " outline: none;\n"
                          "}\n"
                          "QWidget:focus {\n"
                          //" border: 0.1em solid #CA0B5E;\n"
                          "}\n"
                          "QWidget:hover {\n"
                          //" background-color: rgba(202,11,94,0.8);\n"
                          " color: blue;\n"
                          "}\n"
                          "QWidget:pressed {\n"
                          " color: black;\n"
                          " border-color: grey;\n"
                          "}\n"
                          "QPushButton:disabled {\n"
                          "color: grey;\n"
                          " border-color: grey;\n"
                          "}\n";
  QString mProjectName;
  unsigned mSpareStickerCount;
  const VisitCaptions* mVisitCaptions;

 public:
  explicit ParticipantWidget(MainWindow* parent,
                             std::shared_ptr<pep::Client>,
                             QString SID,
                             const pep::Configuration& configuration,
                             const pep::GlobalConfiguration& globalConfiguration,
                             const pep::StudyContexts& allContexts,
                             const Branding& branding,
                             unsigned spareStickerCount,
                             const pep::StudyContext& studyContext,
                             const VisitCaptions* visitCaptions,
                             pep::UserRole role);

  ~ParticipantWidget() override;

  void runQuery();

 signals:

  void participantDataReceived(ParticipantData data, std::string studyContexts);

  void statusMessage(QString, pep::severity_level);

  void participantLookupError(QString, pep::severity_level);

  void queryComplete();

 private slots:
  void onParticipantDataReceived(ParticipantData data, std::string studyContexts);

  void updateDevice(QString columnName, QString deviceId);

  void closeParticipant();

  void acquireParticipant();

  void updateVisitAssessor(QString id);

  void printAllVisitStickers();

  void printSingleVisitSticker();

  void printAllParticipantStickers();

  void printSingleParticipantSticker();

  void printSummary();

  void locateBartender();

  void onTranslation();

  void openEditParticipant();

  void releaseParticipant();

  void setCurrentVisitNumber(int visitNumber);

  void editDeviceHistoryEntry(QString columnName, size_t index);

 private:
  struct ShortPseudonymListEntry {
    pep::ShortPseudonymDefinition definition;
    std::string value;
  };

  std::vector<pep::ShortPseudonymDefinition> getPrintableShortPseudonyms(const std::optional<unsigned int>& visit) const;
  void printSingleSticker(const std::optional<unsigned int>& visit);
  void invokeBartender(const std::vector<pep::ShortPseudonymDefinition>& printPseudonyms);
  QString describeShortPseudonymDefinition(const pep::ShortPseudonymDefinition& sp, bool includeVisitDescription) const;
  QString getShortPseudonymListText(const std::vector<ShortPseudonymListEntry> entries, bool includeVisitNumber) const;
  QString getVisitCaption(unsigned visitNumber) const;

  void setReadOnly(bool readOnly);
  void runQuery(bool completeRegistration);
  void processData();
  void initializeShortPseudonymsUi(const std::optional<uint32_t>& visitNumber,
    QLabel& buttonBarCaption, ButtonBar& buttonBar, QSpacerItem& spacer,
    QLabel& pseudonymsCaption, QLabel& pseudonymsLabel,
    QPushButton& printAllButton, QPushButton& printOneButton,
    QSpacerItem* pseudonymSpacerForOtherVisits = nullptr, QLabel* pseudonymsCaptionForOtherVisits = nullptr, QLabel* pseudonymsLabelForOtherVisits = nullptr);
  void appendShortPseudonymsHtmlTable(QString& htmlFormattedSummary, const std::optional<unsigned int>& visitNumber, const QString& header, const std::function<bool(const pep::ShortPseudonymDefinition&)>& includeSp);
  bool provideBartenderPath();
  void disablePrinting();
};
