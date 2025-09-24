#include <pep/utils/LocalSettings.hpp>
#include <pep/utils/Log.hpp>
#include <pep/utils/ThreadUtil.hpp>

#include <ios>
#include <sstream>
#include <cctype>
#include <cstdint>
#include <unordered_map>

#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/sinks/event_log_backend.hpp>
#include <boost/log/sinks/syslog_backend.hpp>
#include <boost/format.hpp>
#include <boost/uuid/uuid.hpp>
#include <boost/uuid/uuid_io.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/log/sources/global_logger_storage.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/attributes.hpp>
#include <boost/log/expressions.hpp>

#if !defined(_WIN32) && !defined(BOOST_LOG_USE_NATIVE_SYSLOG)
// we have this check to make sure that logging is properly configured and we don't lose logging in production
#error "Non-Windows build need native boost logging functionality"
#endif

namespace pep {

const severity_level Logging::compiledMinimumSeverityLevel = PEP_COMPILED_MIN_LOG_SEVERITY;

// Initialise a global logger called 'logger'
BOOST_LOG_INLINE_GLOBAL_LOGGER_DEFAULT(logger, Logging::pep_severity_channel_logger)

BOOST_LOG_ATTRIBUTE_KEYWORD(severity, "Severity", severity_level)
BOOST_LOG_ATTRIBUTE_KEYWORD(channel, "Channel", std::string)
BOOST_LOG_ATTRIBUTE_KEYWORD(threadName, "ThreadName", std::string)

namespace {

std::string getInstallationUuid() {
  std::string szUuid;
  auto& localSettings = LocalSettings::getInstance();
  if (localSettings->retrieveValue(&szUuid, "Global", "Uuid") != false) {
    return szUuid;
  }

  szUuid = boost::uuids::to_string(boost::uuids::random_generator()());
  localSettings->storeValue("Global", "Uuid", szUuid);
  localSettings->flushChanges();
  return szUuid;
}

template <typename TBoost>
class SinkWrapper : public Logging::Sink {
private:
  boost::shared_ptr<TBoost> mImplementor;

public:
  explicit SinkWrapper(boost::shared_ptr<TBoost> implementor)
    : mImplementor(implementor) {
  }

  void setMinimumSeverityLevel(severity_level minimum) override {
    mImplementor->set_filter(severity >= minimum);
  }
};

template <typename TBoost>
std::shared_ptr<SinkWrapper<TBoost>> CreateSinkWrapper(boost::shared_ptr<TBoost> implementor) {
  return std::make_shared<SinkWrapper<TBoost>>(implementor);
}

using SeverityLevelNames = std::map<severity_level, std::string>;
SeverityLevelNames GetSeverityLevelNames() {
  return SeverityLevelNames({
    { severity_level::verbose, "verbose"  },
    { severity_level::debug, "debug" },
    { severity_level::info, "info" },
    { severity_level::warning, "warning" },
    { severity_level::error, "error" },
    { severity_level::critical, "critical" }
    });
}

std::string FormatThreadName() {
  if (auto name = ThreadName::Get()) {
    return *name + ":";
  }
  return "";
}

} // end anonymous namespace

severity_level Logging::ParseSeverityLevel(const std::string& level) {
  auto names = GetSeverityLevelNames();
  auto end = names.cend();
  auto position = std::find_if(names.cbegin(), end, [&level](const std::pair<const severity_level, std::string>& candidate) {return candidate.second == level; });
  if (position == end) {
    throw std::runtime_error("Invalid severity level " + level);
  }
  return position->first;
}

std::string Logging::FormatSeverityLevel(severity_level level) {
  auto names = GetSeverityLevelNames();
  auto position = names.find(level);
  if (position == names.cend()) {
    throw std::runtime_error("Invalid severity level " + std::to_string(level));
  }
  return position->second;
}

std::vector<std::string> Logging::SeverityLevelNames() {
  auto pairs = GetSeverityLevelNames();
  std::vector<std::string> result;
  result.reserve(pairs.size());
  std::transform(pairs.cbegin(), pairs.cend(), std::back_inserter(result), [](const std::pair<const severity_level, std::string>& pair) {return pair.second; });
  return result;
}

Logging::pep_severity_channel_logger& Logging::GetLogger() {
  return logger::get();
}

void Logging::apply() const {
  this->registerSink()->setMinimumSeverityLevel(minimum_level);
}

void Logging::Initialize(const std::vector<std::shared_ptr<Logging>>& settings) {
  boost::log::core::get()->add_global_attribute("TimeStamp", boost::log::attributes::local_clock());
  boost::log::core::get()->add_global_attribute("ThreadName",
    boost::log::attributes::make_function<std::string>(&FormatThreadName));

  auto initialized = false;
  for (const auto& single : settings) {
    single->apply();
    initialized = true;
  }

  if (!initialized) {
    // Prevent Boost's default log sink from sending everything to the console
    auto highest = GetSeverityLevelNames().rbegin()->first; // Get highest severity level
    auto nonexistent = static_cast<severity_level>(highest + 1); // Get severity level that will never be reached because it doesn't exist
    ConsoleLogging console(nonexistent); // Configure console logging that suppresses all existing severity levels
    Logging& upcast = console;
    upcast.apply();
  }
  // Initialize global logger, prevents some thread sanitizer warnings in boost::log::aux::once_block_sentry
  (void) GetLogger();
}

std::shared_ptr<Logging::Sink> ConsoleLogging::registerSink() const {
  return CreateSinkWrapper(
    boost::log::add_console_log(
                std::clog,
                boost::log::keywords::format = (boost::log::expressions::stream << boost::log::expressions::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d %H:%M:%S") << ": <" << severity << "> [" << threadName << channel << "] " << boost::log::expressions::smessage),
                boost::log::keywords::auto_flush = true
              ));
}


std::shared_ptr<Logging::Sink> FileLogging::registerSink() const {
  return CreateSinkWrapper(
    boost::log::add_file_log(
                prefix + "_%N.log",
                boost::log::keywords::format = (boost::log::expressions::stream << boost::log::expressions::format_date_time< boost::posix_time::ptime >("TimeStamp", "%Y-%m-%d %H:%M:%S") << ": <" << severity << "> [" << threadName << channel << "] " << boost::log::expressions::smessage),
                boost::log::keywords::open_mode = std::ios_base::app,
                boost::log::keywords::rotation_size = 1024 * 1024,
                boost::log::keywords::auto_flush = true,
                boost::log::keywords::target = "rotated_logs"
              ));
}

namespace {
#if !defined(_WIN32) || defined(__MINGW32__)

using SystemLogBackend = boost::log::sinks::syslog_backend;

boost::shared_ptr<SystemLogBackend> createSyslogBackend(const std::string& szHostname, std::uint16_t wPort) {
  boost::shared_ptr<SystemLogBackend> lpBackend;

  bool bLocal = szHostname.empty();

  if (bLocal) {
    lpBackend = boost::make_shared<SystemLogBackend>(
                  boost::log::keywords::facility = boost::log::sinks::syslog::user,
#ifdef BOOST_LOG_USE_NATIVE_SYSLOG
                  boost::log::keywords::use_impl = boost::log::sinks::syslog::native
#else /* BOOST_LOG_USE_NATIVE_SYSLOG */
                  boost::log::keywords::use_impl = boost::log::sinks::syslog::udp_socket_based
#endif
                );
  } else {
    lpBackend = boost::make_shared<SystemLogBackend>(
                  boost::log::keywords::facility = boost::log::sinks::syslog::local0,
                  boost::log::keywords::use_impl = boost::log::sinks::syslog::udp_socket_based
                );

    lpBackend->set_target_address(szHostname, wPort);
  }

  boost::log::sinks::syslog::custom_severity_mapping<severity_level> mSeverityMapper("PepSeverityMapper");
  mSeverityMapper[severity_level::debug] = boost::log::sinks::syslog::debug;
  mSeverityMapper[severity_level::info] = boost::log::sinks::syslog::info;
  mSeverityMapper[severity_level::warning] = boost::log::sinks::syslog::warning;
  mSeverityMapper[severity_level::error] = boost::log::sinks::syslog::error;
  mSeverityMapper[severity_level::critical] = boost::log::sinks::syslog::critical;
  lpBackend->set_severity_mapper(mSeverityMapper);

  return lpBackend;
}

#else // _WIN32 && !__MINGW32__

using SystemLogBackend = boost::log::sinks::simple_event_log_backend;

boost::shared_ptr<SystemLogBackend> createSyslogBackend(const std::string& szHostname, uint16_t wPort) {

  auto lpBackend = boost::make_shared<SystemLogBackend>(
                     boost::log::keywords::target = szHostname,
                     boost::log::keywords::log_source = "pep"
                   );

  boost::log::sinks::event_log::custom_event_type_mapping< severity_level > mSeverityMapper("PepSeverityMapper");
  mSeverityMapper[severity_level::debug] = boost::log::sinks::event_log::info;
  mSeverityMapper[severity_level::info] = boost::log::sinks::event_log::info;
  mSeverityMapper[severity_level::warning] = boost::log::sinks::event_log::warning;
  mSeverityMapper[severity_level::error] = boost::log::sinks::event_log::error;
  mSeverityMapper[severity_level::critical] = boost::log::sinks::event_log::error;
  lpBackend->set_event_type_mapper(mSeverityMapper);

  return lpBackend;
}

#endif // _WIN32 && !__MINGW32__
} // End unnamed namespace

std::shared_ptr<Logging::Sink> SysLogging::registerSink() const {
  try {
    using sink_t = boost::log::sinks::synchronous_sink<SystemLogBackend>;
    auto lpBackend = createSyslogBackend(host_name, port);

    boost::shared_ptr<sink_t> lpSink = boost::make_shared<sink_t>(lpBackend);
    lpSink->set_formatter(
      boost::log::expressions::format("[%1%, %2%%3%]: %4%")
      % getInstallationUuid()
      % boost::log::expressions::attr<std::string>("ThreadName")
      % boost::log::expressions::attr<std::string>("Channel")
      % boost::log::expressions::smessage
    );

    boost::log::core::get()->add_sink(lpSink);
    return CreateSinkWrapper(lpSink);
  } catch (const std::exception& e) {
    // Notify our failure to other sinks that may already have been successfully set up
    LOG("Logging", critical) << "Failed to initialize system log: " << e.what() << std::endl;
    throw;
  }
}

std::string Logging::Escape(const std::string& in) {
  std::ostringstream ss;
  ss << "\"";
  for (char c : in) {
    if (c == '\\') {
      ss << "\\\\";
    } else if (c == '"') {
      ss << "\"";
    } else if (std::isprint(c)) {
      ss << c;
    } else {
      ss << "\\x"
         << std::hex
         << (static_cast<int>(c)) / 16
         << (static_cast<int>(c)) % 16;
    }
  }
  ss << "\"";
  return ss.str();
}

}
