#pragma once

#include <memory>
#include <spdlog/spdlog.h>


namespace stack::log {

class Logger {
public:
  static void init() {
    // Set a clean, professional pattern for all loggers
    spdlog::set_pattern("%^[%H:%M:%S] [%l] %v%$");

    // Get the default console logger
    s_logger_ = spdlog::default_logger();

    // Set the default log level. Change this to spdlog::level::info for
    // production.
    s_logger_->set_level(spdlog::level::info);
  }

  static std::shared_ptr<spdlog::logger> &get() { return s_logger_; }

private:
  inline static std::shared_ptr<spdlog::logger> s_logger_;
};

} // namespace stack::log

// Helper macros to make logging easier
#define LOG_TRACE(...) ::stack::log::Logger::get()->trace(__VA_ARGS__)
#define LOG_DEBUG(...) ::stack::log::Logger::get()->debug(__VA_ARGS__)
#define LOG_INFO(...) ::stack::log::Logger::get()->info(__VA_ARGS__)
#define LOG_WARN(...) ::stack::log::Logger::get()->warn(__VA_ARGS__)
#define LOG_ERROR(...) ::stack::log::Logger::get()->error(__VA_ARGS__)
