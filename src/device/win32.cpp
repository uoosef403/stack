// This define prevents the Windows header from creating min/max macros
// that conflict with the C++ standard library.
#define NOMINMAX

#include "win32.h"
#include <fmt/core.h>

#if defined(_WIN32)

#include <boost/asio.hpp> // Required for asio::post
#include <objbase.h>    // For CoCreateGuid
#include <algorithm>    // For std::min
#include <iphlpapi.h>

namespace asio = boost::asio;

namespace stack::device {
    // --- Constructor / Destructor ---
    Win32Tun::Win32Tun(
        asio::io_context &io_context,
        const std::string &tun_ip4,
        const std::string &tun_ip4_mask,
        const std::string &tun_ip6,
        const uint8_t tun_ip6_prefix_mask
    ) : m_read_handle(io_context), m_tun_ip4(tun_ip4),
    m_tun_ip4_mask(tun_ip4_mask), m_tun_ip6(tun_ip6),
    m_tun_ip6_prefix_mask(tun_ip6_prefix_mask) {
    }

    Win32Tun::~Win32Tun() {
        if (m_session && m_WintunEndSession) {
            m_WintunEndSession(m_session);
            m_session = nullptr;
        }

        if (m_adapter) {
            if (m_WintunDeleteAdapter) {
                // Remove the adapter from the system entirely
                m_WintunDeleteAdapter(m_adapter, 0, nullptr);
            } else if (m_WintunCloseAdapter) {
                // Fallback: just close handle, leaves adapter orphaned
                m_WintunCloseAdapter(m_adapter);
            }
            m_adapter = nullptr;
        }

        if (m_wintun_dll) {
            FreeLibrary(m_wintun_dll);
            m_wintun_dll = nullptr;
        }
    }

    // --- Factory Method ---
    std::unique_ptr<Tun> Win32Tun::create(
        asio::io_context &io_context,
        const std::string &name,
        const std::string &tun_ip4,
        const std::string &tun_ip4_mask,
        const std::string &tun_ip6,
        const uint8_t tun_ip6_prefix_mask
    ) {
        auto tun = std::unique_ptr<Win32Tun>(
            new Win32Tun(
                io_context,
                tun_ip4,
                tun_ip4_mask,
                tun_ip6,
                tun_ip6_prefix_mask
            )
        );
        if (!tun->open(name)) {
            return nullptr;
        }
        return tun;
    }

    namespace {
        // A helper to get a descriptive error message from a Windows error code.
        std::string get_error_string(DWORD error_code) {
            char *buffer = nullptr;
            size_t size = FormatMessageA(
                FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                nullptr, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                reinterpret_cast<LPSTR>(&buffer), 0, nullptr);

            if (size == 0) {
                return fmt::format("Unknown error code {}", error_code);
            }

            std::string message(buffer, size);
            LocalFree(buffer);
            // Remove trailing newline characters
            while (!message.empty() && (message.back() == '\r' || message.back() == '\n')) {
                message.pop_back();
            }
            return message;
        }

        // Converts an IPv4 netmask string to a CIDR prefix length.
        UCHAR netmask_to_prefix_length(const std::string &netmask) {
            IN_ADDR addr;
            if (InetPtonA(AF_INET, netmask.c_str(), &addr) != 1) {
                return 0; // Invalid format
            }
            DWORD mask = ntohl(addr.S_un.S_addr);
            UCHAR prefix = 0;
            // Count the leading '1' bits in the mask
            while (prefix < 32 && ((mask << prefix) & 0x80000000)) {
                prefix++;
            }
            return prefix;
        }
    } // namespace

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
        m_WintunDeleteAdapter = reinterpret_cast<WINTUN_DELETE_ADAPTER_FUNC>(GetProcAddress(
            m_wintun_dll, "WintunDeleteAdapter"));

        if (!m_WintunCreateAdapter || !m_WintunStartSession || !m_WintunReceivePacket || !m_WintunSendPacket) {
            fmt::println("Failed to get one or more critical Wintun functions from wintun.dll");
            return false;
        }
        return true;
    }

    bool Win32Tun::open(const std::string &name) {
        if (!initialize_wintun()) return false;

        if (FAILED(CoCreateGuid(&m_guid))) {
            fmt::println(stderr, "Failed to create GUID for Wintun adapter.");
            return false;
        }

        std::wstring wName(name.begin(), name.end());
        // Pass the address of the member GUID to WintunCreateAdapter.
        m_adapter = m_WintunCreateAdapter(wName.c_str(), L"stack", &m_guid);
        if (!m_adapter) {
            fmt::println(stderr, "Failed to create Wintun adapter. Error: {}", GetLastError());
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

        if (!configure(m_tun_ip4, m_tun_ip4_mask,
                       m_tun_ip6, m_tun_ip6_prefix_mask)) {
            fmt::println(stderr, "Failed to assign IPs to Wintun adapter, do this yourself");
        }

        m_read_handle.assign(read_handle);

        fmt::println("Successfully created TUN device '{}'", name);
        return true;
    }

    asio::awaitable<void> Win32Tun::run_reader(IPPacketChannel &incoming_channel) {
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

    asio::awaitable<void> Win32Tun::run_writer(IPPacketChannel &outgoing_channel) {
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

    /**
     * @brief Configures the Wintun adapter's IPv4 and IPv6 addresses.
     *
     * This function:
     *  - Converts the adapter GUID into a LUID
     *  - Removes existing addresses
     *  - Sets the provided IPv4 and IPv6 addresses
     *
     * @param ipv4_addr IPv4 address (e.g., "192.168.69.69").
     * @param netmask   IPv4 subnet mask (e.g., "255.255.255.0").
     * @param ipv6_addr (Optional) IPv6 address (e.g., "fd00::1").
     * @param ipv6_prefix_len (Optional) IPv6 prefix length (e.g., 64).
     * @return True if at least IPv4 configuration succeeded.
     */
    bool Win32Tun::configure(const std::string &ipv4_addr,
                             const std::string &netmask,
                             const std::string &ipv6_addr = "",
                             uint8_t ipv6_prefix_len = 0) const {
        if (!m_adapter) {
            fmt::print(stderr, "Configuration failed: Wintun adapter is not open.\n");
            return false;
        }

        // Directly get the adapter's LUID from the GUID
        NET_LUID luid;
        if (ConvertInterfaceGuidToLuid(&m_guid, &luid) != NO_ERROR) {
            fmt::print(stderr, "Could not convert GUID to LUID.\n");
            return false;
        }

        // Flush existing IPs
        for (int family: {AF_INET, AF_INET6}) {
            PMIB_UNICASTIPADDRESS_TABLE table = nullptr;
            if (GetUnicastIpAddressTable(static_cast<ADDRESS_FAMILY>(family), &table) == NO_ERROR) {
                for (ULONG i = 0; i < table->NumEntries; ++i) {
                    if (table->Table[i].InterfaceLuid.Value == luid.Value) {
                        DeleteUnicastIpAddressEntry(&table->Table[i]);
                    }
                }
                FreeMibTable(table);
            }
        }

        bool success = true;

        // ---------------- IPv4 ----------------
        if (!ipv4_addr.empty()) {
            MIB_UNICASTIPADDRESS_ROW row4;
            InitializeUnicastIpAddressEntry(&row4);
            row4.InterfaceLuid = luid;
            row4.Address.si_family = AF_INET;

            if (InetPtonA(AF_INET, ipv4_addr.c_str(), &row4.Address.Ipv4.sin_addr) != 1) {
                fmt::print(stderr, "Invalid IPv4 address: {}\n", ipv4_addr);
                success = false;
            } else {
                row4.OnLinkPrefixLength = netmask_to_prefix_length(netmask);
                row4.DadState = IpDadStatePreferred;
                row4.PrefixOrigin = IpPrefixOriginManual;
                row4.SuffixOrigin = IpSuffixOriginManual;
                row4.ValidLifetime = 0xFFFFFFFF;
                row4.PreferredLifetime = 0xFFFFFFFF;

                DWORD result = CreateUnicastIpAddressEntry(&row4);
                if (result != NO_ERROR) {
                    fmt::print(stderr, "Failed to set IPv4 address {}: {}\n",
                               ipv4_addr, get_error_string(result));
                    success = false;
                } else {
                    fmt::println("Configured IPv4: {}/{}", ipv4_addr, netmask);
                }
            }
        }

        // ---------------- IPv6 ----------------
        if (!ipv6_addr.empty() && ipv6_prefix_len > 0) {
            MIB_UNICASTIPADDRESS_ROW row6;
            InitializeUnicastIpAddressEntry(&row6);
            row6.InterfaceLuid = luid;
            row6.Address.si_family = AF_INET6;

            if (InetPtonA(AF_INET6, ipv6_addr.c_str(), &row6.Address.Ipv6.sin6_addr) != 1) {
                fmt::print(stderr, "Invalid IPv6 address: {}\n", ipv6_addr);
                success = false;
            } else {
                row6.OnLinkPrefixLength = ipv6_prefix_len;
                row6.DadState = IpDadStatePreferred;
                row6.PrefixOrigin = IpPrefixOriginManual;
                row6.SuffixOrigin = IpSuffixOriginManual;
                row6.ValidLifetime = 0xFFFFFFFF;
                row6.PreferredLifetime = 0xFFFFFFFF;

                DWORD result = CreateUnicastIpAddressEntry(&row6);
                if (result != NO_ERROR) {
                    fmt::print(stderr, "Failed to set IPv6 address {}: {}\n",
                               ipv6_addr, get_error_string(result));
                    success = false;
                } else {
                    fmt::println("Configured IPv6: {}/{}", ipv6_addr, ipv6_prefix_len);
                }
            }
        }

        return success;
    }
} // namespace stack::device
#endif // _WIN32
