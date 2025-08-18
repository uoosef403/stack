#pragma once

#include "tun.h"

#if defined(_WIN32)

#include <windows.h>
#include "wintun.h"

namespace stack::device {

    /**
     * @brief Windows-specific implementation of the Tun interface using the Wintun driver.
     */
    class Win32Tun final : public Tun {
    public:
        ~Win32Tun() override;

        Win32Tun(const Win32Tun&) = delete;
        Win32Tun& operator=(const Win32Tun&) = delete;
        Win32Tun(Win32Tun&&) = delete;
        Win32Tun& operator=(Win32Tun&&) = delete;

        asio::awaitable<void> run_reader(IPPacketChannel& incoming_channel) override;
        asio::awaitable<void> run_writer(IPPacketChannel& outgoing_channel) override;

        bool configure(const std::string &ip_addr, const std::string &netmask) const;

        /**
         * @brief Factory method to create and open a new TUN device on Windows.
         */
        static std::unique_ptr<Tun> create(asio::io_context& io_context, const std::string& name);

    private:
        explicit Win32Tun(asio::io_context& io_context);
        bool open(const std::string& name);
        bool initialize_wintun();

        using WINTUN_CREATE_ADAPTER_FUNC = WINTUN_ADAPTER_HANDLE(WINAPI*)(const WCHAR*, const WCHAR*, const GUID*);
        using WINTUN_CLOSE_ADAPTER_FUNC = void(WINAPI*)(WINTUN_ADAPTER_HANDLE);
        using WINTUN_START_SESSION_FUNC = WINTUN_SESSION_HANDLE(WINAPI*)(WINTUN_ADAPTER_HANDLE, DWORD);
        using WINTUN_END_SESSION_FUNC = void(WINAPI*)(WINTUN_SESSION_HANDLE);
        using WINTUN_GET_READ_WAIT_EVENT_FUNC = HANDLE(WINAPI*)(WINTUN_SESSION_HANDLE);
        using WINTUN_RECEIVE_PACKET_FUNC = BYTE*(WINAPI*)(WINTUN_SESSION_HANDLE, DWORD*);
        using WINTUN_RELEASE_RECEIVE_PACKET_FUNC = void(WINAPI*)(WINTUN_SESSION_HANDLE, const BYTE*);
        using WINTUN_ALLOCATE_SEND_PACKET_FUNC = BYTE*(WINAPI*)(WINTUN_SESSION_HANDLE, DWORD);
        using WINTUN_SEND_PACKET_FUNC = void(WINAPI*)(WINTUN_SESSION_HANDLE, const BYTE*);

        asio::windows::object_handle m_read_handle;
        HMODULE m_wintun_dll = nullptr;
        WINTUN_ADAPTER_HANDLE m_adapter = nullptr;
        WINTUN_SESSION_HANDLE m_session = nullptr;

        WINTUN_CREATE_ADAPTER_FUNC m_WintunCreateAdapter = nullptr;
        WINTUN_CLOSE_ADAPTER_FUNC m_WintunCloseAdapter = nullptr;
        WINTUN_START_SESSION_FUNC m_WintunStartSession = nullptr;
        WINTUN_END_SESSION_FUNC m_WintunEndSession = nullptr;
        WINTUN_GET_READ_WAIT_EVENT_FUNC m_WintunGetReadWaitEvent = nullptr;
        WINTUN_RECEIVE_PACKET_FUNC m_WintunReceivePacket = nullptr;
        WINTUN_RELEASE_RECEIVE_PACKET_FUNC m_WintunReleaseReceivePacket = nullptr;
        WINTUN_ALLOCATE_SEND_PACKET_FUNC m_WintunAllocateSendPacket = nullptr;
        WINTUN_SEND_PACKET_FUNC m_WintunSendPacket = nullptr;
        GUID m_guid{};
    };

} // namespace stack::device
#endif // _WIN32