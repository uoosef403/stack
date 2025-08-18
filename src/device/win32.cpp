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

    namespace {
        // A helper to get a descriptive error message from a Windows error code.
        std::string get_error_string(DWORD error_code) {
            char* buffer = nullptr;
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
        UCHAR netmask_to_prefix_length(const std::string& netmask) {
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

        if (!configure("192.168.69.69", "255.255.255.0")) {
            fmt::println(stderr, "Failed to assign ip to Wintun adapter, do this yourself");
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

    /**
     * @brief Configures the Wintun adapter's IP address, subnet, and brings it online.
     *
     * This function is designed to be robust. It reliably finds the adapter using the
     * GUID it was created with, flushes any pre-existing IP addresses to prevent
     * conflicts, sets the new IP configuration, and enables the adapter.
     *
     * @param ip_addr The desired IPv4 address (e.g., "10.8.0.1").
     * @param netmask The desired IPv4 subnet mask (e.g., "255.255.255.0").
     * @return True on success, false on failure.
     */
    bool Win32Tun::configure(const std::string& ip_addr, const std::string& netmask) const {
        // This function requires the adapter to have been created via open() first.
        if (!m_adapter) {
            fmt::print(stderr, "Configuration failed: Wintun adapter is not open.\n");
            return false;
        }

        // 1. Find the adapter's LUID and Index by its GUID.
        // We retry for a short period, as the adapter might take a moment to be visible to the OS.
        std::optional<NET_LUID> luid_opt;

        RPC_CSTR guid_str_rpc;
        if (UuidToStringA(&m_guid, &guid_str_rpc) != RPC_S_OK) {
            fmt::print(stderr, "Failed to convert adapter GUID to string.\n");
            return false;
        }
        std::string target_guid_str = reinterpret_cast<char*>(guid_str_rpc);
        RpcStringFreeA(&guid_str_rpc);

        for (int attempt = 0; attempt < 10; ++attempt) { // Retry for up to 1 second
            ULONG buffer_size = 0;
            GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, nullptr, &buffer_size);
            if (buffer_size == 0) continue;

            std::vector<BYTE> buffer(buffer_size);
            auto* adapters = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buffer.data());

            if (GetAdaptersAddresses(AF_UNSPEC, GAA_FLAG_INCLUDE_PREFIX, nullptr, adapters, &buffer_size) == NO_ERROR) {
                for (auto* curr = adapters; curr != nullptr; curr = curr->Next) {
                    // AdapterName is the GUID string like "{...}"
                    if (target_guid_str.compare(curr->AdapterName) == 0) {
                        luid_opt = curr->Luid;
                        break;
                    }
                }
            }
            if (luid_opt) break;
            Sleep(100); // Wait and retry
        }

        if (!luid_opt) {
            fmt::print(stderr, "Could not find Wintun adapter with GUID {}. The adapter may have failed to initialize.\n", target_guid_str);
            return false;
        }

        // 2. Flush any existing IP addresses on this interface to ensure a clean slate.
        PMIB_UNICASTIPADDRESS_TABLE table = nullptr;
        if (GetUnicastIpAddressTable(AF_INET, &table) == NO_ERROR) {
            for (ULONG i = 0; i < table->NumEntries; ++i) {
                if (table->Table[i].InterfaceLuid.Value == luid_opt->Value) {
                    // This is a best-effort deletion. We don't fail if it doesn't work.
                    DeleteUnicastIpAddressEntry(&table->Table[i]);
                }
            }
            FreeMibTable(table);
        }

        // 3. Set the new IPv4 address and subnet mask.
        MIB_UNICASTIPADDRESS_ROW ip_row;
        InitializeUnicastIpAddressEntry(&ip_row);
        ip_row.InterfaceLuid = *luid_opt;
        ip_row.Address.si_family = AF_INET;

        if (InetPtonA(AF_INET, ip_addr.c_str(), &ip_row.Address.Ipv4.sin_addr) != 1) {
            fmt::print(stderr, "Invalid IP address format: {}.\n", ip_addr);
            return false;
        }

        ip_row.OnLinkPrefixLength = netmask_to_prefix_length(netmask);
        if (ip_row.OnLinkPrefixLength == 0 && netmask != "0.0.0.0") {
            fmt::print(stderr, "Invalid netmask format: {}.\n", netmask);
            return false;
        }

        // Set standard properties for a manually configured IP.
        ip_row.DadState = IpDadStatePreferred;
        ip_row.PrefixOrigin = IpPrefixOriginManual;
        ip_row.SuffixOrigin = IpSuffixOriginManual;
        ip_row.ValidLifetime = 0xFFFFFFFF;   // Infinite
        ip_row.PreferredLifetime = 0xFFFFFFFF; // Infinite

        DWORD result = CreateUnicastIpAddressEntry(&ip_row);
        if (result != NO_ERROR) {
            if (result == ERROR_ACCESS_DENIED) {
                 fmt::print(stderr, "Permission denied to set IP address. Please run as Administrator.\n");
            } else {
                 fmt::print(stderr, "Failed to set IP address. Error: {}\n", get_error_string(result));
            }
            return false;
        }

        // no need to bring wintun up because its aleady up
        fmt::println("Successfully configured TUN device: IP={}, Netmask={}", ip_addr, netmask);
        return true;
    }
} // namespace stack::device
#endif // _WIN32
