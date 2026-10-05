#pragma once

#include <optional>
#include <span>
#include <vector>

#include <boost/type_traits/is_base_of.hpp>

#include <pep/application/CommandLineCommand.hpp>
#include <pep/utils/Configuration_fwd.hpp>
#include <pep/utils/Log.hpp>

#define PEP_DEFINE_C_MAIN_FUNCTION(applicationType) \
  int main(int argc, char* argv[]) { \
    return pep::Application::Run< applicationType >(std::span<const char* const>(argv, static_cast<std::size_t>(argc))); \
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
  std::vector<std::string> args_;

  std::optional<std::filesystem::path> configDirectory_;
  bool showVersionInfo_ = false;

  static std::vector<std::string> ConvertArguments(std::span<const char* const> args);

  static int RunWithoutError(std::function<int()> implementor) noexcept;
  static bool ReportTermination(std::exception_ptr exception) noexcept;

#ifdef _WIN32
  static int InvokeWithArgs(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd, std::function<int(std::vector<std::string> args)> invoke);
#endif

  template <typename TDerived>
  static int RunApplicationInstance(std::vector<std::string> args) {
    static_assert(boost::is_base_of<Application, TDerived>::value, "Call this function only with classes that inherit pep::Application");

    TDerived instance;
    return instance.run(std::move(args));
  }

  int run(std::vector<std::string> args);

  bool initializeLoggingOnceFlag_ = false;  ///< Tracks if initializeLoggingOnce was called
  void initializeLoggingOnce();

  std::filesystem::path rawConfigDirectory() const;
  std::optional<std::filesystem::path> rawConfigFile() const;
  std::filesystem::path getMainConfigPath();
  int printVersionInfo(const commandline::LexedValues& lexed);

 protected:
  Application();

  std::string getName() const override;

  const std::vector<std::string>& getArgs() const { return args_; }

  virtual bool useUnwinder() const;
  virtual std::optional<Severity> syslogLogMinimumSeverityLevel() const;
  virtual std::optional<Severity> consoleLogMinimumSeverityLevel() const;
  virtual std::optional<Severity> fileLogMinimumSeverityLevel() const;
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
  static int Run(std::span<const char* const> args) noexcept {
    return RunWithoutError([args] {
      return RunApplicationInstance<TDerived>(ConvertArguments(args));
    });
  }

#ifdef _WIN32
  template <class TDerived>
  static int Run(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nShowCmd) noexcept {
    return RunWithoutError([hInstance, hPrevInstance, lpCmdLine, nShowCmd]() {
      return InvokeWithArgs(hInstance, hPrevInstance, lpCmdLine, nShowCmd, &RunApplicationInstance<TDerived>);
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
