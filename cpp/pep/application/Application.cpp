#include <pep/application/Application.hpp>
#include <pep/application/Unwinder.hpp>
#include <pep/versioning/Version.hpp>
#include <pep/utils/Defer.hpp>
#include <pep/utils/Exceptions.hpp>
#include <pep/utils/WorkingDirectory.hpp>
#include <pep/utils/Paths.hpp>

#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>

#include <filesystem>

#include <boost/exception/diagnostic_information.hpp>

#ifdef _WIN32
#include <sstream>
#include <typeinfo>
#include <pep/utils/Win32Api.hpp>

#include <shellapi.h>
#else
#include <unistd.h>
#endif

namespace pep {

namespace {

const std::string CONSOLE_REDIRECTION_WARNING = "Note that output cannot be piped or redirected (e.g. to file) in this mode.";

void LogVersionInfo(const std::string& tag, std::string summary) {
  if (summary.empty()) {
    summary = "No version information available. Running a local build?";
  }
  LOG("Application " + tag, info) << summary;
}

template <typename T>
void LogVersionInfo(const std::string& tag, const std::optional<T>& version) {
  if (!version) {
    LogVersionInfo(tag, std::string());
  }
  else {
    LogVersionInfo(tag, version->getSummary());
  }
}

std::ostream& GetStdioNotificationStream(bool error) noexcept {
  return error ? std::cerr : std::cout;
}

class StdioNotificationChannel : public Application::UserNotificationChannel {
private:
  std::ostream& mStream;

public:
  explicit StdioNotificationChannel(bool error) noexcept : mStream(GetStdioNotificationStream(error)) {}
  ~StdioNotificationChannel() noexcept override { mStream.flush(); }
  inline std::ostream& stream() override { return mStream; }
};

#ifdef _WIN32
class MessageBoxNotificationChannel : public Application::UserNotificationChannel {
private:
  bool mError;
  std::ostringstream mContent;

public:
  explicit MessageBoxNotificationChannel(bool error) noexcept : mError(error) {}
  inline std::ostream& stream() override { return mContent; }

  ~MessageBoxNotificationChannel() noexcept override {
    auto message = mContent.str();

    // Write message to stdio so that it can be piped or redirected
    auto& destination = GetStdioNotificationStream(mError);
    destination << message;
    destination.flush();

    // TODO: use fixed-width font in the dialog so that output (e.g. from --help) is aligned properly
    auto display = message +
      "\n"
      "\n" "If this notification's formatting looks corrupted, please view it using a fixed-width font, e.g. by"
      "\n" "- copying it (Ctrl+C) and pasting it to a text editor, or"
      "\n" "- redirecting application output to a file and viewing that file in a text editor, or"
      "\n" "- invoking the application from a command line and passing the --bind-to-console switch.";
    UINT icon = mError ? MB_ICONERROR : MB_ICONINFORMATION;
    ::MessageBoxA(nullptr, display.c_str(), "Application", MB_OK | MB_APPLMODAL | icon);
  }
};

/*
* Since (Windows or console) build subsystem is a(n MSVC) linker property, we can't detect it at compile time.
* Instead we set this run time flag if the WinMain entry point is used.
* See https://docs.microsoft.com/en-us/cpp/build/reference/subsystem-specify-subsystem?view=msvc-170
*/
bool runningOnWindowsSubsystem = false;
std::unique_ptr<win32api::ParentConsoleBinding> parentConsoleBinding;
#endif

template <typename TChannel>
std::unique_ptr<Application::UserNotificationChannel> CreateTypedNotificationChannel(bool error) {
  return std::make_unique<TChannel>(error);
}

typedef std::unique_ptr<Application::UserNotificationChannel>(*NotificationChannelFactory)(bool);
NotificationChannelFactory createNotificationChannel = &CreateTypedNotificationChannel<StdioNotificationChannel>;

}

Application* Application::instance_ = nullptr;
bool Application::usingConsoleLog_ = false;

Application::Application() {
  if (instance_ != nullptr) {
    throw std::runtime_error("Only a single Application instance may exist over the process's lifetime");
  }
  instance_ = this;
}

Application::~Application() noexcept {
#ifdef _WIN32
  parentConsoleBinding.reset();
#endif
}

std::string Application::getName() const {
  return GetExecutablePath().filename().string();
}

int Application::getArgc() const {
  if (mArgc < 0) {
    throw std::runtime_error("Main function parameters may not be retrieved until the execute() method is invoked");
  }
  return mArgc;
}

char** Application::getArgv() const {
  if (mArgv == nullptr) {
    throw std::runtime_error("Main function parameters may not be retrieved until the execute() method is invoked");
  }
  return mArgv;
}

int Application::RunWithoutError(std::function<int()> implementor) noexcept {
    try {
      return implementor();
    }
    catch (...) {
      ReportTermination(GetExceptionMessage(std::current_exception()));
    }
    return EXIT_FAILURE;
}

int Application::run(int argc, char* argv[]) {
#ifdef _WIN32
  // The active code page should've been set by our windows-app.*.manifest
  if (GetACP() != CP_UTF8) {
    throw std::runtime_error("Code page should have been set to UTF-8 by manifest");
  }
  // Make sure std streams support in-/outputting UTF-8.
  // For some reason leaving it as CP_ACP doesn't work
  SetConsoleCP(CP_UTF8);
  SetConsoleOutputCP(CP_UTF8);
#endif

  if (useUnwinder()) {
    InitializeUnwinder();
  }

  std::queue<std::string> args;
  std::for_each(argv + 1, argv + argc, [&args](const char* arg) {args.push(arg); });

  mArgc = argc;
  mArgv = argv;
  return this->process(args);
}

void Application::tryLoadConfigVersion(const std::filesystem::path& directory) {
  if (mShowVersionInfo) {
    LogVersionInfo("configuration", ConfigVersion::TryLoad(directory));
    mShowVersionInfo = false;
  }
}

Configuration Application::loadMainConfigFile(const std::filesystem::path& path) {
  this->tryLoadConfigVersion(path.parent_path());
  return Configuration::FromFile(path);
}

void Application::ReportTermination(const std::string& reason) {
  std::string detail;
  if (!reason.empty()) {
    detail += ": " + reason;
  }

  if (usingConsoleLog_) {
    LOG("Application", severity_level::critical) << "Terminating application due to uncaught exception" << detail;
  } else {
    auto channel = CreateNotificationChannel(true);
    channel->stream() << "Terminating application due to uncaught exception" << detail << std::endl;
  }
}

#ifdef _WIN32

// Helper class to convert a number of wide strings to the char *argv[] expected by Application.execute()
class MainFunctionArguments {
  std::vector<std::string> argStrings_;
  std::vector<char*> argv_;

 public:
   MainFunctionArguments(int argc, LPWSTR* wideArgv) {
     assert(argc >= 0);
     assert(wideArgv != nullptr);

     for (int i = 0; i < argc; i++) {
       auto wide = wideArgv[i];
       assert(wide != nullptr);
       argStrings_.emplace_back(win32api::WideStringToUtf8(wide));
     }

     argv_.reserve(argStrings_.size());
     std::transform(argStrings_.begin(), argStrings_.end(), std::back_inserter(argv_), [](std::string& argString) {return argString.data(); });

     argv_.emplace_back(nullptr); // C++ standard requires that "The value of argv[argc] shall be 0": see https://timsong-cpp.github.io/cppwp/basic.start.main
   }

  int argc() const noexcept {
    return static_cast<int>(argStrings_.size());
  }

  char** argv() noexcept {
    return argv_.data();
  }
};

int Application::InvokeWithArgcArgv(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd, std::function<int(int, char**)> invoke) {
  runningOnWindowsSubsystem = true;

  int argc;
  LPWSTR* wideArgv = ::CommandLineToArgvW(GetCommandLineW(), &argc);
  if (wideArgv == nullptr) {
    win32api::ApiCallFailure::RaiseLastError();
  }
  PEP_DEFER(::LocalFree(wideArgv));

  MainFunctionArguments arguments(argc, wideArgv);
  assert(argc == arguments.argc());

  return invoke(argc, arguments.argv());
}

#endif

bool Application::useUnwinder() const {
#ifdef WITH_UNWINDER
  return true;
#else
  return false;
#endif
}

std::optional<severity_level> Application::syslogLogMinimumSeverityLevel() const {
  return severity_level::info;
}

std::optional<severity_level> Application::consoleLogMinimumSeverityLevel() const {
  return severity_level::warning;
}

std::optional<severity_level> Application::fileLogMinimumSeverityLevel() const {
  return severity_level::warning;
}

commandline::Parameters Application::getSupportedParameters() const {
  auto loglevel = commandline::Value<std::string>().allow(Logging::SeverityLevelNames());
  auto defaultValue = this->consoleLogMinimumSeverityLevel();
  if (defaultValue.has_value()) {
    loglevel = loglevel.defaultsTo(Logging::FormatSeverityLevel(*defaultValue));
  }
  auto result = commandline::Command::getSupportedParameters()
    + commandline::Parameter("suppress-version-info", "Don't log (" + Logging::FormatSeverityLevel(info) + "-level messages with) version details")
    + commandline::Parameter("loglevel", "Write log messages to stderr if they have at least this severity").value(loglevel);

#ifdef _WIN32
  if (runningOnWindowsSubsystem) {
    result = result + commandline::Parameter("bind-to-console", "Send output to parent console instead of stdio. " + CONSOLE_REDIRECTION_WARNING);
  }
#endif

  return result;
}

std::optional<int> Application::processLexedParameters(const commandline::LexedValues& lexed) {
#ifdef _WIN32
  if (runningOnWindowsSubsystem) {
    if (lexed.find("bind-to-console") != lexed.cend()) {
      parentConsoleBinding = win32api::ParentConsoleBinding::TryCreate();
      if (parentConsoleBinding != nullptr) {
        std::cerr << '\n' // Don't write on the line containing the next user prompt
          << "The " << this->getName() << " application will write its stdio output to this console\n"
          << "because it was invoked with the --bind-to-console command line switch.\n"
          << CONSOLE_REDIRECTION_WARNING << '\n'
          << std::endl;
      }
    }

    if (parentConsoleBinding == nullptr) {
      // We didn't (or failed to) bind to the parent console, but we'll still want to show notifications -> use a message box instead of stdio
      createNotificationChannel = &CreateTypedNotificationChannel<MessageBoxNotificationChannel>;
    }
  }
#endif
  return commandline::Command::processLexedParameters(lexed);
}

void Application::finalizeParameters() {
  Command::finalizeParameters();

  const auto& values = this->getParameterValues();
  mShowVersionInfo = !values.has("suppress-version-info");

  std::vector<std::shared_ptr<Logging>> logging;

  std::optional<severity_level> console_level;
  if (values.has("loglevel")) {
    console_level = Logging::ParseSeverityLevel(values.get<std::string>("loglevel"));
  }
  if (!console_level) {
    console_level = consoleLogMinimumSeverityLevel();
  }
  if (console_level) {
    logging.push_back(std::make_shared<ConsoleLogging>(*console_level));
    usingConsoleLog_ = true;
  }

  auto file_level = fileLogMinimumSeverityLevel();
  if (file_level) {
    logging.push_back(std::make_shared<FileLogging>(*file_level));
  }

  auto syslog_level = syslogLogMinimumSeverityLevel();
  if (syslog_level) {
    logging.push_back(std::make_shared<SysLogging>(*syslog_level));
  }

  Logging::Initialize(logging);

  if (mShowVersionInfo) {
    LogVersionInfo("binary", BinaryVersion::current.getSummary());
  }
}

std::unique_ptr<Application::UserNotificationChannel> Application::CreateNotificationChannel(bool error) {
  return createNotificationChannel(error);
}

}
