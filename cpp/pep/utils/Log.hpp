#pragma once

#include <pep/utils/Paths.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>

#include <ostream>

namespace pep {

/*!
 * \brief Severity levels used for logging.
 */
enum severity_level {
  verbose,
  debug,
  info,
  warning,
  error,
  critical
};

// The LOG macro can be used to log messages. The messages will be logged to enabled sinks (file/console/syslog).
#define LOG(channel, severity) \
    if((severity) < pep::Logging::compiledMinimumSeverityLevel) {} \
        else BOOST_LOG_CHANNEL_SEV(pep::Logging::GetLogger(), channel, severity)
// TODO ^ find a clean alternative for this hideous if statement.

class Logging {
public:
  // Produces a global logger.
  using pep_severity_channel_logger = boost::log::sources::severity_channel_logger_mt<severity_level, std::string>;
  static pep_severity_channel_logger& GetLogger();

  // Minimum severity level of logging statements that have been compiled in.
  static const severity_level compiledMinimumSeverityLevel;

  // Initializes logging with the specified output settings
  static void Initialize(const std::vector<std::shared_ptr<Logging>>& settings);

  // Converts a string representation to a severity_level value
  static severity_level ParseSeverityLevel(const std::string& level);
  // Converts a severity_level value to a string representation
  static std::string FormatSeverityLevel(severity_level level);
  // Gets string representations of supported severity_level values
  static std::vector<std::string> SeverityLevelNames();

  /*!
   * \brief Returns a logging-safe representation of in
   */
  static std::string Escape(const std::string& in);

  // Stuff below is for implementing classes (i.e. specific logging output channels)
  class Sink {
  public:
    virtual ~Sink() noexcept = default;
    virtual void setMinimumSeverityLevel(severity_level minimum) = 0;
  };

  virtual ~Logging() noexcept = default;

private:
  severity_level minimum_level;

  void apply() const;

protected:
  explicit Logging(severity_level minimum_level)
    : minimum_level(minimum_level) {
  }

  virtual std::shared_ptr<Sink> registerSink() const = 0;
};

// Used by boost for printing severities
inline std::ostream& operator<<(std::ostream& os, severity_level level) {
  return os << Logging::FormatSeverityLevel(level);
}

class ConsoleLogging : public Logging {
protected:
  std::shared_ptr<Sink> registerSink() const override;

public:
  explicit ConsoleLogging(severity_level minimum_level)
    : Logging(minimum_level) {
  }
};

class FileLogging : public Logging {
private:
  std::string prefix;

protected:
  std::shared_ptr<Sink> registerSink() const override;

public:
  FileLogging(severity_level minimum_level)
    : Logging(minimum_level), prefix(GetOutputBasePath().string()) {
  }
};

class SysLogging: public Logging{
private:
  std::string host_name;
  uint16_t port;

protected:
  std::shared_ptr<Sink> registerSink() const override;

public:
  SysLogging(severity_level minimum_level, const std::string& szHostname = "", uint16_t wPort = 514)
    : Logging(minimum_level), host_name(szHostname), port(wPort) {
  }
};

}
