#pragma once

#include <optional>

#include <boost/type_traits/is_base_of.hpp>

#include <pep/application/CommandLineCommand.hpp>
#include <pep/utils/Configuration.hpp>
#include <pep/utils/Log.hpp>

#define PEP_DEFINE_C_MAIN_FUNCTION(applicationType) \
  int main(int argc, char* argv[]) { \
    return pep::Application::Run< applicationType >(argc, argv); \
  }

/* Windows discriminates between the "subsystem" that an application is compiled for:
 * - UI applications use the Microsoft specific `WinMain` function as their entry point.
 * - Console applications use the C standard `main` function as their entry point.
 * So on Windows we need to define a different "main" (entry point) function depending on
 * the subsystem, but there is no standard macro to distinguish between them.
 * (Instead the subsystem is determined at link time with /SUBSYSTEM switch.)
 * So on Windows we just define both a `main` and a `WinMain` function. According to
 * https://stackoverflow.com/a/4839739/5862042 , the linker will then find the function
 * appropriate for the subsystem and "ignore" the other one (presumably discarding it).
 */
// Do not place this under the pep namespace
#ifdef _WIN32
#include <pep/utils/Platform.hpp>

# define PEP_DEFINE_MAIN_FUNCTION(applicationType) \
    PEP_DEFINE_C_MAIN_FUNCTION(applicationType) \
    \
    int ::WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) { \
      return pep::Application::Run< applicationType >(hInstance, hPrevInstance, lpCmdLine, nShowCmd); \
    }
#else
# define PEP_DEFINE_MAIN_FUNCTION(applicationType) \
    PEP_DEFINE_C_MAIN_FUNCTION(applicationType)
#endif


namespace pep {

class Application : public commandline::Command {
 private:
  static Application* instance_;
  static bool usingConsoleLog_;

  int mArgc = -1;
  char** mArgv = nullptr;

  std::optional<std::filesystem::path> mConfigDirectory;
  bool mShowVersionInfo = false;

  static int RunWithoutError(std::function<int()> implementor) noexcept;
  static bool ReportTermination(std::exception_ptr exception) noexcept;

#ifdef _WIN32
  static int InvokeWithArgcArgv(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd, std::function<int(int, char**)> invoke);
#endif

  template <typename TDerived>
  static int RunApplicationInstance(int argc, char* argv[]) {
    static_assert(boost::is_base_of<Application, TDerived>::value, "Call this function only with classes that inherit pep::Application");

    TDerived instance;
    return instance.run(argc, argv);
  }

  int run(int argc, char* argv[]);

  std::filesystem::path rawConfigDirectory() const;
  std::optional<std::filesystem::path> rawConfigFile() const;
  std::filesystem::path getMainConfigPath();
  int printVersionInfo(const commandline::LexedValues& lexed);

 protected:
  Application();

  std::string getName() const override;

  int getArgc() const;
  char** getArgv() const;

  virtual bool useUnwinder() const;
  virtual std::optional<severity_level> syslogLogMinimumSeverityLevel() const;
  virtual std::optional<severity_level> consoleLogMinimumSeverityLevel() const;
  virtual std::optional<severity_level> fileLogMinimumSeverityLevel() const;
  commandline::Parameters getSupportedParameters() const override;
  std::optional<int> processLexedParameters(const commandline::LexedValues& lexed) override;
  void finalizeParameters() override;

  static commandline::Parameter MakeConfigDirectoryParameter(const std::filesystem::path& defaultValue, bool positional = false, const std::optional<std::string>& alias = std::nullopt);
  static commandline::Parameters MakeConfigFileParameters(const std::filesystem::path& defaultDir, const std::optional<std::filesystem::path>& defaultFile = std::nullopt, bool positional = false, const std::optional<std::string>& alias = std::nullopt, const std::optional<std::string>& dirAlias = std::nullopt);

  std::filesystem::path getConfigDirectory();
  Configuration loadMainConfigFile();

 public:
  ~Application() override;

  template <class TDerived>
  static int Run(int argc, char* argv[]) noexcept {
    return RunWithoutError([argc, argv]() {
      return RunApplicationInstance<TDerived>(argc, argv);
    });
  }

#ifdef _WIN32
  template <class TDerived>
  static int Run(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) noexcept {
    return RunWithoutError([hInstance, hPrevInstance, lpCmdLine, nShowCmd]() {
      return InvokeWithArgcArgv(hInstance, hPrevInstance, lpCmdLine, nShowCmd, &RunApplicationInstance<TDerived>);
    });
  }
#endif

  class UserNotificationChannel {
  public:
    virtual ~UserNotificationChannel() noexcept = default;
    virtual std::ostream& stream() = 0;
  };
  static std::unique_ptr<UserNotificationChannel> CreateNotificationChannel(bool error);
};
}
