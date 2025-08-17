#include "tun.h"
#include <fmt/core.h>

// Platform-specific headers for the concrete implementations
#if defined(__linux__) || defined(__ANDROID__)
#include "posix.h"
#elif defined(_WIN32)
#include "win32.h"
#endif

namespace stack::device {

    /**
     * @brief Parses a URI to create a TUN device.
     */
    std::unique_ptr<Tun> create_tun(asio::io_context& io_context, const std::string& uri) {
        // Handle tun://fd/N URI for mobile platforms
        if (uri.rfind("tun://fd/", 0) == 0) {
            try {
#if defined(__linux__) || defined(__ANDROID__)
                const int fd = std::stoi(uri.substr(9));
                return PosixTun::from_fd(io_context, fd);
#else
                fmt::println("TUN device creation from FD not supported on this platform.");
                return nullptr;
#endif
            } catch (const std::exception& e) {
                fmt::println("Invalid file descriptor URI '{}': {}", uri, e.what());
                return nullptr;
            }
        }

        // Handle regular device name
#if defined(__linux__) || defined(__ANDROID__)
        return PosixTun::create(io_context, uri);
#elif defined(_WIN32)
        return Win32Tun::create(io_context, uri);
#else
        fmt::println("TUN device creation not supported on this platform.");
        return nullptr;
#endif
    }

} // namespace stack::device