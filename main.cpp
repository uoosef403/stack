#include "core/config/config.hpp"
#include "core/logging/logger.hpp"
#include "core/network_stack.hpp"

int main(int argc, char *argv[]) {
#ifdef DEBUG
  setvbuf(stdout, nullptr, _IONBF, 0);
#endif

  try {
    // Initialize configuration
    Config &config = Config::get_instance();

    // Parse command line arguments
    if (!config.parse_command_line(argc, argv)) {
      return 1; // Help was shown or error occurred
    }

    // Initialize logging with configured settings
    stack::log::Logger::init(config.get_logging_config());

    // Create and run the network stack
    stack::core::NetworkStack network_stack(config);
    network_stack.initialize();
    network_stack.run();

    return 0;
  } catch (const std::exception &e) {
    LOG_ERROR("Exception in main: {}", e.what());
    return 1;
  }
}
