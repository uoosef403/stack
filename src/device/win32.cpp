// This define prevents the Windows header from creating min/max macros
// that conflict with the C++ standard library.
#define NOMINMAX

#include "win32.h"
#include <fmt/core.h>

#if defined(_WIN32)

#include <boost/asio.hpp> // Required for asio::post
#include <objbase.h>    // For CoCreateGuid
#include <algorithm>    // For std::min

namespace asio = boost::asio;

namespace stack::device {
    // --- Constructor / Destructor ---
    Win32Tun::Win32Tun(asio::io_context &io_context) : m_read_handle(io_context) {
    }

    Win32Tun::~Win32Tun() {
        if (m_session && m_WintunEndSession) m_WintunEndSession(m_session);
        if (m_adapter && m_WintunCloseAdapter) m_WintunCloseAdapter(m_adapter);
        if (m_wintun_dll) FreeLibrary(m_wintun_dll);
    }

    // --- Factory Method ---
    std::unique_ptr<Tun> Win32Tun::create(asio::io_context &io_context, const std::string &name) {
        auto tun = std::unique_ptr<Win32Tun>(new Win32Tun(io_context));
        if (!tun->open(name)) {
            return nullptr;
        }
        return tun;
    }

    // --- Private Initialization ---
    bool Win32Tun::initialize_wintun() {
        m_wintun_dll = LoadLibraryExW(L"wintun.dll", nullptr, LOAD_LIBRARY_SEARCH_APPLICATION_DIR);
        if (!m_wintun_dll) {
            fmt::println("Failed to load wintun.dll. Error: {}", GetLastError());
            return false;
        }

        m_WintunCreateAdapter = reinterpret_cast<WINTUN_CREATE_ADAPTER_FUNC>(GetProcAddress(
            m_wintun_dll, "WintunCreateAdapter"));
        m_WintunCloseAdapter = reinterpret_cast<WINTUN_CLOSE_ADAPTER_FUNC>(GetProcAddress(
            m_wintun_dll, "WintunCloseAdapter"));
        m_WintunStartSession = reinterpret_cast<WINTUN_START_SESSION_FUNC>(GetProcAddress(
            m_wintun_dll, "WintunStartSession"));
        m_WintunEndSession = reinterpret_cast<WINTUN_END_SESSION_FUNC>(GetProcAddress(
            m_wintun_dll, "WintunEndSession"));
        m_WintunGetReadWaitEvent = reinterpret_cast<WINTUN_GET_READ_WAIT_EVENT_FUNC>(GetProcAddress(
            m_wintun_dll, "WintunGetReadWaitEvent"));
        m_WintunReceivePacket = reinterpret_cast<WINTUN_RECEIVE_PACKET_FUNC>(GetProcAddress(
            m_wintun_dll, "WintunReceivePacket"));
        m_WintunReleaseReceivePacket = reinterpret_cast<WINTUN_RELEASE_RECEIVE_PACKET_FUNC>(GetProcAddress(
            m_wintun_dll, "WintunReleaseReceivePacket"));
        m_WintunAllocateSendPacket = reinterpret_cast<WINTUN_ALLOCATE_SEND_PACKET_FUNC>(GetProcAddress(
            m_wintun_dll, "WintunAllocateSendPacket"));
        m_WintunSendPacket = reinterpret_cast<WINTUN_SEND_PACKET_FUNC>(GetProcAddress(
            m_wintun_dll, "WintunSendPacket"));

        if (!m_WintunCreateAdapter || !m_WintunStartSession || !m_WintunReceivePacket || !m_WintunSendPacket) {
            fmt::println("Failed to get one or more critical Wintun functions from wintun.dll");
            return false;
        }
        return true;
    }

    bool Win32Tun::open(const std::string &name) {
        if (!initialize_wintun()) return false;

        GUID guid;
        if (FAILED(CoCreateGuid(&guid))) {
            fmt::println("Failed to create GUID for Wintun adapter.");
            return false;
        }

        std::wstring wName(name.begin(), name.end());
        m_adapter = m_WintunCreateAdapter(wName.c_str(), L"stack", &guid);
        if (!m_adapter) {
            fmt::println("Failed to create Wintun adapter. Error: {}", GetLastError());
            return false;
        }

        m_session = m_WintunStartSession(m_adapter, 0x400000);
        if (!m_session) {
            fmt::println("Failed to start Wintun session. Error: {}", GetLastError());
            m_WintunCloseAdapter(m_adapter);
            return false;
        }

        HANDLE read_handle = m_WintunGetReadWaitEvent(m_session);
        if (read_handle == INVALID_HANDLE_VALUE) {
            fmt::println("Failed to get Wintun read handle.");
            return false;
        }
        m_read_handle.assign(read_handle);

        fmt::println("Successfully created TUN device '{}'", name);
        return true;
    }

    asio::awaitable<void> Win32Tun::run_reader(IPPacketChannel& incoming_channel) {
        try {
            while (true) {
                std::vector<uint8_t> buffer(2048);
                if (
                    auto [ec] = co_await m_read_handle.async_wait(
                        asio::as_tuple(asio::use_awaitable)
                    );
                    ec
                ) {
                    // Handle wait errors (e.g., handle was closed)
                    throw boost::system::system_error(ec);
                }

                DWORD packet_size_dw;
                const BYTE *packet = m_WintunReceivePacket(m_session, &packet_size_dw);
                if (!packet) {
                    co_return;
                }

                size_t bytes_to_copy = std::min(buffer.size(), static_cast<size_t>(packet_size_dw));
                buffer.resize(bytes_to_copy);
                std::memcpy(buffer.data(), packet, bytes_to_copy);
                m_WintunReleaseReceivePacket(m_session, packet);
                co_await incoming_channel.async_send({}, std::move(buffer));
            }
        } catch (const std::exception &e) {
            if (errno != EBADF) {
                fmt::print(stderr, "TUN reader stopped: {}\n", e.what());
            }
            incoming_channel.close();
        }
    }
    asio::awaitable<void> Win32Tun::run_writer(IPPacketChannel& outgoing_channel) {
        try {
            while (true) {
                auto [ec, buffer] = co_await outgoing_channel.async_receive(
                    asio::as_tuple(asio::use_awaitable));

                // Check the error code.
                if (ec) {
                    // Channel closed or other error, exit the coroutine.
                    co_return;
                }

                BYTE *send_packet = m_WintunAllocateSendPacket(m_session, static_cast<DWORD>(buffer.size()));
                if (!send_packet) {
                    fmt::println("Failed to allocate send packet from Wintun ring.");
                    throw boost::system::system_error(asio::error::no_buffer_space);
                }

                std::memcpy(send_packet, buffer.data(), buffer.size());
                m_WintunSendPacket(m_session, send_packet);
            }
        } catch (const std::exception &e) {
            if (errno != EBADF) {
                fmt::print(stderr, "TUN writer stopped: {}\n", e.what());
            }
        }
    }
} // namespace stack::device
#endif // _WIN32
