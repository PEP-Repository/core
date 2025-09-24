#include <pep/application/Application.hpp>
#include <pep/assessor/Branding.hpp>
#include <pep/assessor/mainwindow.hpp>
#include <pep/assessor/enrollmentwidget.hpp>
#include <pep/assessor/loginwidget.hpp>
#include <pep/assessor/participantwidget.hpp>
#include <pep/assessor/UserRole.hpp>
#include <pep/client/Client.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/utils/Log.hpp>
#include <pep/utils/Paths.hpp>
#include <pep/gui/QTrxGui.hpp>
#include <pep/gui/InterProcess.hpp>
#include <QStyleFactory>
#include <QApplication>
#include <QMessageBox>
#include <QSizePolicy>
#include <QDir>
#include <QStandardPaths>

#include <filesystem>
#include <iostream>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif /*_WIN32*/

namespace {

class PepAssessorApplication : public pep::Application {
 private:
  static std::string GetWritableDirectory() {
    auto root = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (root.isEmpty()) {
      throw std::runtime_error("Cannot determine application data directory");
    }

    auto result= (std::filesystem::path(root.toStdString()) / "PEP").string();
    auto qresult = QString::fromStdString(result);
    if (!QDir(qresult).exists()) {
      QDir().mkdir(qresult);
    }

    return result;
  }

#ifdef _WIN32

  // Bring window to foreground based on PID: https://stackoverflow.com/a/9846519
  static BOOL CALLBACK BringToForegroundIfMatchesPid(HWND windowHandle, LPARAM lParam) {
    DWORD searchedProcessId = (DWORD)lParam;  // This is the process ID we search for (passed from BringToForeground as lParam)
    DWORD windowProcessId = 0;
    ::GetWindowThreadProcessId(windowHandle, &windowProcessId); // Get process ID of the window we just found
    if (searchedProcessId == windowProcessId) {  // Is it the process we care about?
      // Set the found window to foreground
      ::SetForegroundWindow(windowHandle);

      // Flash window (and taskbar button) to indicate it's not a normal application start
      FLASHWINFO data{ 0 };
      data.cbSize = sizeof(FLASHWINFO);
      data.hwnd = windowHandle;
      data.dwFlags = FLASHW_ALL;
      data.uCount = 3U;
      data.dwTimeout = 0U; // Default cursor blink rate
      ::FlashWindowEx(&data);

      return FALSE;  // Stop enumerating windows
    }
    return TRUE;  // Continue enumerating
  }

  static void BringToForeground(qint64 pid) {
    ::EnumWindows(&BringToForegroundIfMatchesPid, (LPARAM)pid); // TODO: ensure cast succeeds
  }

#endif

  // Single application instance check adapted from https://forum.qt.io/topic/83317/qt5-prevent-multiple-instances-of-application
  static bool EnsureOnlyInstance(QObject* application) {
    auto assessorPid = new InterProcess<qint64>("{17718724-0F06-409B-BF09-1BDD04376B1B}", QCoreApplication::applicationPid(), application); // The (randomly generated) UUID identifies this application (type)
    auto result = assessorPid->createdValue();

    if (!result) {
      auto pid = assessorPid->get();
      LOG("Startup", pep::info) << "Terminating because a pepAssessor instance is already running with PID " << pid;
#ifdef _WIN32
      BringToForeground(pid);
#endif
    }

    return result;
  }

  void prepareForExecution(QApplication& application) {
    QApplication::setStyle(QStyleFactory::create("Fusion"));

    // Without an organization name and application name the windows build crashes on ssl usage.
    // The names are also used when instantating QSettings using its default constructor.
    QCoreApplication::setOrganizationName("PEP");
    QCoreApplication::setApplicationName("PEP assessor");

    // Allow types to be passed as arguments in signal->slot
    qRegisterMetaType<pep::ParticipantDeviceHistory>("pep::ParticipantDeviceHistory");
    qRegisterMetaType<pep::severity_level>("pep::severity_level");

    pep::Configuration config = this->loadMainConfigFile();
    auto pepClient = pep::Client::OpenClient(config, nullptr, false);

    pep::Configuration projectConfig = pep::Configuration::FromFile(
      config.get<std::filesystem::path>("ProjectConfigFile"));
    auto branding = Branding::Get(projectConfig, "Branding");
    auto spareStickerCount = projectConfig.get<std::optional<unsigned>>("SpareStickerCount").value_or(0U);
    auto visitCaptionsByContext = projectConfig.get<std::optional<VisitCaptionsByContext>>("VisitCaptions")
      .value_or(VisitCaptionsByContext());

    auto mainWindow = new MainWindow(pepClient, branding, config, spareStickerCount, visitCaptionsByContext);
    mainWindow->setAttribute(Qt::WA_DeleteOnClose);

    auto loginWidget = new LoginWidget(pepClient->getIoContext(), projectConfig, config, pep::GetExecutablePath().parent_path());
    loginWidget->setAttribute(Qt::WA_DeleteOnClose);
    loginWidget->setFixedSize(loginWidget->size());

    QObject::connect(loginWidget, &LoginWidget::loginSuccess, mainWindow, &MainWindow::showForToken);

    loginWidget->show();
  }

 public:
  PepAssessorApplication() {
#if defined(_WIN32) || (defined(__APPLE__) && defined(__MACH__))
    // Executable (installation) directory may not be writable. Set it to some location that _is_ writable.
    // Doing this in the constructor so that this->run will initialize with the correct working directory.
    std::filesystem::current_path(GetWritableDirectory());
#endif
  }

 protected:
  std::optional<pep::severity_level> syslogLogMinimumSeverityLevel() const override {
    return std::nullopt;
  }

  std::string getDescription() const override {
    return "GUI for data gathering";
  }

  pep::commandline::Parameters getSupportedParameters() const override {
    return pep::Application::getSupportedParameters()
      + MakeConfigFileParameters(pep::GetResourceWorkingDirForOS(), "ClientConfig.json", false, "config"); // Maintain backward compatible "--config" switch name as an alias
  }

  int execute() override {
    //QApplication must be instantiated before being used to pass in the client config
    auto argc = this->getArgc();
    auto argv = this->getArgv();
    QApplication pepAssessor(argc, argv);

    // Terminate if there's another PEP Assessor running to prevent later failure, e.g. w.r.t. the port receiving the OAuth token after logon
    if (!EnsureOnlyInstance(&pepAssessor)) {
      return EXIT_FAILURE;
    }

    try {
      this->prepareForExecution(pepAssessor);
    }
    catch (...) {
      QMessageBox box;
      box.setWindowTitle("Could not start application"); // Don't translate: translations may not be available (depending on what raised the exception)
      box.setText(QString::fromStdString(pep::GetExceptionMessage(std::current_exception())));
      box.setIcon(QMessageBox::Critical);
      box.exec();
      return EXIT_FAILURE;
    }

    return QApplication::exec();
  }
};

}

/*! \brief Main(s). Program entry point(s)
 *
 * \return int exit code.
 */
PEP_DEFINE_MAIN_FUNCTION(PepAssessorApplication)
