#include <pep/assessor/mainwindow.hpp>
#include <pep/assessor/ui_mainwindow.h>
#include <pep/versioning/Version.hpp>
#include <pep/assessor/enrollmentwidget.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/assessor/exportwidget.hpp>
#include <pep/structure/GlobalConfiguration.hpp>
#include <pep/assessor/notconnectedwidget.hpp>
#include <pep/assessor/participantselector.hpp>
#include <pep/assessor/participantwidget.hpp>
#include <pep/gui/QTrxGui.hpp>
#include <pep/client/Client.hpp>

#include <rxcpp/operators/rx-filter.hpp>
#include <rxcpp/operators/rx-flat_map.hpp>
#include <rxcpp/operators/rx-map.hpp>
#include <rxcpp/operators/rx-subscribe.hpp>
#include <rxcpp/operators/rx-take.hpp>
#include <rxcpp/operators/rx-merge.hpp>

#include <pep/async/RxUtils.hpp>

#include <QDebug>
#include <QDir>
#include <QSettings>
#include <QTimer>
#include <QToolTip>

namespace {

void ClearStackedWidget(QStackedWidget& stacked) {
  // Adapted from https://stackoverflow.com/a/35136536/5862042
  while (stacked.count() != 0) {
    auto child = stacked.widget(0);
    stacked.removeWidget(child);
    child->deleteLater();
  }
}

}

QFont* MainWindow::tooltipFont = new QFont();
const int MainWindow::statusMessageDuration = 3000;

/*! \brief Mainwindow holds all widgets within the client and manages data passing between them.
 *
 * When mainwindow closes the client will terminate.
 *
 * \param client The shared pointer to the pep::Client object which mediates all network IO. This is passed to all children which need to interface with the network.
 * \param config_tree The boost::property_tree::ptree which contains the working configuration that this client instance should use. This too is passed to the children as needed.
 */
MainWindow::MainWindow(std::shared_ptr<pep::Client> client, const Branding& branding, const pep::Configuration& config_tree, unsigned spareStickerCount, const VisitCaptionsByContext& visitCaptionsByContext)
  : QMainWindow(nullptr), config(config_tree), mBranding(branding), mSpareStickerCount(spareStickerCount), mVisitCaptionsByContext(visitCaptionsByContext), ui(new Ui::MainWindow) {
  pepClient = client;
  ui->setupUi(this);
  tooltipFont->setPointSize(12);
  //ui->currentPatient->hide();
  //Remove language option

  mBranding.showLogo(*ui->icon);

  if (pep::ConfigVersion::Current() != std::nullopt) {
    if (!pep::ConfigVersion::Current()->exposesProductionData()) {
      ui->topBar->setProperty("nonrelease", true);
      ui->topBar->style()->unpolish(ui->topBar);
      ui->topBar->style()->polish(ui->topBar);
    }
  }

  ui->statusBar->hide(); // Only show it when there are messages
  statusTimer = new QTimer(this);
  statusTimer->setSingleShot(true);
  connect(statusTimer, &QTimer::timeout, [this]() {
    updateStatusBar(false);
    });
  // There is apparently no way to add these in the .ui file instead of here.
  statusbarCancelButton = new QPushButton(QString::fromWCharArray(L"\u2715"), ui->statusBar);
  statusbarLabel = new QLabel(ui->statusBar);
  ui->statusBar->addWidget(statusbarCancelButton);
  ui->statusBar->addWidget(statusbarLabel);
  QObject::connect(statusbarCancelButton, &QPushButton::clicked, this, [this]() {
    updateStatusBar(false);
    });

  applyLanguage(QLocale::Language::Dutch); //Needed in order to render dutch text over place holder text
  // Setup statusbar
  MainWindow::setTitle("PEP Assessor Client");

  boost::property_tree::ptree keys;

  //Subscribe for network status updates from the pepClient
  client->getAccessManagerConnectionStatus().observe_on(observe_on_gui()).subscribe([this](pep::ConnectionStatus accessManagerStatus) {
    accessManagerConnectionStatus = accessManagerStatus;
  updateConnectionStatus();
    });
  client->getKeyClient()->connectionStatus().observe_on(observe_on_gui()).subscribe([this](pep::ConnectionStatus keyServerStatus) {
    keyServerConnectionStatus = keyServerStatus;
  updateConnectionStatus();
    });
  client->getStorageFacilityStatus().observe_on(observe_on_gui()).subscribe([this](pep::ConnectionStatus stroageFacilityStatus) {
    storageFacilityConnectionStatus = stroageFacilityStatus;
  updateConnectionStatus();
    });
  client->getRegistrationExpiryObservable().observe_on(observe_on_gui()).subscribe([this](int) {
    loginExpired();
    });

  // TODO: ensure types are registered only once per process
  qRegisterMetaType<std::string>("std::string");
  qRegisterMetaType<pep::PolymorphicPseudonym>("pep::PolymorphicPseudonym");

  QObject::connect(this, &MainWindow::announcePP, this, &MainWindow::selectByPolymorphicPseudonym);
  QObject::connect(this, &MainWindow::announceSID, this, &MainWindow::showParticipantData);
  QObject::connect(this, &MainWindow::announceLookupFailure, this, &MainWindow::on_lookupFailure);
  QObject::connect(this, &MainWindow::statusMessage, this, &MainWindow::updateStatus);
  QObject::connect(ui->content_tabs, &QTabWidget::currentChanged, this, &MainWindow::ensureFocus);
  QObject::connect(ui->contextComboBox, SIGNAL(currentIndexChanged(int)), SLOT(contextComboIndexChanged(int)));
}

/*! \brief Mainwindows destructor
 *
 * Simple destructor which clears out the working ui object.
 */
MainWindow::~MainWindow() {
  delete ui;
}

/*! \brief Show a widget in the register content display
 *
 * Adds a widget and sets it to be the current widget on display.
 *
 * \param widget Pointer to the QWidget which should be displayed.
 */
void MainWindow::showRegistrationWidget(QWidget* widget) {
  auto target = ui->register_content;
  target->setCurrentIndex(target->addWidget(widget));
}

/*! \brief Show a widget in the specified content display
 *
 * Adds a widget and sets it to be the current widget on display.
 *
 * \param widget Pointer to the QWidget which should be displayed.
 * \param target Pointer to the QStackedWidget on which the widget should be displayed. Can be nullptr, on which it'll use the register_content Widget.
 */
void MainWindow::showWidget(QStackedWidget* target, QWidget* widget) {
  target->setCurrentIndex(target->addWidget(widget));
}

/*! \brief Close and delete widget
 *
 * Removes a widget from the display and sets it to be deleted.
 *
 * \param widget Pointer to the QWidget which should be removed.
 */
void MainWindow::closeWidget(QWidget* widget) {
  auto target = qobject_cast<QStackedWidget*>(widget->parentWidget());
  if (target != nullptr) {
    target->removeWidget(widget); //TODO: Is removing the widget necessary if it's being deleted, if not simplify function.
    widget->deleteLater();
  }
}

/*! \brief Runs selection query for the user
 *
 * Selects a participant for display based on the SID input by the user. SID is set in the ParticipantSelector widget.
 */
void MainWindow::initializeOpenPatientContent(bool setFocus) {
  //Remove old layout and child widgets that might be hanging around
  auto currentStackedWidget = ui->open_content;
  if (!currentPEPRole) {
    showPatienceWidget(currentStackedWidget, "Not allowed to search for participant data.");
    return;
  }

  ClearStackedWidget(*currentStackedWidget);

  openWidget(currentStackedWidget, [this, setFocus, currentStackedWidget](const pep::GlobalConfiguration& config) {
    ParticipantSelector* selectBySIDorPseudonym = new ParticipantSelector(this, config);
  QObject::connect(selectBySIDorPseudonym, &ParticipantSelector::cancelled, selectBySIDorPseudonym, [this, currentStackedWidget]() {
    std::cerr << "Over here now for some reason" << std::endl;
  clearActiveWidget(currentStackedWidget);
    });
  QObject::connect(selectBySIDorPseudonym, &ParticipantSelector::participantSidSelected, this, &MainWindow::showParticipantData);
  QObject::connect(selectBySIDorPseudonym, &ParticipantSelector::participantShortPseudonymSelected, this, &MainWindow::handleOpenByShortPseudonym);
  currentSelectorWidget = selectBySIDorPseudonym;
  currentStackedWidget->addWidget(selectBySIDorPseudonym);
  if (setFocus)
    selectBySIDorPseudonym->doFocus();
    });
}

void MainWindow::openWidget(QStackedWidget* target, const std::function<void(const pep::GlobalConfiguration& config)>& callback) {
  auto processed = std::make_shared<bool>(false);
  pepClient->getGlobalConfiguration()
    .observe_on(observe_on_gui())
    .subscribe(
      [callback, processed](std::shared_ptr<pep::GlobalConfiguration> configuration) {
        if (*processed) {
          throw std::runtime_error("Received multiple global configurations");
        }
  callback(*configuration);
  *processed = true;
      },
      [this, processed, target](std::exception_ptr ep) {
        if (*processed) {
          clearActiveWidget(target);
        }
      emit statusMessage(tr("Cannot open widget: %1").arg(QString::fromStdString(pep::GetExceptionMessage(ep))), pep::error);
      },
        [this, processed, target]() {
        if (!processed) {
          clearActiveWidget(target);
          emit statusMessage(tr("Global configuration not received"), pep::error);
        }
      });
}

const pep::StudyContext& MainWindow::getCurrentStudyContext() const {
  if (mAllContexts->getItems().size() > 1U) {
    return mAllContexts->getItems()[static_cast<unsigned int>(ui->contextComboBox->currentIndex())];
  }
  return *mAllContexts->getDefault();
}

const VisitCaptions* MainWindow::getVisitCaptionsForCurrentStudyContext() const {
  const auto& context = this->getCurrentStudyContext();
  auto position = mVisitCaptionsByContext.find(context.getId());
  if (position == mVisitCaptionsByContext.cend()) {
    return nullptr;
  }
  return &position->second;
}

void MainWindow::handleWidgetMessage(QString message, pep::severity_level severity) {
  emit statusMessage(message, severity);
}

void MainWindow::initializeExportContent() {
  auto currentStackedWidget = ui->export_content;
  if (!currentPEPRole) {
    showPatienceWidget(currentStackedWidget, "Not allowed to export data.");
    return;
  }

  ClearStackedWidget(*currentStackedWidget);

  openWidget(currentStackedWidget, [this, currentStackedWidget](const pep::GlobalConfiguration& configuration) {
    auto widget = new ExportWidget(configuration, getCurrentStudyContext(), *currentPEPRole, this->mVisitCaptionsByContext, pepClient, this);
  QObject::connect(widget, &ExportWidget::sendMessage, this, &MainWindow::handleWidgetMessage);
  currentExportWidget = widget;
  currentStackedWidget->addWidget(widget);
    });
}

void MainWindow::contextComboIndexChanged(int index) {
  // Store selected context so we can preselect it the next time
  QSettings().setValue("StudyContext", QString::fromStdString(getCurrentStudyContext().getId()));

  // Participants in open widgets may not be available in new context
  for (const auto& widget : mOpenedParticipants) {
    int index = ui->content_tabs->indexOf(widget);
    auto tabToRemove = ui->content_tabs->widget(index);
    ui->content_tabs->removeTab(index);
    tabToRemove->deleteLater();
  }
  mOpenedParticipants.clear();

  initializeTabsIfConnected();
}

void MainWindow::on_participantRegistered() {
  updateStatus(tr("Completing participant registration"), pep::info);
}

void MainWindow::showForToken(QString token) {
  enrollmentToken = token;
  ui->user->setText(tr("Not connected"));
  showPatienceWidget(ui->register_content, tr("Connecting to servers..."));
  showMaximized();

  pepClient->enrollUser(token.toStdString())
    .flat_map([this](pep::EnrollmentResult result) {
    return this->pepClient->getGlobalConfiguration()
    .map([enrollment = std::make_shared<pep::EnrollmentResult>(result)](std::shared_ptr<pep::GlobalConfiguration> config) {
        return std::make_pair(enrollment, config);
      });
      })
    .observe_on(observe_on_gui())
        .subscribe([this](const std::pair<std::shared_ptr<pep::EnrollmentResult>, std::shared_ptr<pep::GlobalConfiguration>> pair) {
        std::cout << "Received EnrollmentResult" << std::endl;
      auto& cert = pair.first->signingIdentity.getCertificateChain().front();

      if(!cert.getCommonName().has_value()) {
        throw std::runtime_error("User certificate does not contain a username.");
      }
      if(!cert.getOrganizationalUnit().has_value()) {
        throw std::runtime_error("User certificate does not contain a user group.");
      }
      auto user = QString::fromStdString(cert.getCommonName().value());
      auto role = QString::fromStdString(cert.getOrganizationalUnit().value());

      std::cout << "user = " << user.toStdString() << std::endl;
      std::cout << "role = " << role.toStdString() << std::endl;

      enrollmentToken.clear();
      currentUser = user;
      currentPEPRole = pep::UserRole::GetForOAuthRole(role.toStdString());

      mAllContexts = std::make_shared<pep::StudyContexts>(pair.second->getStudyContexts());

      if (mAllContexts->getItems().size() > 1U) {
        auto defaultContext = mAllContexts->getDefault();
        assert(defaultContext != nullptr);
        auto setting = QSettings().value("StudyContext");
        auto selectId = setting.isNull() ? defaultContext->getId() : setting.toString().toStdString();

        //Set UI elements
        for (const auto& context : mAllContexts->getItems()) {
          auto id = context.getId();
          ui->contextComboBox->addItem(QString::fromStdString(id));
          if (id == selectId) {
            ui->contextComboBox->setCurrentIndex(ui->contextComboBox->count() - 1);
          }
        }
        ui->user->setText(tr("logged-in-as %1 (%2) for context").arg(user, role));
      }
      else {
        ui->contextComboBox->setVisible(false);
        ui->user->setText(tr("logged-in-as %1 (%2)").arg(user, role));
      }

      clearActiveWidget(ui->register_content);
      updateConnectionStatus();
          }, [this](std::exception_ptr ep) {
            qDebug() << "Exception occured: " << QString::fromStdString(pep::GetExceptionMessage(ep));
          updateConnectionStatus();
          }, [] {
            std::cout << "Enrollment done" << std::endl;
          });
}

/*! \brief Notify the user that some time consuming process is taking place
*
* Clears the screen and displays a message.
*/
void MainWindow::showPatienceWidget(QStackedWidget* target, const QString& text) {
  QLabel* infiniteProgress = new QLabel(this);
  infiniteProgress->setText(text);
  infiniteProgress->setAlignment(Qt::AlignCenter);
  clearAndSetWidget(target, infiniteProgress);
}

/*! \brief Query for participant data and display
*
* Accepts a std::string as input from a previously validated field and attempts to match it against known short pseudonyms. If successful then the participant's data are loaded and the user can work.
* In the event of an error the main area is cleared and user is informed.
*
* \param shortPseudonym Short pseudonym which is the key for the lookup query.
*/
void MainWindow::handleOpenByShortPseudonym(std::string shortPseudonym) {
  auto currentStackedWidget = ui->open_content;
  showPatienceWidget(currentStackedWidget, "Searching...");

  updateStatus(tr("Searching for short pseudonym %1").arg(QString::fromStdString(shortPseudonym)), pep::severity_level::info);
  pepClient->findPPforShortPseudonym(shortPseudonym, getCurrentStudyContext()).subscribe(
    [this](pep::PolymorphicPseudonym pp) {
      emit announcePP(pp);
    },
    [this, shortPseudonym](std::exception_ptr ep) {
      try {
      std::rethrow_exception(ep);
    }
    catch (const rxcpp::empty_error&) {
      emit announceLookupFailure(tr("Short pseudonym '%1' not found").arg(QString::fromStdString(shortPseudonym)));
    }
    catch (const pep::ShortPseudonymFormatError&) {
      emit announceLookupFailure(tr("'%1' does not look like a short pseudonym").arg(QString::fromStdString(shortPseudonym)));
    }
    catch (const pep::ShortPseudonymContextError&) {
      emit announceLookupFailure(tr("Short pseudonym '%1' is not available in the current (%2) context.").arg(QString::fromStdString(shortPseudonym),
        QString::fromStdString(getCurrentStudyContext().getId())));
    }
    catch (const std::exception& e) {
      emit announceLookupFailure(e.what());
    }
    });
}

/*! \brief Query for participant data and display
*
* Accepts a std::string as input from a previously validated field and attempts to select based on this. If successful then the main widget is set and the user can work.
* In the event of an error the main area is cleared and user is informed.
*
* \param participantIdentifier Participant ID which is the key for the lookup query.
*/
void MainWindow::showParticipantData(std::string participantIdentifier) {
  auto currentStackedWidget = ui->register_content;
  if (!currentPEPRole) {
    showPatienceWidget(currentStackedWidget, "Not allowed to view participant data.");
    return;
  }

  QStackedWidget* widgets[] = { ui->register_content, ui->open_content };

  for (auto widget : widgets) {
    showPatienceWidget(widget, "Loading...");
  }

  openWidget(widgets[0], [this, participantIdentifier, widgets](const pep::GlobalConfiguration& globalConfiguration) {
    auto selector = new ParticipantWidget(this, pepClient, QString::fromStdString(participantIdentifier), config, globalConfiguration, *mAllContexts, mBranding, mSpareStickerCount, getCurrentStudyContext(), getVisitCaptionsForCurrentStudyContext(), *currentPEPRole);
  selector->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
  selector->setVisible(false);
  QObject::connect(this, SIGNAL(translation()), selector, SLOT(onTranslation()));
  QObject::connect(selector, &ParticipantWidget::queryComplete, [this, selector, participantIdentifier, widgets]() {
    QString participantSID = QString::fromStdString(participantIdentifier);

  auto checkOpened = mOpenedParticipants.find(participantSID);
  if (checkOpened != mOpenedParticipants.end()) {
    //Remove participant that was already opened.
    int index = ui->content_tabs->indexOf(checkOpened.value());
    auto tabToRemove = ui->content_tabs->widget(index);
    ui->content_tabs->removeTab(index);
    mOpenedParticipants.remove(participantSID);
    tabToRemove->deleteLater();
  }

  auto newTab = new QStackedWidget(ui->content_tabs);
  newTab->addWidget(selector);
  mOpenedParticipants.insert(participantSID, newTab);

  int newIndex = ui->content_tabs->addTab(newTab, participantSID);
  ui->content_tabs->setCurrentIndex(newIndex);

  //Clear patienceWidgets
  for (auto widget : widgets) {
    clearActiveWidget(widget);
  }
    });
  QObject::connect(selector, &ParticipantWidget::participantLookupError, this, &MainWindow::on_participantLookupError);
  QObject::connect(selector, SIGNAL(statusMessage(QString, pep::severity_level)), this, SLOT(updateStatus(QString, pep::severity_level)));
  selector->runQuery();
    });
}

void MainWindow::on_lookupFailure(QString reason) {
  clearActiveWidget(ui->open_content);
  MainWindow::updateStatus(reason, pep::error);
  ensureFocus(0);
}

void MainWindow::selectByPolymorphicPseudonym(pep::PolymorphicPseudonym foundPP) {
  auto sid = std::make_shared<std::string>();

  pep::enumerateAndRetrieveData2Opts opts;
  opts.pps = { foundPP };
  opts.columns = { "ParticipantIdentifier" };
  pepClient->enumerateAndRetrieveData2(opts)
    .subscribe([sid](const pep::EnumerateAndRetrieveResult& result) {
    if (!sid->empty()) {
      throw std::runtime_error("Multiple identifiers found for participant");
    }
  *sid = result.mData;
      }, [this](std::exception_ptr ep) {
        emit announceLookupFailure(QString::fromStdString(pep::GetExceptionMessage(ep)));
      }, [this, sid]() {
        if (sid->empty()) {
          emit announceLookupFailure("Identifier for this participant was not stored yet. Please open the participant's details to complete storage.");
        }
        else {
          emit announceSID(*sid);
        }
      });
}

void MainWindow::on_participantLookupError(QString str, pep::severity_level sev) {
  clearActiveWidget(ui->register_content);
  clearActiveWidget(ui->open_content);
  updateStatus(str, sev);
  ensureFocus(0);
}

/*! \brief Begins the enrollment process for a new participant
 *
 * Run when the enroll button is pressed this ensures that the current user has the privilage to enroll a new participant and begins the process.
 */
void MainWindow::initializeRegisterPatientContent(bool setFocus) {
  auto currentStackedWidget = ui->register_content;

  auto allow = currentPEPRole.has_value() && currentPEPRole->canRegisterParticipants();
  if (!allow) {
    showPatienceWidget(currentStackedWidget, "Not allowed to register participants.");
  }
  else {
    ClearStackedWidget(*currentStackedWidget);

    EnrollmentWidget* enroll = new EnrollmentWidget(pepClient, this, getCurrentStudyContext());
    QObject::connect(enroll, &EnrollmentWidget::cancelled, this, [this, currentStackedWidget]() {
      clearActiveWidget(currentStackedWidget);
      });
    QObject::connect(enroll, &EnrollmentWidget::enrollConfirmed, this, [this, currentStackedWidget]() {
      showPatienceWidget(currentStackedWidget, "Loading...");
      });
    QObject::connect(enroll, &EnrollmentWidget::enrollComplete, this, &MainWindow::showParticipantData);
    QObject::connect(enroll, SIGNAL(enrollFailed(QString, pep::severity_level)), this,
      SLOT(updateStatus(QString, pep::severity_level)));
    QObject::connect(enroll, &EnrollmentWidget::participantRegistered, this, &MainWindow::on_participantRegistered);
    currentEnrollmentWidget = enroll;
    currentStackedWidget->addWidget(enroll);
    if (setFocus)
      enroll->doFocus();
  }
}

/*! \brief Parameters language (NO LONGER USED).
 *
 * This toggles active language between dutch and english (NO LONGER USED).
 */
void MainWindow::applyLanguage(QLocale::Language language) {
  // Construct new locale
  auto currentLocale = QLocale();
  QLocale newLocale(language, currentLocale.territory());

  // Extract two-letter ISO language code from constructed locale. Code adapted from https://stackoverflow.com/q/24109270
  auto iso = newLocale.name().split('_').at(0).toLower();
  if (iso.length() != 2) {
    throw std::runtime_error("Could not determine ISO code for language");
  }

  // Load the translation file for the new language
  auto path = QDir(":/i18n").filePath("pep_" + iso + ".qm");
  auto newTranslator = std::make_shared<QTranslator>();
  if (!newTranslator->load(path)) {
    throw std::runtime_error(("Could not load translation file for language '" + iso + "'").toStdString());
  }

  // Apply the new translator
  auto current = currentTranslator.get();
  if (current != nullptr) {
    QCoreApplication::removeTranslator(current);
  }
  currentTranslator = newTranslator;
  QCoreApplication::installTranslator(newTranslator.get());

  // Ensure QT in its entirety plays along nicely (e.g. translating calendar popups to the correct language)
  QLocale::setDefault(newLocale);

  // Update UI
  ui->retranslateUi(this);
  emit translation();
}

/*! \brief Makes connection status known to the user
 *
 * It's possible for the client to drop connection with one or more of the pep servers and in that event the user will be unable to work.
 * This function informs the user that there is a problem and prevents further work until the connection is restored.
 *
 * \param expired True if the user has an expired session. Default = false.
 */
void MainWindow::updateConnectionStatus(bool expired /* = false */) {
  if (!this->isVisible())
    return;

  // First remove any previous not-connected message, whether or not we are now connected
  if (notConnectedWidget != nullptr) {
    closeWidget(notConnectedWidget);
    notConnectedWidget = nullptr;
  }

  if (expired || !enrollmentToken.isEmpty() // Enrollment expired or initial enrollment failed
    || !accessManagerConnectionStatus.connected || !keyServerConnectionStatus.connected || !storageFacilityConnectionStatus.connected) { //Check server status
    notConnectedWidget = new NotConnectedWidget(accessManagerConnectionStatus, keyServerConnectionStatus,
      storageFacilityConnectionStatus, ui->root_content);
    showWidget(ui->root_content, notConnectedWidget);
  }
  initializeTabsIfConnected();
}

void MainWindow::initializeTabsIfConnected() {
  if (accessManagerConnectionStatus.connected && keyServerConnectionStatus.connected && storageFacilityConnectionStatus.connected) {
    initializeRegisterPatientContent();
    initializeOpenPatientContent();
    initializeExportContent();
    ui->content_tabs->setCurrentIndex(0);
  }
}

/*! \brief Helper function for an expired login session.
 *
 * Calls MainWindow::updateConnectionStatus with a boolean value of true to disable the session.
 */

void MainWindow::loginExpired() {
  assert(enrollmentToken.isEmpty());

  currentUser.clear();
  currentPEPRole = std::nullopt;

  updateConnectionStatus(true);
}

void MainWindow::updateStatus(QString message, pep::severity_level mode) {
  statusMessages.emplace(std::make_pair(message, mode));
  qDebug() << QStringLiteral("Queueing status message: %1").arg(message);
  updateStatusBar();
}

/*! \brief Update the status bar
 *
 * It can be helpful to pass status information to the UI and this function does that.
 *
 * \param manuallyCalled If true and statusTimer is working then this function does nothing. Useful should statusTimer ever fail.
 */
void MainWindow::updateStatusBar(bool manuallyCalled /* = true */) {
  if (manuallyCalled && statusTimer->isActive()) {
    return; // Just wait till the timer fires
  }

  QStatusBar* bar = ui->statusBar;
  if (statusMessages.empty()) {
    bar->hide();
    statusTimer->stop();
  }
  else {
    std::pair<QString, pep::severity_level> msg = statusMessages.front();
    statusMessages.pop();
    switch (msg.second) {
    case pep::severity_level::debug:
    case pep::severity_level::info:
      bar->setProperty("class", "info");
      break;
    case pep::severity_level::warning:
      bar->setProperty("class", "warning");
      break;
    case pep::severity_level::error:
    case pep::severity_level::critical:
      bar->setProperty("class", "error");
      break;
    default:
      break;
    }

    // http://lists.qt-project.org/pipermail/interest/2013-October/009482.html
    // Qt doesn't automatically redraw a widget when its CSS class gets updated.
    // This isn't very pretty, but there is no pretty way to deal with this.
    QWidget* widgets[] = { ui->statusBar, statusbarCancelButton };
    for (QWidget* widget : widgets) {
      widget->style()->unpolish(widget);
      widget->style()->polish(widget);
      widget->update();
    }

    statusbarLabel->setText(msg.first);
    bar->show();
    statusTimer->start(statusMessageDuration);
  }
}

/*! \brief Set client UI title
 *
 * Takes a string and sets the client UI title with it and build information.
 *
 * \param newTitle Text that should be displayed along with build information.
 */
void MainWindow::setTitle(const std::string& newTitle) {
  auto version = pep::ConfigVersion::Current();
  if (version != std::nullopt && version->isGitlabBuild()) {
    MainWindow::setWindowTitle(QString::fromStdString(newTitle + " - " + version->getSummary()));
  }
  else {
    MainWindow::setWindowTitle(QString::fromStdString(newTitle) + " - internal version (not built in GitLab)");
  }
}

/*! \brief Clears the currently active widget
 *
 * The main window should only have a single widget active at a time. This function facilitates that by allowing for easy removal of whatever is currently active.
 */
void MainWindow::clearActiveWidget(QStackedWidget* contentToClear) {
  if (QWidget* oldWidget = contentToClear->currentWidget()) {
    oldWidget->setVisible(false);
    oldWidget->deleteLater();
  }
}

/*! \brief Clears active widget and sets a new one to be active
 *
 * Boths clears the current widget from the main window and sets a new widget to be active.
 *
 * \param newActiveWidget The new widget to be set as active.
 */
void MainWindow::clearAndSetWidget(QStackedWidget* contentToClear, QWidget* newActiveWidget) {
  if (contentToClear->widget(contentToClear->currentIndex()) == newActiveWidget) {
    // Same widget
    return;
  }

  clearActiveWidget(contentToClear);
  int currentIndex = contentToClear->addWidget(newActiveWidget);
  contentToClear->setCurrentIndex(currentIndex);
}

void MainWindow::on_registerWidgetClosed() {
  bool needsFocus = (ui->content_tabs->currentIndex() == 1);
  if (ui->register_content->count() == 0) {
    initializeRegisterPatientContent(needsFocus);
  }
}

void MainWindow::on_openWidgetClosed() {
  bool needsFocus = (ui->content_tabs->currentIndex() == 0);
  if (ui->open_content->count() == 0) {
    initializeOpenPatientContent(needsFocus);
  }
}

void MainWindow::ensureFocus(int index) {
  switch (index) {
  case 0:
    if (ui->open_content->currentWidget() == currentSelectorWidget) {
      currentSelectorWidget->doFocus();
    }
    else {
      initializeOpenPatientContent(true);
    }
    break;
  case 1:
    if (ui->register_content->currentWidget() == currentEnrollmentWidget) {
      currentEnrollmentWidget->doFocus();
    }
    else {
      initializeRegisterPatientContent();
    }
    break;
  case 2:
    if (ui->export_content->currentWidget() == currentExportWidget) {
      currentExportWidget->doFocus();
    }
    else {
      initializeExportContent();
    }
    break;
  default:
    break;
  }
}

void MainWindow::changeActiveTab(int index) {
  ui->content_tabs->setCurrentIndex(index);
}
