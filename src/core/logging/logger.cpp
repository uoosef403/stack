#include "logger.hpp"
#include "core/config/config.hpp"
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>


namespace stack::log {

void Logger::init(const LoggingConfig &config) {
  // Set a clean, professional pattern for all loggers
  spdlog::set_pattern("%^[%Y-%m-%d %H:%M:%S.%e] [%t] [%l] %v%$");

  // Create sinks based on configuration
  std::vector<spdlog::sink_ptr> sinks;

  // Add console sink if enabled
  if (config.console_output) {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    sinks.push_back(console_sink);
  }

  // Add file sink if log file is specified
  if (!config.log_file.empty()) {
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        config.log_file, true);
    sinks.push_back(file_sink);
  }

  // If no sinks are configured, fall back to console
  if (sinks.empty()) {
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    sinks.push_back(console_sink);
  }

  // Create the logger
  s_logger_ =
      std::make_shared<spdlog::logger>("stack", begin(sinks), end(sinks));

  // Set the log level based on configuration
  s_logger_->set_level(parse_log_level(config.log_level));
  s_logger_->flush_on(spdlog::level::err);

  // Register as default logger
  spdlog::set_default_logger(s_logger_);
}

spdlog::level::level_enum Logger::parse_log_level(const std::string &level) {
  if (level == "trace") {
    return spdlog::level::trace;
  }
  if (level == "debug") {
    return spdlog::level::debug;
  }
  if (level == "info") {
    return spdlog::level::info;
  }
  if (level == "warn" || level == "warning") {
    return spdlog::level::warn;
  }
  if (level == "error") {
    return spdlog::level::err;
  }
  if (level == "critical") {
    return spdlog::level::critical;
  }
  if (level == "off") {
    return spdlog::level::off;
  }

  // Default to info if unknown level
  return spdlog::level::info;
}

} // namespace stack::log
