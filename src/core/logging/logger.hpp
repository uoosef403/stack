#pragma once

#include <memory>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>


struct LoggingConfig; // Forward declaration

namespace stack::log {

class Logger {
public:
  static void init(const LoggingConfig &config);

  static std::shared_ptr<spdlog::logger> &get() { return s_logger_; }

private:
  static spdlog::level::level_enum parse_log_level(const std::string &level);
  inline static std::shared_ptr<spdlog::logger> s_logger_;
};

} // namespace stack::log

// Helper macros to make logging easier
#define LOG_TRACE(...) ::stack::log::Logger::get()->trace(__VA_ARGS__)
#define LOG_DEBUG(...) ::stack::log::Logger::get()->debug(__VA_ARGS__)
#define LOG_INFO(...) ::stack::log::Logger::get()->info(__VA_ARGS__)
#define LOG_WARN(...) ::stack::log::Logger::get()->warn(__VA_ARGS__)
#define LOG_ERROR(...) ::stack::log::Logger::get()->error(__VA_ARGS__)
