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

std::optional<std::filesystem::path> GetLexedPathParameter(const commandline::LexedValues& lexed, const commandline::Parameters& definitions, const std::string& name) {
  auto definition = definitions.find(name);
  auto values = lexed.find(name);
  if (definition != nullptr && values != lexed.cend()) {
    try {
      auto parsed = definition->parse(values->second); // Applies defaults (which we want) and validates inputs (which may raise an exception)
      if (!parsed.empty()) { // The Parameter may have been defined without a ParameterValue: just a "--name" switch
        assert(parsed.count() == 1U); // Caller apparently expects that "--name" accepts a single value
        auto untyped = *parsed.begin();
        return std::any_cast<std::filesystem::path>(untyped); // Raises an exception if the ParameterValue was specified with a different type
      }
    }
    catch (...) {
      // Parameter value couldn't be extracted as a path from lexed values: we don't know its value
    }
  }
  return std::nullopt;
}

std::filesystem::path GetEffectiveConfigFilePath(const std::optional<std::filesystem::path>& dir, const std::filesystem::path& file) {
  if (file.is_absolute()) {
    return file;
  }
  if (dir.has_value()) {
    return std::filesystem::absolute(*dir / file);
  }
  return std::filesystem::absolute(file);
}

std::optional<std::filesystem::path> GetEffectiveConfigDirectory(const std::optional<std::filesystem::path>& dir, const std::optional<std::filesystem::path>& file) {
  // If a "config-file" path was specified, config will be loaded from that file's directory
  if (file.has_value()) {
    return GetEffectiveConfigFilePath(dir, *file).parent_path();
  }
  // Else if (no "config-file" was specified but) a "config-dir" was specified, config will be loaded from there
  if (dir.has_value()) {
    return std::filesystem::absolute(*dir);
  }
  // Else we dunno
  return std::nullopt;
}

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
  // Ensure that uncaught exceptions (in this or any other noexcept function) are reported before the process dies
  std::set_terminate([]() {
    if (ReportTermination(std::current_exception())) { // If we showed a message to the user...
#ifdef _WIN32
      // ...prevent Windows from showing an additional message box saying "abort() has been called". See https://stackoverflow.com/a/56282073
      _set_abort_behavior(0, _WRITE_ABORT_MSG);
#endif
    }
    std::abort();
    });

  // Since this function is marked "noexcept", the terminate handler will be invoked if implementor raises an exception
  return implementor();
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

std::filesystem::path Application::rawConfigDirectory() const {
  return this->getParameterValues().get<std::filesystem::path>("config-dir");
}

std::optional<std::filesystem::path> Application::rawConfigFile() const {
  return this->getParameterValues().getOptional<std::filesystem::path>("config-file");
}

std::filesystem::path Application::getMainConfigPath() {
  auto file = this->rawConfigFile();
  if (!file.has_value()) {
    throw std::runtime_error("No value was provided for 'config-file'");
  }

  return GetEffectiveConfigFilePath(this->rawConfigDirectory(), *file);
}

std::filesystem::path Application::getConfigDirectory() {
  if (!mConfigDirectory.has_value()) {
    mConfigDirectory = GetEffectiveConfigDirectory(this->rawConfigDirectory(), this->rawConfigFile());

    // Version info cannot be logged until the config directory is known
    if (mShowVersionInfo) {
      LogVersionInfo("configuration", ConfigVersion::TryLoad(*mConfigDirectory));
      mShowVersionInfo = false;
    }
  }
  return *mConfigDirectory;
}

Configuration Application::loadMainConfigFile() {
  [[maybe_unused]] auto dir = this->getConfigDirectory(); // Logs version info (if required) at this time
  auto file = this->getMainConfigPath();
  assert(dir == file.parent_path());
  return Configuration::FromFile(file);
}

bool Application::ReportTermination(std::exception_ptr exception) noexcept {
  try {
    std::string detail;
    if (exception != nullptr) {
      detail = "due to uncaught exception: " + GetExceptionMessage(exception);
    }
    else {
      detail = "because an unrecoverable error has occurred"; // Looks a bit better than no information at all
    }

    if (usingConsoleLog_) {
      LOG("Application", severity_level::critical) << "Terminating application " << detail;
    }
    else {
      auto channel = CreateNotificationChannel(true);
      channel->stream() << "Terminating application " << detail << std::endl;
    }
  }
  catch (...)
  {
    // Exception occurred while we tried to report the original exception. Give up.
    return false;
  }
  return true;
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
    + commandline::Parameter("loglevel", "Write log messages to stderr if they have at least this severity").value(loglevel)
    + commandline::Parameter("version", "Produce version info and exit");

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
  if (lexed.find("version") != lexed.end()){
    return printVersionInfo(lexed);
  }
  return commandline::Command::processLexedParameters(lexed);
}

int Application::printVersionInfo(const commandline::LexedValues& lexed) {
  auto supported = this->getSupportedParameters();
  auto dir = GetLexedPathParameter(lexed, supported, "config-dir");
  auto file = GetLexedPathParameter(lexed, supported, "config-file");

  auto configDir = GetEffectiveConfigDirectory(dir, file);
  if (!configDir.has_value()) {
    configDir = GetResourceWorkingDirForOS();
  }

  auto version = ConfigVersion::Current();
  if (!version.has_value()) {
    version = ConfigVersion::TryLoad(*configDir);
  }

  auto channel = CreateNotificationChannel(false);
  if (version.has_value()) {
    channel->stream() << version->prettyPrint();
    }
  else {
    channel->stream()
      << "No config version info found at: " << (*configDir / "configVersion.json").string() << "." << '\n'
      << "Running a local build?";
  }
  channel->stream()
    << '\n'
    << "Additional technical information:" << '\n'
    << BinaryVersion::current.prettyPrint();

  return EXIT_SUCCESS;
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

commandline::Parameter Application::MakeConfigDirectoryParameter(const std::filesystem::path& defaultValue, bool positional, const std::optional<std::string>& alias) {
  auto result = pep::commandline::Parameter("config-dir", "Configuration directory");
  auto value = pep::commandline::Value<std::filesystem::path>().directory().defaultsTo(defaultValue);

  if (positional) {
    assert(!alias.has_value()); // Positional parameters cannot be aliased
    value = value.positional();
  }
  else if (alias.has_value()) {
    result = result.alias(*alias);
  }

  return result.value(value);
}

commandline::Parameters Application::MakeConfigFileParameters(const std::filesystem::path& defaultDir, const std::optional<std::filesystem::path>& defaultFile, bool positional, const std::optional<std::string>& alias, const std::optional<std::string>& dirAlias) {
  // Common settings for the "config-file" parameter and its value
  auto fileParameter = pep::commandline::Parameter("config-file", "Main configuration file. May be specified relative to the --config-dir");
  auto fileValue = pep::commandline::Value<std::filesystem::path>();

  if (positional) {
    assert(!alias.has_value()); // Positional parameters cannot be aliased
    fileValue = fileValue.positional();
  }
  else if (alias.has_value()) {
    fileParameter = fileParameter.alias(*alias);
  }

  if (defaultFile.has_value()) {
    fileValue = fileValue.defaultsTo(*defaultFile);
  }
  else {
    fileValue = fileValue.required();
  }

  fileParameter = fileParameter.value(fileValue);

  return pep::commandline::Parameters()
    + MakeConfigDirectoryParameter(defaultDir, false, dirAlias)
    + fileParameter;
}

std::unique_ptr<Application::UserNotificationChannel> Application::CreateNotificationChannel(bool error) {
  return createNotificationChannel(error);
}

}
