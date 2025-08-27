#pragma once

#include <pep/client/Client_fwd.hpp>
#include <pep/assessor/UserRole.hpp>
#include <pep/assessor/Branding.hpp>
#include <pep/assessor/ExportableItem.hpp>
#include <pep/assessor/VisitCaptions.hpp>
#include <pep/crypto/AsymmetricKey.hpp>
#include <pep/messaging/ConnectionStatus.hpp>
#include <pep/utils/Configuration.hpp>
#include <pep/utils/Log.hpp>

#include <memory>
#include <string>

#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QMainWindow>
#include <QPushButton>
#include <QTranslator>
#include <QtWidgets/QStackedWidget>
#include <pep/assessor/enrollmentwidget.hpp>
#include <pep/assessor/participantselector.hpp>
#include <pep/assessor/exportwidget.hpp>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
  Q_OBJECT

    static const int statusMessageDuration;

  pep::ElgamalPrivateKey userPrivateKeyData;
  pep::ElgamalPrivateKey userPrivateKeyPseudonyms;
  pep::AsymmetricKey userPrivateKey;
  static QFont* tooltipFont;
  std::shared_ptr<QTranslator> currentTranslator = nullptr;
  QString enrollmentToken;
  QString currentUser;
  std::optional<pep::UserRole> currentPEPRole;
  std::shared_ptr<pep::Client> pepClient;
  pep::Configuration config;
  pep::ConnectionStatus accessManagerConnectionStatus;
  pep::ConnectionStatus keyServerConnectionStatus;
  pep::ConnectionStatus storageFacilityConnectionStatus;
  std::queue<std::pair<QString, pep::severity_level>> statusMessages;
  QTimer* statusTimer;
  QLabel* statusbarLabel;
  QPushButton* statusbarCancelButton;
  QWidget* notConnectedWidget = nullptr;
  std::shared_ptr<pep::StudyContexts> mAllContexts;
  Branding mBranding;
  unsigned mSpareStickerCount;
  EnrollmentWidget* currentEnrollmentWidget = nullptr;
  ParticipantSelector* currentSelectorWidget = nullptr;
  ExportWidget* currentExportWidget = nullptr;
  VisitCaptionsByContext mVisitCaptionsByContext;

public:
  QMap<QString, QWidget*> mOpenedParticipants;

  explicit MainWindow(std::shared_ptr<pep::Client> incommingIOObject, const Branding& branding, const pep::Configuration& config_tree, unsigned spareStickerCount, const VisitCaptionsByContext& visitCaptionsByContext);
  ~MainWindow() override;

  void showRegistrationWidget(QWidget* widget);
  void showWidget(QStackedWidget* target, QWidget* widget);
  void closeWidget(QWidget* widget);
  void changeActiveTab(int index);

signals:
  void translation();

  void announceSID(std::string SID);

  void announcePP(pep::PolymorphicPseudonym foundPP);

  void announceLookupFailure(QString reason);

  void statusMessage(QString message, pep::severity_level severity);

private slots:
  void initializeRegisterPatientContent(bool setFocus = false);

  void initializeOpenPatientContent(bool setFocus = true);

  void initializeExportContent();

  void on_participantRegistered();

  void on_participantLookupError(QString str, pep::severity_level sev);

  void on_registerWidgetClosed();

  void on_openWidgetClosed();

  void ensureFocus(int index);

  void selectByPolymorphicPseudonym(pep::PolymorphicPseudonym foundPP);

  void showPatienceWidget(QStackedWidget* target, const QString& text);

  void showParticipantData(std::string participantIdentifier);

  void updateConnectionStatus(bool expired = false);

  void updateStatus(QString message, pep::severity_level mode);

  void updateStatusBar(bool manuallyCalled = true);

  void setTitle(const std::string&);

  void loginExpired();

  void on_lookupFailure(QString reason);

  void contextComboIndexChanged(int index);

  void handleOpenByShortPseudonym(std::string sp);

public slots:
  void showForToken(QString token);
  void handleWidgetMessage(QString message, pep::severity_level severity);

private:
  void applyLanguage(QLocale::Language language);
  void clearActiveWidget(QStackedWidget* contentToClear);
  void clearAndSetWidget(QStackedWidget* contentToClear, QWidget* newActiveWidget);
  rxcpp::observable<std::map<std::string, std::string>> getParticipantData(const QList<std::shared_ptr<ExportableItem>>& items);
  void initializeTabsIfConnected();
  void openWidget(QStackedWidget* target, const std::function<void(const pep::GlobalConfiguration& config)>& callback);
  const pep::StudyContext& getCurrentStudyContext() const;
  const VisitCaptions* getVisitCaptionsForCurrentStudyContext() const;

  Ui::MainWindow* ui;
};
