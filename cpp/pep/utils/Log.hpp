#pragma once

#include <pep/utils/Paths.hpp>
#include <boost/log/sources/record_ostream.hpp>
#include <boost/log/sources/severity_channel_logger.hpp>

#include <ostream>

namespace pep {

/*!
 * \brief Severity levels used for logging.
 */
enum class Severity {
  Verbose,
  Debug,
  Info,
  Warning,
  Error,
  Critical
};

// The PEP_LOG macro can be used to log messages. The messages will be logged to enabled sinks (file/console/syslog).
#define PEP_LOG(channel, severity) \
    if((severity) < pep::Logging::compiledMinimumSeverity) {} \
        else BOOST_LOG_CHANNEL_SEV(pep::Logging::GetLogger(), channel, severity)
// TODO ^ find a clean alternative for this hideous if statement.

class Logging {
public:
  // Produces a global logger.
  using pep_severity_channel_logger = boost::log::sources::severity_channel_logger_mt<Severity, std::string>;
  static pep_severity_channel_logger& GetLogger();

  // Minimum severity level of logging statements that have been compiled in.
  static const Severity compiledMinimumSeverity;

  // Initializes logging with the specified output settings
  static void Initialize(const std::vector<std::shared_ptr<Logging>>& settings);

  // Converts a string representation to a Severity value
  static Severity ParseSeverity(const std::string& level);
  // Converts a Severity value to a string representation
  static std::string FormatSeverity(Severity level);
  // Gets string representations of supported Severity values
  static std::vector<std::string> SeverityNames();

  /*!
   * \brief Returns a logging-safe representation of in
   */
  static std::string Escape(const std::string& in);

  // Stuff below is for implementing classes (i.e. specific logging output channels)
  class Sink {
  public:
    virtual ~Sink() noexcept = default;
    virtual void setMinimumSeverity(Severity minimum) = 0;
  };

  virtual ~Logging() noexcept = default;

private:
  Severity minimum;

  void apply() const;

protected:
  explicit Logging(Severity minimum)
    : minimum(minimum) {
  }

  virtual std::shared_ptr<Sink> registerSink() const = 0;
};

// Used by boost for printing severities
inline std::ostream& operator<<(std::ostream& os, Severity level) {
  return os << Logging::FormatSeverity(level);
}

class ConsoleLogging : public Logging {
protected:
  std::shared_ptr<Sink> registerSink() const override;

public:
  explicit ConsoleLogging(Severity minimum)
    : Logging(minimum) {
  }
};

class FileLogging : public Logging {
private:
  std::string prefix;

protected:
  std::shared_ptr<Sink> registerSink() const override;

public:
  FileLogging(Severity minimum)
    : Logging(minimum), prefix(GetOutputBasePath().string()) {
  }
};

class SysLogging: public Logging{
private:
  std::string host_name;
  uint16_t port;

protected:
  std::shared_ptr<Sink> registerSink() const override;

public:
  SysLogging(Severity minimum, const std::string& szHostname = "", uint16_t wPort = 514)
    : Logging(minimum), host_name(szHostname), port(wPort) {
  }
};

}
