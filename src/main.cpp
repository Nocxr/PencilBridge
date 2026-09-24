#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <bcrypt.h>
#include <wincrypt.h>
#include <wincodec.h>
#define SECURITY_WIN32
#include <security.h>
#include <schannel.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <cmath>
#include <cctype>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#include "web_ui.h"

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "ole32.lib")
#pragma comment(lib, "secur32.lib")
#pragma comment(lib, "uxtheme.lib")
#pragma comment(lib, "windowscodecs.lib")
#pragma comment(lib, "ws2_32.lib")

namespace
{
constexpr UINT WM_APP_STATUS = WM_APP + 1;
constexpr int ID_TARGET_COMBO = 1001;
constexpr int ID_REFRESH_BUTTON = 1002;
constexpr int ID_SELECT_ZONE_BUTTON = 1003;
constexpr int ID_CLEAR_ZONE_BUTTON = 1004;
constexpr int ID_SEND_CLIPBOARD_BUTTON = 1005;
constexpr int ID_AUTO_CLIPBOARD_CHECK = 1006;
constexpr int ID_SELECT_SCREEN_RECT_BUTTON = 1007;
constexpr int ID_WHITEBOARD_TOGGLE = 1008;
constexpr int ID_WHITEBOARD_CLIP = 1009;
constexpr int ID_WHITEBOARD_CLEAR = 1010;
constexpr UINT_PTR ID_ZONE_TRACK_TIMER = 2001;
constexpr int kPort = 8765;
constexpr int kBootstrapPort = 8764;
constexpr wchar_t kZoneSelectClassName[] = L"PencilBridgeZoneSelect";
constexpr wchar_t kZoneOutlineClassName[] = L"PencilBridgeZoneOutline";
constexpr wchar_t kWhiteboardClassName[] = L"PencilBridgeWhiteboardOverlay";

struct WindowEntry
{
    HWND hwnd = nullptr;
    std::wstring label;
};

HWND gMainWindow = nullptr;
HWND gTargetCombo = nullptr;
HWND gUrlText = nullptr;
HWND gStatusText = nullptr;
HWND gZoneText = nullptr;
HWND gAutoClipboardCheck = nullptr;
HWND gZoneSelectWindow = nullptr;
HWND gZoneOutlineWindow = nullptr;
HWND gZoneOutlineOwner = nullptr;
HWND gWhiteboardWindow = nullptr;
HWND gWhiteboardToggle = nullptr;
HINSTANCE gInstance = nullptr;
HBRUSH gDarkBackgroundBrush = nullptr;
HBRUSH gDarkControlBrush = nullptr;

constexpr COLORREF kDarkBackground = RGB(17, 19, 24);
constexpr COLORREF kDarkControl = RGB(28, 32, 39);
constexpr COLORREF kDarkText = RGB(228, 231, 236);

std::atomic<bool> gZoneActive{false};
std::atomic<HWND> gZoneTarget{nullptr};
std::atomic<double> gZoneX{0.0};
std::atomic<double> gZoneY{0.0};
std::atomic<double> gZoneWidth{1.0};
std::atomic<double> gZoneHeight{1.0};

std::atomic<bool> gScreenRectActive{false};
std::atomic<int> gScreenRectLeft{0};
std::atomic<int> gScreenRectTop{0};
std::atomic<int> gScreenRectWidth{1};
std::atomic<int> gScreenRectHeight{1};

struct WhiteboardSegment
{
    double x1 = 0.0;
    double y1 = 0.0;
    double x2 = 0.0;
    double y2 = 0.0;
    int width = 4;
    COLORREF color = RGB(255, 70, 60);
};

std::atomic<bool> gWhiteboardActive{false};
std::atomic<int> gWhiteboardBrushSize{7};
std::atomic<COLORREF> gWhiteboardBrushColor{RGB(255, 70, 60)};
std::mutex gWhiteboardMutex;
std::vector<WhiteboardSegment> gWhiteboardSegments;
bool gWhiteboardStrokeActive = false;
double gWhiteboardLastX = 0.0;
double gWhiteboardLastY = 0.0;

bool gSelectingScreenRect = false;
bool gZoneDragging = false;
POINT gZoneDragStart{};
POINT gZoneDragCurrent{};

std::vector<WindowEntry> gWindows;
std::atomic<HWND> gTargetWindow{nullptr};
std::atomic<bool> gRunning{true};
std::atomic<SOCKET> gListenSocket{INVALID_SOCKET};
std::atomic<SOCKET> gBootstrapListenSocket{INVALID_SOCKET};
std::atomic<SOCKET> gWebSocketClient{INVALID_SOCKET};
std::mutex gWebSocketSendMutex;
std::mutex gOpenClientSocketsMutex;
std::vector<SOCKET> gOpenClientSockets;
std::thread gServerThread;
std::thread gBootstrapThread;

struct TlsConnection
{
    CtxtHandle context{};
    bool contextValid = false;
    SecPkgContext_StreamSizes streamSizes{};
    std::vector<uint8_t> encryptedPending;
    std::vector<uint8_t> plainPending;
    std::mutex sendMutex;
    std::mutex recvMutex;

    ~TlsConnection()
    {
        if (contextValid)
        {
            DeleteSecurityContext(&context);
        }
    }
};

CredHandle gTlsCredentials{};
bool gTlsCredentialsValid = false;
HCERTSTORE gTlsCertificateStore = nullptr;
PCCERT_CONTEXT gTlsServerCertificate = nullptr;
std::mutex gTlsConnectionsMutex;
std::unordered_map<SOCKET, std::shared_ptr<TlsConnection>> gTlsConnections;

IWICImagingFactory* gWicFactory = nullptr;
std::atomic<bool> gAutoSendClipboard{true};
std::atomic<bool> gIgnoreNextClipboardUpdate{false};

HSYNTHETICPOINTERDEVICE gPenDevice = nullptr;
std::mutex gInputMutex;
bool gPenDown = false;
bool gPenInRange = false;
POINT gLastPenScreenPoint{};
int gLastPenTiltX = 0;
int gLastPenTiltY = 0;

struct TargetGeometry
{
    HWND target = nullptr;
    POINT origin{};
    int width = 0;
    int height = 0;
    bool valid = false;
};

TargetGeometry gPenTargetGeometry;
std::atomic<int> gVirtualDesktopX{0};
std::atomic<int> gVirtualDesktopY{0};
std::atomic<int> gVirtualDesktopWidth{1};
std::atomic<int> gVirtualDesktopHeight{1};

bool gTouchDown = false;
int gActiveTouchPointer = -1;
bool gTouchInjectionReady = false;

struct GestureTouchState
{
    int id = -1;
    UINT32 injectedId = 0;
    POINT point{};
    bool active = false;
    bool primary = false;
};

std::array<GestureTouchState, 4> gGestureTouches{};

void ApplyDefaultFont(HWND hwnd)
{
    SendMessageW(hwnd, WM_SETFONT, reinterpret_cast<WPARAM>(GetStockObject(DEFAULT_GUI_FONT)), TRUE);
}

void ApplyDarkWindowTheme(HWND hwnd)
{
    if (!hwnd)
    {
        return;
    }

    const BOOL dark = TRUE;
    constexpr DWMWINDOWATTRIBUTE kUseImmersiveDarkMode =
        static_cast<DWMWINDOWATTRIBUTE>(20);
    DwmSetWindowAttribute(
        hwnd,
        kUseImmersiveDarkMode,
        &dark,
        sizeof(dark));
    SetWindowTheme(hwnd, L"DarkMode_Explorer", nullptr);
}

void ApplyDarkControlTheme(HWND hwnd)
{
    if (!hwnd)
    {
        return;
    }

    SetWindowTheme(hwnd, L"DarkMode_Explorer", nullptr);
    SendMessageW(hwnd, WM_THEMECHANGED, 0, 0);
}

void RegisterOpenClientSocket(SOCKET socket)
{
    std::lock_guard lock(gOpenClientSocketsMutex);
    gOpenClientSockets.push_back(socket);
}

void UnregisterOpenClientSocket(SOCKET socket)
{
    std::lock_guard lock(gOpenClientSocketsMutex);
    const auto it = std::find(
        gOpenClientSockets.begin(),
        gOpenClientSockets.end(),
        socket);
    if (it != gOpenClientSockets.end())
    {
        gOpenClientSockets.erase(it);
    }
}

void ShutdownOpenClientSockets()
{
    std::lock_guard lock(gOpenClientSocketsMutex);
    for (SOCKET socket : gOpenClientSockets)
    {
        if (socket != INVALID_SOCKET)
        {
            shutdown(socket, SD_BOTH);
        }
    }
}

void PostStatus(const std::wstring& message)
{
    if (!gMainWindow)
    {
        return;
    }

    auto* copy = new std::wstring(message);
    if (!PostMessageW(gMainWindow, WM_APP_STATUS, 0, reinterpret_cast<LPARAM>(copy)))
    {
        delete copy;
    }
}

std::wstring GetProcessName(HWND hwnd)
{
    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    if (!pid)
    {
        return {};
    }

    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process)
    {
        return {};
    }

    wchar_t path[1024]{};
    DWORD size = static_cast<DWORD>(std::size(path));
    std::wstring result;
    if (QueryFullProcessImageNameW(process, 0, path, &size))
    {
        std::wstring full(path, size);
        const size_t slash = full.find_last_of(L"\\/");
        result = slash == std::wstring::npos ? full : full.substr(slash + 1);
    }
    CloseHandle(process);
    return result;
}

BOOL CALLBACK EnumWindowsProc(HWND hwnd, LPARAM)
{
    if (hwnd == gMainWindow || !IsWindowVisible(hwnd))
    {
        return TRUE;
    }

    const int titleLength = GetWindowTextLengthW(hwnd);
    if (titleLength <= 0)
    {
        return TRUE;
    }

    std::wstring title(static_cast<size_t>(titleLength) + 1, L'\0');
    const int copied = GetWindowTextW(hwnd, title.data(), titleLength + 1);
    if (copied <= 0)
    {
        return TRUE;
    }
    title.resize(static_cast<size_t>(copied));

    const std::wstring processName = GetProcessName(hwnd);
    std::wstring label = title;
    if (!processName.empty())
    {
        label += L"  —  ";
        label += processName;
    }

    gWindows.push_back({hwnd, std::move(label)});
    return TRUE;
}

void ClearZone();
void UpdateZoneOutline();
void BeginZoneSelection();
void BeginScreenRectSelection();
void UpdateWhiteboardOverlay();
void SetWhiteboardActive(bool active);
void ClearWhiteboardInk();
void ClipWhiteboardToClipboard();

bool ActivateTargetWindow(HWND target)
{
    if (!target || !IsWindow(target))
    {
        return false;
    }

    if (IsIconic(target))
    {
        ShowWindowAsync(target, SW_RESTORE);
    }

    BringWindowToTop(target);
    SetForegroundWindow(target);
    return IsWindowVisible(target) != FALSE;
}

void SelectComboIndex(int index, bool activate)
{
    if (index < 0 || index >= static_cast<int>(gWindows.size()))
    {
        gTargetWindow.store(nullptr);
        return;
    }

    const HWND target = gWindows[static_cast<size_t>(index)].hwnd;
    const HWND previousTarget = gTargetWindow.exchange(target);

    if (previousTarget && previousTarget != target &&
        gZoneActive.load(std::memory_order_relaxed))
    {
        ClearZone();
    }

    if (activate)
    {
        ActivateTargetWindow(target);
    }

    const std::wstring status =
        (activate ? L"Armed target: " : L"Target: ") +
        gWindows[static_cast<size_t>(index)].label;
    SetWindowTextW(gStatusText, status.c_str());
}

void RefreshWindows()
{
    const HWND previous = gTargetWindow.load();

    gWindows.clear();
    SendMessageW(gTargetCombo, CB_RESETCONTENT, 0, 0);
    EnumWindows(EnumWindowsProc, 0);

    int desiredIndex = -1;
    for (size_t i = 0; i < gWindows.size(); ++i)
    {
        SendMessageW(gTargetCombo, CB_ADDSTRING, 0, reinterpret_cast<LPARAM>(gWindows[i].label.c_str()));
        if (gWindows[i].hwnd == previous)
        {
            desiredIndex = static_cast<int>(i);
        }
    }

    if (desiredIndex < 0 && !gWindows.empty())
    {
        desiredIndex = 0;
    }

    if (desiredIndex >= 0)
    {
        SendMessageW(gTargetCombo, CB_SETCURSEL, desiredIndex, 0);
        SelectComboIndex(desiredIndex, false);
    }
    else
    {
        gTargetWindow.store(nullptr);
        SetWindowTextW(gStatusText, L"No eligible target windows found.");
    }
}

std::string GetLocalIPv4()
{
    char hostname[256]{};
    if (gethostname(hostname, static_cast<int>(sizeof(hostname))) == SOCKET_ERROR)
    {
        return "127.0.0.1";
    }

    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* results = nullptr;
    if (getaddrinfo(hostname, nullptr, &hints, &results) != 0)
    {
        return "127.0.0.1";
    }

    std::string best = "127.0.0.1";
    for (addrinfo* it = results; it; it = it->ai_next)
    {
        auto* addr = reinterpret_cast<sockaddr_in*>(it->ai_addr);
        char buffer[INET_ADDRSTRLEN]{};
        if (!inet_ntop(AF_INET, &addr->sin_addr, buffer, static_cast<DWORD>(sizeof(buffer))))
        {
            continue;
        }

        std::string candidate(buffer);
        if (candidate.rfind("127.", 0) == 0)
        {
            continue;
        }

        if (candidate.rfind("192.168.", 0) == 0 ||
            candidate.rfind("10.", 0) == 0 ||
            candidate.rfind("172.", 0) == 0)
        {
            best = candidate;
            break;
        }

        if (best == "127.0.0.1")
        {
            best = candidate;
        }
    }

    freeaddrinfo(results);
    return best;
}

std::string GetConfiguredServerIPv4()
{
    std::ifstream file(
        "Saved/PencilBridge/TLS/server-ip.txt",
        std::ios::binary);
    if (file)
    {
        std::string configured{
            std::istreambuf_iterator<char>(file),
            std::istreambuf_iterator<char>()};

        while (!configured.empty() &&
               (configured.back() == '\r' ||
                configured.back() == '\n' ||
                configured.back() == ' ' ||
                configured.back() == '\t'))
        {
            configured.pop_back();
        }

        sockaddr_in address{};
        if (!configured.empty() &&
            inet_pton(
                AF_INET,
                configured.c_str(),
                &address.sin_addr) == 1)
        {
            return configured;
        }
    }

    return GetLocalIPv4();
}

bool RawSendAll(SOCKET socket, const char* data, size_t size)
{
    size_t sent = 0;
    while (sent < size)
    {
        const int chunk =
            send(
                socket,
                data + sent,
                static_cast<int>(
                    std::min<size_t>(
                        size - sent,
                        static_cast<size_t>(INT_MAX))),
                0);
        if (chunk <= 0)
        {
            return false;
        }
        sent += static_cast<size_t>(chunk);
    }
    return true;
}

int RawRecvSome(SOCKET socket, char* data, int size)
{
    return recv(socket, data, size, 0);
}

std::shared_ptr<TlsConnection> GetTlsConnection(SOCKET socket)
{
    std::lock_guard lock(gTlsConnectionsMutex);
    const auto it = gTlsConnections.find(socket);
    return it == gTlsConnections.end()
        ? nullptr
        : it->second;
}

void RegisterTlsConnection(
    SOCKET socket,
    const std::shared_ptr<TlsConnection>& connection)
{
    std::lock_guard lock(gTlsConnectionsMutex);
    gTlsConnections[socket] = connection;
}

void UnregisterTlsConnection(SOCKET socket)
{
    std::lock_guard lock(gTlsConnectionsMutex);
    gTlsConnections.erase(socket);
}

bool InitializeTlsCredentials()
{
    if (gTlsCredentialsValid)
    {
        return true;
    }

    gTlsCertificateStore = CertOpenStore(
        CERT_STORE_PROV_SYSTEM_W,
        0,
        0,
        CERT_SYSTEM_STORE_CURRENT_USER,
        L"MY");
    if (!gTlsCertificateStore)
    {
        return false;
    }

    gTlsServerCertificate = CertFindCertificateInStore(
        gTlsCertificateStore,
        X509_ASN_ENCODING | PKCS_7_ASN_ENCODING,
        0,
        CERT_FIND_SUBJECT_STR_W,
        L"PencilBridge Local Server",
        nullptr);
    if (!gTlsServerCertificate)
    {
        CertCloseStore(gTlsCertificateStore, 0);
        gTlsCertificateStore = nullptr;
        return false;
    }

    SCHANNEL_CRED credential{};
    credential.dwVersion = SCHANNEL_CRED_VERSION;
    credential.cCreds = 1;
    credential.paCred = &gTlsServerCertificate;
    credential.dwFlags =
        SCH_CRED_NO_DEFAULT_CREDS |
        SCH_CRED_NO_SYSTEM_MAPPER;

    TimeStamp expiry{};
    const SECURITY_STATUS status =
        AcquireCredentialsHandleW(
            nullptr,
            const_cast<wchar_t*>(UNISP_NAME_W),
            SECPKG_CRED_INBOUND,
            nullptr,
            &credential,
            nullptr,
            nullptr,
            &gTlsCredentials,
            &expiry);

    if (status != SEC_E_OK)
    {
        CertFreeCertificateContext(gTlsServerCertificate);
        gTlsServerCertificate = nullptr;
        CertCloseStore(gTlsCertificateStore, 0);
        gTlsCertificateStore = nullptr;
        return false;
    }

    gTlsCredentialsValid = true;
    return true;
}

void ShutdownTlsCredentials()
{
    {
        std::lock_guard lock(gTlsConnectionsMutex);
        gTlsConnections.clear();
    }

    if (gTlsCredentialsValid)
    {
        FreeCredentialsHandle(&gTlsCredentials);
        gTlsCredentialsValid = false;
    }

    if (gTlsServerCertificate)
    {
        CertFreeCertificateContext(gTlsServerCertificate);
        gTlsServerCertificate = nullptr;
    }

    if (gTlsCertificateStore)
    {
        CertCloseStore(gTlsCertificateStore, 0);
        gTlsCertificateStore = nullptr;
    }
}

bool AcceptTlsConnection(
    SOCKET socket,
    const std::shared_ptr<TlsConnection>& connection)
{
    if (!gTlsCredentialsValid || !connection)
    {
        return false;
    }

    constexpr DWORD requestFlags =
        ASC_REQ_SEQUENCE_DETECT |
        ASC_REQ_REPLAY_DETECT |
        ASC_REQ_CONFIDENTIALITY |
        ASC_REQ_EXTENDED_ERROR |
        ASC_REQ_STREAM |
        ASC_REQ_ALLOCATE_MEMORY;

    std::vector<uint8_t> incoming;
    incoming.reserve(32 * 1024);

    while (gRunning.load())
    {
        if (incoming.empty())
        {
            std::array<uint8_t, 16 * 1024> buffer{};
            const int received =
                RawRecvSome(
                    socket,
                    reinterpret_cast<char*>(buffer.data()),
                    static_cast<int>(buffer.size()));
            if (received <= 0)
            {
                return false;
            }
            incoming.insert(
                incoming.end(),
                buffer.begin(),
                buffer.begin() + received);
        }

        SecBuffer inputBuffers[2]{};
        inputBuffers[0].BufferType = SECBUFFER_TOKEN;
        inputBuffers[0].pvBuffer = incoming.data();
        inputBuffers[0].cbBuffer =
            static_cast<unsigned long>(incoming.size());
        inputBuffers[1].BufferType = SECBUFFER_EMPTY;

        SecBufferDesc inputDesc{};
        inputDesc.ulVersion = SECBUFFER_VERSION;
        inputDesc.cBuffers = 2;
        inputDesc.pBuffers = inputBuffers;

        SecBuffer outputBuffer{};
        outputBuffer.BufferType = SECBUFFER_TOKEN;

        SecBufferDesc outputDesc{};
        outputDesc.ulVersion = SECBUFFER_VERSION;
        outputDesc.cBuffers = 1;
        outputDesc.pBuffers = &outputBuffer;

        DWORD attributes = 0;
        TimeStamp expiry{};

        const SECURITY_STATUS status =
            AcceptSecurityContext(
                &gTlsCredentials,
                connection->contextValid
                    ? &connection->context
                    : nullptr,
                &inputDesc,
                requestFlags,
                SECURITY_NATIVE_DREP,
                &connection->context,
                &outputDesc,
                &attributes,
                &expiry);

        if (outputBuffer.pvBuffer &&
            outputBuffer.cbBuffer > 0)
        {
            const bool sent =
                RawSendAll(
                    socket,
                    static_cast<const char*>(
                        outputBuffer.pvBuffer),
                    outputBuffer.cbBuffer);
            FreeContextBuffer(outputBuffer.pvBuffer);
            outputBuffer.pvBuffer = nullptr;

            if (!sent)
            {
                return false;
            }
        }

        if (status == SEC_E_INCOMPLETE_MESSAGE)
        {
            std::array<uint8_t, 16 * 1024> buffer{};
            const int received =
                RawRecvSome(
                    socket,
                    reinterpret_cast<char*>(buffer.data()),
                    static_cast<int>(buffer.size()));
            if (received <= 0)
            {
                return false;
            }
            incoming.insert(
                incoming.end(),
                buffer.begin(),
                buffer.begin() + received);
            continue;
        }

        if (status != SEC_E_OK &&
            status != SEC_I_CONTINUE_NEEDED)
        {
            if (connection->contextValid)
            {
                DeleteSecurityContext(
                    &connection->context);
                connection->contextValid = false;
            }
            return false;
        }

        connection->contextValid = true;

        size_t extraBytes = 0;
        if (inputBuffers[1].BufferType ==
            SECBUFFER_EXTRA)
        {
            extraBytes =
                inputBuffers[1].cbBuffer;
        }

        if (extraBytes > 0 &&
            extraBytes <= incoming.size())
        {
            std::vector<uint8_t> extra(
                incoming.end() -
                    static_cast<std::ptrdiff_t>(
                        extraBytes),
                incoming.end());
            incoming.swap(extra);
        }
        else
        {
            incoming.clear();
        }

        if (status == SEC_I_CONTINUE_NEEDED)
        {
            continue;
        }

        if (QueryContextAttributesW(
                &connection->context,
                SECPKG_ATTR_STREAM_SIZES,
                &connection->streamSizes) != SEC_E_OK)
        {
            return false;
        }

        connection->encryptedPending =
            std::move(incoming);
        return true;
    }

    return false;
}

bool TlsSendAll(
    const std::shared_ptr<TlsConnection>& connection,
    SOCKET socket,
    const char* data,
    size_t size)
{
    if (!connection ||
        !connection->contextValid)
    {
        return false;
    }

    std::lock_guard lock(connection->sendMutex);

    size_t sent = 0;
    while (sent < size)
    {
        const size_t maxMessage =
            std::max<size_t>(
                1,
                connection->streamSizes.cbMaximumMessage);
        const size_t chunkSize =
            std::min(size - sent, maxMessage);

        std::vector<uint8_t> packet(
            static_cast<size_t>(
                connection->streamSizes.cbHeader) +
            chunkSize +
            static_cast<size_t>(
                connection->streamSizes.cbTrailer));

        uint8_t* dataStart =
            packet.data() +
            connection->streamSizes.cbHeader;
        std::memcpy(
            dataStart,
            data + sent,
            chunkSize);

        SecBuffer buffers[4]{};
        buffers[0].BufferType = SECBUFFER_STREAM_HEADER;
        buffers[0].pvBuffer = packet.data();
        buffers[0].cbBuffer =
            connection->streamSizes.cbHeader;

        buffers[1].BufferType = SECBUFFER_DATA;
        buffers[1].pvBuffer = dataStart;
        buffers[1].cbBuffer =
            static_cast<unsigned long>(chunkSize);

        buffers[2].BufferType = SECBUFFER_STREAM_TRAILER;
        buffers[2].pvBuffer =
            dataStart + chunkSize;
        buffers[2].cbBuffer =
            connection->streamSizes.cbTrailer;

        buffers[3].BufferType = SECBUFFER_EMPTY;

        SecBufferDesc desc{};
        desc.ulVersion = SECBUFFER_VERSION;
        desc.cBuffers = 4;
        desc.pBuffers = buffers;

        if (EncryptMessage(
                &connection->context,
                0,
                &desc,
                0) != SEC_E_OK)
        {
            return false;
        }

        const size_t encryptedSize =
            static_cast<size_t>(
                buffers[0].cbBuffer) +
            static_cast<size_t>(
                buffers[1].cbBuffer) +
            static_cast<size_t>(
                buffers[2].cbBuffer);

        if (!RawSendAll(
                socket,
                reinterpret_cast<const char*>(
                    packet.data()),
                encryptedSize))
        {
            return false;
        }

        sent += chunkSize;
    }

    return true;
}

int TlsRecvSome(
    const std::shared_ptr<TlsConnection>& connection,
    SOCKET socket,
    char* output,
    int outputSize)
{
    if (!connection ||
        !connection->contextValid ||
        outputSize <= 0)
    {
        return -1;
    }

    std::lock_guard lock(connection->recvMutex);

    auto copyPlain = [&]() -> int
    {
        if (connection->plainPending.empty())
        {
            return 0;
        }

        const size_t count =
            std::min<size_t>(
                connection->plainPending.size(),
                static_cast<size_t>(outputSize));
        std::memcpy(
            output,
            connection->plainPending.data(),
            count);
        connection->plainPending.erase(
            connection->plainPending.begin(),
            connection->plainPending.begin() +
                static_cast<std::ptrdiff_t>(count));
        return static_cast<int>(count);
    };

    if (const int copied = copyPlain();
        copied > 0)
    {
        return copied;
    }

    while (gRunning.load())
    {
        if (connection->encryptedPending.empty())
        {
            std::array<uint8_t, 16 * 1024> buffer{};
            const int received =
                RawRecvSome(
                    socket,
                    reinterpret_cast<char*>(buffer.data()),
                    static_cast<int>(buffer.size()));
            if (received <= 0)
            {
                return received;
            }
            connection->encryptedPending.insert(
                connection->encryptedPending.end(),
                buffer.begin(),
                buffer.begin() + received);
        }

        SecBuffer buffers[4]{};
        buffers[0].BufferType = SECBUFFER_DATA;
        buffers[0].pvBuffer =
            connection->encryptedPending.data();
        buffers[0].cbBuffer =
            static_cast<unsigned long>(
                connection->encryptedPending.size());
        for (int i = 1; i < 4; ++i)
        {
            buffers[i].BufferType =
                SECBUFFER_EMPTY;
        }

        SecBufferDesc desc{};
        desc.ulVersion = SECBUFFER_VERSION;
        desc.cBuffers = 4;
        desc.pBuffers = buffers;

        const SECURITY_STATUS status =
            DecryptMessage(
                &connection->context,
                &desc,
                0,
                nullptr);

        if (status == SEC_E_INCOMPLETE_MESSAGE)
        {
            std::array<uint8_t, 16 * 1024> buffer{};
            const int received =
                RawRecvSome(
                    socket,
                    reinterpret_cast<char*>(buffer.data()),
                    static_cast<int>(buffer.size()));
            if (received <= 0)
            {
                return received;
            }
            connection->encryptedPending.insert(
                connection->encryptedPending.end(),
                buffer.begin(),
                buffer.begin() + received);
            continue;
        }

        if (status == SEC_I_CONTEXT_EXPIRED)
        {
            return 0;
        }

        if (status != SEC_E_OK)
        {
            return -1;
        }

        std::vector<uint8_t> plaintext;
        std::vector<uint8_t> extra;

        for (SecBuffer& buffer : buffers)
        {
            if (buffer.BufferType == SECBUFFER_DATA &&
                buffer.pvBuffer &&
                buffer.cbBuffer > 0)
            {
                const auto* begin =
                    static_cast<const uint8_t*>(
                        buffer.pvBuffer);
                plaintext.assign(
                    begin,
                    begin + buffer.cbBuffer);
            }
            else if (
                buffer.BufferType == SECBUFFER_EXTRA &&
                buffer.pvBuffer &&
                buffer.cbBuffer > 0)
            {
                const auto* begin =
                    static_cast<const uint8_t*>(
                        buffer.pvBuffer);
                extra.assign(
                    begin,
                    begin + buffer.cbBuffer);
            }
        }

        connection->encryptedPending =
            std::move(extra);
        if (!plaintext.empty())
        {
            connection->plainPending =
                std::move(plaintext);
            return copyPlain();
        }
    }

    return 0;
}

bool SendAll(SOCKET socket, const char* data, size_t size)
{
    if (auto tls = GetTlsConnection(socket))
    {
        return TlsSendAll(
            tls,
            socket,
            data,
            size);
    }
    return RawSendAll(socket, data, size);
}

bool SendAll(SOCKET socket, const std::string& data)
{
    return SendAll(
        socket,
        data.data(),
        data.size());
}

int RecvSome(SOCKET socket, char* data, int size)
{
    if (auto tls = GetTlsConnection(socket))
    {
        return TlsRecvSome(
            tls,
            socket,
            data,
            size);
    }
    return RawRecvSome(socket, data, size);
}

std::string Trim(std::string value)
{
    while (!value.empty() && (value.front() == ' ' || value.front() == '\t' || value.front() == '\r' || value.front() == '\n'))
    {
        value.erase(value.begin());
    }
    while (!value.empty() && (value.back() == ' ' || value.back() == '\t' || value.back() == '\r' || value.back() == '\n'))
    {
        value.pop_back();
    }
    return value;
}

std::string ToLower(std::string value)
{
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c)
    {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::string GetHeaderValue(const std::string& request, const std::string& wantedName)
{
    std::istringstream stream(request);
    std::string line;
    const std::string wanted = ToLower(wantedName);

    while (std::getline(stream, line))
    {
        const size_t colon = line.find(':');
        if (colon == std::string::npos)
        {
            continue;
        }

        if (ToLower(Trim(line.substr(0, colon))) == wanted)
        {
            return Trim(line.substr(colon + 1));
        }
    }
    return {};
}

std::string MakeWebSocketAccept(const std::string& key)
{
    constexpr char kGuid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
    const std::string input = key + kGuid;

    BCRYPT_ALG_HANDLE algorithm = nullptr;
    BCRYPT_HASH_HANDLE hashHandle = nullptr;
    DWORD objectLength = 0;
    DWORD bytes = 0;
    std::vector<UCHAR> hashObject;
    std::array<UCHAR, 20> digest{};

    if (BCryptOpenAlgorithmProvider(&algorithm, BCRYPT_SHA1_ALGORITHM, nullptr, 0) < 0)
    {
        return {};
    }

    if (BCryptGetProperty(
            algorithm,
            BCRYPT_OBJECT_LENGTH,
            reinterpret_cast<PUCHAR>(&objectLength),
            sizeof(objectLength),
            &bytes,
            0) < 0)
    {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return {};
    }

    hashObject.resize(objectLength);
    if (BCryptCreateHash(
            algorithm,
            &hashHandle,
            hashObject.data(),
            static_cast<ULONG>(hashObject.size()),
            nullptr,
            0,
            0) < 0)
    {
        BCryptCloseAlgorithmProvider(algorithm, 0);
        return {};
    }

    const NTSTATUS hashStatus = BCryptHashData(
        hashHandle,
        reinterpret_cast<PUCHAR>(const_cast<char*>(input.data())),
        static_cast<ULONG>(input.size()),
        0);

    const NTSTATUS finishStatus = hashStatus < 0
        ? hashStatus
        : BCryptFinishHash(hashHandle, digest.data(), static_cast<ULONG>(digest.size()), 0);

    BCryptDestroyHash(hashHandle);
    BCryptCloseAlgorithmProvider(algorithm, 0);

    if (finishStatus < 0)
    {
        return {};
    }

    DWORD outputChars = 0;
    if (!CryptBinaryToStringA(
            digest.data(),
            static_cast<DWORD>(digest.size()),
            CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
            nullptr,
            &outputChars))
    {
        return {};
    }

    std::string encoded(outputChars, '\0');
    if (!CryptBinaryToStringA(
            digest.data(),
            static_cast<DWORD>(digest.size()),
            CRYPT_STRING_BASE64 | CRYPT_STRING_NOCRLF,
            encoded.data(),
            &outputChars))
    {
        return {};
    }

    while (!encoded.empty() && encoded.back() == '\0')
    {
        encoded.pop_back();
    }
    return encoded;
}

bool RecvExact(SOCKET socket, uint8_t* data, size_t size)
{
    size_t received = 0;
    while (received < size && gRunning.load())
    {
        const int chunk =
            RecvSome(
                socket,
                reinterpret_cast<char*>(data + received),
                static_cast<int>(
                    std::min<size_t>(
                        size - received,
                        static_cast<size_t>(INT_MAX))));
        if (chunk <= 0)
        {
            return false;
        }
        received += static_cast<size_t>(chunk);
    }
    return received == size;
}

bool SendWebSocketFrameBytes(
    SOCKET socket,
    uint8_t opcode,
    const uint8_t* payload,
    size_t payloadSize)
{
    std::vector<uint8_t> header;
    header.reserve(10);
    header.push_back(static_cast<uint8_t>(0x80 | (opcode & 0x0F)));

    if (payloadSize <= 125)
    {
        header.push_back(static_cast<uint8_t>(payloadSize));
    }
    else if (payloadSize <= 65535)
    {
        header.push_back(126);
        header.push_back(static_cast<uint8_t>((payloadSize >> 8) & 0xFF));
        header.push_back(static_cast<uint8_t>(payloadSize & 0xFF));
    }
    else
    {
        header.push_back(127);
        const uint64_t length = static_cast<uint64_t>(payloadSize);
        for (int shift = 56; shift >= 0; shift -= 8)
        {
            header.push_back(static_cast<uint8_t>((length >> shift) & 0xFF));
        }
    }

    if (!SendAll(socket, reinterpret_cast<const char*>(header.data()), header.size()))
    {
        return false;
    }

    return payloadSize == 0 ||
           SendAll(socket, reinterpret_cast<const char*>(payload), payloadSize);
}

bool SendWebSocketFrame(SOCKET socket, uint8_t opcode, const std::string& payload)
{
    return SendWebSocketFrameBytes(
        socket,
        opcode,
        reinterpret_cast<const uint8_t*>(payload.data()),
        payload.size());
}

void SendWhiteboardStateToBrowser()
{
    const SOCKET socket = gWebSocketClient.load();
    if (socket == INVALID_SOCKET)
    {
        return;
    }

    const std::string state =
        gWhiteboardActive.load(std::memory_order_relaxed)
            ? "state,whiteboard,1"
            : "state,whiteboard,0";

    std::lock_guard lock(gWebSocketSendMutex);
    SendWebSocketFrame(socket, 0x1, state);
}


void ReleaseCom(IUnknown*& value)
{
    if (value)
    {
        value->Release();
        value = nullptr;
    }
}

bool EnsureWicFactory()
{
    if (gWicFactory)
    {
        return true;
    }

    return SUCCEEDED(CoCreateInstance(
        CLSID_WICImagingFactory,
        nullptr,
        CLSCTX_INPROC_SERVER,
        IID_PPV_ARGS(&gWicFactory)));
}

HBITMAP CopyClipboardBitmap()
{
    if (!OpenClipboard(gMainWindow))
    {
        return nullptr;
    }

    HBITMAP result = nullptr;
    if (IsClipboardFormatAvailable(CF_BITMAP))
    {
        HBITMAP source = static_cast<HBITMAP>(GetClipboardData(CF_BITMAP));
        if (source)
        {
            result = static_cast<HBITMAP>(CopyImage(
                source,
                IMAGE_BITMAP,
                0,
                0,
                LR_CREATEDIBSECTION));
        }
    }

    CloseClipboard();
    return result;
}

bool EncodeBitmapToPng(HBITMAP bitmap, std::vector<uint8_t>& png)
{
    png.clear();
    if (!bitmap || !EnsureWicFactory())
    {
        return false;
    }

    IWICBitmap* source = nullptr;
    IWICBitmapEncoder* encoder = nullptr;
    IWICBitmapFrameEncode* frame = nullptr;
    IPropertyBag2* properties = nullptr;
    IStream* stream = nullptr;

    HRESULT hr = gWicFactory->CreateBitmapFromHBITMAP(
        bitmap,
        nullptr,
        WICBitmapIgnoreAlpha,
        &source);

    if (SUCCEEDED(hr))
    {
        hr = CreateStreamOnHGlobal(nullptr, TRUE, &stream);
    }
    if (SUCCEEDED(hr))
    {
        hr = gWicFactory->CreateEncoder(
            GUID_ContainerFormatPng,
            nullptr,
            &encoder);
    }
    if (SUCCEEDED(hr))
    {
        hr = encoder->Initialize(stream, WICBitmapEncoderNoCache);
    }
    if (SUCCEEDED(hr))
    {
        hr = encoder->CreateNewFrame(&frame, &properties);
    }
    if (SUCCEEDED(hr))
    {
        hr = frame->Initialize(properties);
    }

    UINT width = 0;
    UINT height = 0;
    if (SUCCEEDED(hr))
    {
        hr = source->GetSize(&width, &height);
    }
    if (SUCCEEDED(hr))
    {
        hr = frame->SetSize(width, height);
    }
    if (SUCCEEDED(hr))
    {
        WICPixelFormatGUID format = GUID_WICPixelFormat32bppBGRA;
        hr = frame->SetPixelFormat(&format);
    }
    if (SUCCEEDED(hr))
    {
        hr = frame->WriteSource(source, nullptr);
    }
    if (SUCCEEDED(hr))
    {
        hr = frame->Commit();
    }
    if (SUCCEEDED(hr))
    {
        hr = encoder->Commit();
    }

    if (SUCCEEDED(hr))
    {
        STATSTG stat{};
        hr = stream->Stat(&stat, STATFLAG_NONAME);
        if (SUCCEEDED(hr) && stat.cbSize.QuadPart > 0 &&
            stat.cbSize.QuadPart <= static_cast<ULONGLONG>(32 * 1024 * 1024))
        {
            png.resize(static_cast<size_t>(stat.cbSize.QuadPart));
            LARGE_INTEGER zero{};
            stream->Seek(zero, STREAM_SEEK_SET, nullptr);
            ULONG read = 0;
            hr = stream->Read(
                png.data(),
                static_cast<ULONG>(png.size()),
                &read);
            if (FAILED(hr) || read != png.size())
            {
                png.clear();
                hr = E_FAIL;
            }
        }
        else
        {
            hr = E_FAIL;
        }
    }

    if (properties) properties->Release();
    if (frame) frame->Release();
    if (encoder) encoder->Release();
    if (stream) stream->Release();
    if (source) source->Release();

    return SUCCEEDED(hr) && !png.empty();
}

bool CaptureClipboardPng(std::vector<uint8_t>& png)
{
    HBITMAP bitmap = CopyClipboardBitmap();
    if (!bitmap)
    {
        return false;
    }

    const bool encoded = EncodeBitmapToPng(bitmap, png);
    DeleteObject(bitmap);
    return encoded;
}

bool DecodePngToDibV5(const uint8_t* png, size_t pngSize, HGLOBAL& dibOut)
{
    dibOut = nullptr;
    if (!png || pngSize == 0 || pngSize > static_cast<size_t>(MAXDWORD) || !EnsureWicFactory())
    {
        return false;
    }

    IWICStream* stream = nullptr;
    IWICBitmapDecoder* decoder = nullptr;
    IWICBitmapFrameDecode* frame = nullptr;
    IWICFormatConverter* converter = nullptr;

    HRESULT hr = gWicFactory->CreateStream(&stream);
    if (SUCCEEDED(hr))
    {
        hr = stream->InitializeFromMemory(
            const_cast<BYTE*>(reinterpret_cast<const BYTE*>(png)),
            static_cast<DWORD>(pngSize));
    }
    if (SUCCEEDED(hr))
    {
        hr = gWicFactory->CreateDecoderFromStream(
            stream,
            nullptr,
            WICDecodeMetadataCacheOnLoad,
            &decoder);
    }
    if (SUCCEEDED(hr))
    {
        hr = decoder->GetFrame(0, &frame);
    }
    if (SUCCEEDED(hr))
    {
        hr = gWicFactory->CreateFormatConverter(&converter);
    }
    if (SUCCEEDED(hr))
    {
        hr = converter->Initialize(
            frame,
            GUID_WICPixelFormat32bppBGRA,
            WICBitmapDitherTypeNone,
            nullptr,
            0.0,
            WICBitmapPaletteTypeCustom);
    }

    UINT width = 0;
    UINT height = 0;
    if (SUCCEEDED(hr))
    {
        hr = converter->GetSize(&width, &height);
    }

    const size_t stride = static_cast<size_t>(width) * 4;
    const size_t pixelBytes = stride * static_cast<size_t>(height);
    const size_t totalBytes = sizeof(BITMAPV5HEADER) + pixelBytes;

    if (SUCCEEDED(hr) &&
        width > 0 &&
        height > 0 &&
        stride <= MAXDWORD &&
        pixelBytes <= MAXDWORD &&
        totalBytes <= static_cast<size_t>(MAXDWORD))
    {
        dibOut = GlobalAlloc(GMEM_MOVEABLE, totalBytes);
        if (!dibOut)
        {
            hr = E_OUTOFMEMORY;
        }
    }

    if (SUCCEEDED(hr))
    {
        auto* memory = static_cast<uint8_t*>(GlobalLock(dibOut));
        if (!memory)
        {
            hr = E_FAIL;
        }
        else
        {
            auto* header = reinterpret_cast<BITMAPV5HEADER*>(memory);
            ZeroMemory(header, sizeof(*header));
            header->bV5Size = sizeof(BITMAPV5HEADER);
            header->bV5Width = static_cast<LONG>(width);
            header->bV5Height = -static_cast<LONG>(height);
            header->bV5Planes = 1;
            header->bV5BitCount = 32;
            header->bV5Compression = BI_BITFIELDS;
            header->bV5SizeImage = static_cast<DWORD>(pixelBytes);
            header->bV5RedMask = 0x00FF0000;
            header->bV5GreenMask = 0x0000FF00;
            header->bV5BlueMask = 0x000000FF;
            header->bV5AlphaMask = 0xFF000000;
            header->bV5CSType = LCS_sRGB;

            BYTE* pixels = memory + sizeof(BITMAPV5HEADER);
            hr = converter->CopyPixels(
                nullptr,
                static_cast<UINT>(stride),
                static_cast<UINT>(pixelBytes),
                pixels);
            GlobalUnlock(dibOut);
        }
    }

    if (converter) converter->Release();
    if (frame) frame->Release();
    if (decoder) decoder->Release();
    if (stream) stream->Release();

    if (FAILED(hr) && dibOut)
    {
        GlobalFree(dibOut);
        dibOut = nullptr;
    }

    return SUCCEEDED(hr) && dibOut != nullptr;
}

HGLOBAL CopyBytesToGlobal(const uint8_t* bytes, size_t size)
{
    HGLOBAL memory = GlobalAlloc(GMEM_MOVEABLE, size);
    if (!memory)
    {
        return nullptr;
    }

    void* target = GlobalLock(memory);
    if (!target)
    {
        GlobalFree(memory);
        return nullptr;
    }

    std::memcpy(target, bytes, size);
    GlobalUnlock(memory);
    return memory;
}

bool PutPngOnClipboard(const uint8_t* png, size_t pngSize)
{
    HGLOBAL dib = nullptr;
    if (!DecodePngToDibV5(png, pngSize, dib))
    {
        return false;
    }

    HGLOBAL pngGlobal = CopyBytesToGlobal(png, pngSize);
    const UINT pngFormat = RegisterClipboardFormatW(L"PNG");

    if (!OpenClipboard(gMainWindow))
    {
        GlobalFree(dib);
        if (pngGlobal) GlobalFree(pngGlobal);
        return false;
    }

    gIgnoreNextClipboardUpdate.store(true, std::memory_order_relaxed);
    EmptyClipboard();

    const bool dibSet = SetClipboardData(CF_DIBV5, dib) != nullptr;
    if (!dibSet)
    {
        GlobalFree(dib);
    }

    bool pngSet = false;
    if (pngGlobal && pngFormat != 0)
    {
        pngSet = SetClipboardData(pngFormat, pngGlobal) != nullptr;
        if (!pngSet)
        {
            GlobalFree(pngGlobal);
        }
    }
    else if (pngGlobal)
    {
        GlobalFree(pngGlobal);
    }

    CloseClipboard();
    return dibSet || pngSet;
}

bool SendPngToIpad(const std::vector<uint8_t>& png)
{
    const SOCKET socket = gWebSocketClient.load();
    if (socket == INVALID_SOCKET || png.empty())
    {
        return false;
    }

    std::lock_guard lock(gWebSocketSendMutex);
    return SendWebSocketFrameBytes(
        socket,
        0x2,
        png.data(),
        png.size());
}

void SendClipboardToIpad()
{
    std::vector<uint8_t> png;
    if (!CaptureClipboardPng(png))
    {
        PostStatus(L"Clipboard does not contain a bitmap image.");
        return;
    }

    if (!SendPngToIpad(png))
    {
        PostStatus(L"Could not send clipboard image. Is the iPad connected?");
        return;
    }

    PostStatus(
        L"Sent clipboard image to iPad for markup (" +
        std::to_wstring(png.size() / 1024) +
        L" KB).");
}

HWND TopLevelWindowAtPoint(POINT point)
{
    const HWND window = WindowFromPoint(point);
    if (!window)
    {
        return nullptr;
    }

    const HWND root = GetAncestor(window, GA_ROOT);
    return root ? root : window;
}

bool PointBelongsToTarget(POINT point, HWND target)
{
    const HWND top = TopLevelWindowAtPoint(point);
    if (top == target)
    {
        return true;
    }

    // The persistent zone outline is click-through, but WindowFromPoint can still
    // report it on some systems. Treat its thin border as belonging to its owner.
    return top == gZoneOutlineWindow &&
           gZoneActive.load(std::memory_order_relaxed) &&
           gZoneTarget.load(std::memory_order_relaxed) == target;
}

bool CaptureTargetGeometry(HWND target, TargetGeometry& geometry)
{
    if (!target || !IsWindow(target) || !IsWindowVisible(target))
    {
        geometry.valid = false;
        return false;
    }

    RECT client{};
    if (!GetClientRect(target, &client))
    {
        geometry.valid = false;
        return false;
    }

    const int width = client.right - client.left;
    const int height = client.bottom - client.top;
    if (width <= 1 || height <= 1)
    {
        geometry.valid = false;
        return false;
    }

    POINT origin{0, 0};
    if (!ClientToScreen(target, &origin))
    {
        geometry.valid = false;
        return false;
    }

    geometry.target = target;
    geometry.origin = origin;
    geometry.width = width;
    geometry.height = height;
    geometry.valid = true;
    return true;
}

bool MapWithGeometry(
    const TargetGeometry& geometry,
    double normalizedX,
    double normalizedY,
    POINT& outPoint,
    HWND& outTarget)
{
    if (!geometry.valid || !geometry.target)
    {
        return false;
    }

    normalizedX = std::clamp(normalizedX, 0.0, 1.0);
    normalizedY = std::clamp(normalizedY, 0.0, 1.0);

    if (gZoneActive.load(std::memory_order_relaxed) &&
        gZoneTarget.load(std::memory_order_relaxed) == geometry.target)
    {
        const double zoneX = gZoneX.load(std::memory_order_relaxed);
        const double zoneY = gZoneY.load(std::memory_order_relaxed);
        const double zoneWidth = gZoneWidth.load(std::memory_order_relaxed);
        const double zoneHeight = gZoneHeight.load(std::memory_order_relaxed);
        normalizedX = zoneX + normalizedX * zoneWidth;
        normalizedY = zoneY + normalizedY * zoneHeight;
    }

    outPoint.x =
        geometry.origin.x +
        static_cast<LONG>(std::lround(normalizedX * static_cast<double>(geometry.width - 1)));
    outPoint.y =
        geometry.origin.y +
        static_cast<LONG>(std::lround(normalizedY * static_cast<double>(geometry.height - 1)));
    outTarget = geometry.target;
    return true;
}

bool GetCurrentMappingRect(RECT& rect)
{
    if (gScreenRectActive.load(std::memory_order_relaxed))
    {
        const int left =
            gScreenRectLeft.load(std::memory_order_relaxed);
        const int top =
            gScreenRectTop.load(std::memory_order_relaxed);
        const int width = std::max(
            1,
            gScreenRectWidth.load(std::memory_order_relaxed));
        const int height = std::max(
            1,
            gScreenRectHeight.load(std::memory_order_relaxed));

        rect = {
            left,
            top,
            left + width,
            top + height
        };
        return true;
    }

    TargetGeometry geometry;
    const HWND target = gTargetWindow.load();
    if (!CaptureTargetGeometry(target, geometry))
    {
        return false;
    }

    double leftN = 0.0;
    double topN = 0.0;
    double widthN = 1.0;
    double heightN = 1.0;

    if (gZoneActive.load(std::memory_order_relaxed) &&
        gZoneTarget.load(std::memory_order_relaxed) == target)
    {
        leftN = gZoneX.load(std::memory_order_relaxed);
        topN = gZoneY.load(std::memory_order_relaxed);
        widthN = gZoneWidth.load(std::memory_order_relaxed);
        heightN = gZoneHeight.load(std::memory_order_relaxed);
    }

    rect.left =
        geometry.origin.x +
        static_cast<LONG>(std::lround(
            leftN * static_cast<double>(geometry.width)));
    rect.top =
        geometry.origin.y +
        static_cast<LONG>(std::lround(
            topN * static_cast<double>(geometry.height)));
    rect.right =
        geometry.origin.x +
        static_cast<LONG>(std::lround(
            (leftN + widthN) * static_cast<double>(geometry.width)));
    rect.bottom =
        geometry.origin.y +
        static_cast<LONG>(std::lround(
            (topN + heightN) * static_cast<double>(geometry.height)));

    return rect.right > rect.left && rect.bottom > rect.top;
}

bool MapToScreenRect(
    double normalizedX,
    double normalizedY,
    POINT& outPoint,
    HWND& outTarget)
{
    if (!gScreenRectActive.load(std::memory_order_relaxed))
    {
        return false;
    }

    const int left = gScreenRectLeft.load(std::memory_order_relaxed);
    const int top = gScreenRectTop.load(std::memory_order_relaxed);
    const int width = std::max(
        1,
        gScreenRectWidth.load(std::memory_order_relaxed));
    const int height = std::max(
        1,
        gScreenRectHeight.load(std::memory_order_relaxed));

    normalizedX = std::clamp(normalizedX, 0.0, 1.0);
    normalizedY = std::clamp(normalizedY, 0.0, 1.0);

    outPoint.x =
        left +
        static_cast<LONG>(std::lround(
            normalizedX * static_cast<double>(width - 1)));
    outPoint.y =
        top +
        static_cast<LONG>(std::lround(
            normalizedY * static_cast<double>(height - 1)));
    outTarget = nullptr;
    return true;
}

bool MapToTarget(double normalizedX, double normalizedY, POINT& outPoint, HWND& outTarget)
{
    if (gScreenRectActive.load(std::memory_order_relaxed))
    {
        return MapToScreenRect(
            normalizedX,
            normalizedY,
            outPoint,
            outTarget);
    }

    TargetGeometry geometry;
    if (!CaptureTargetGeometry(gTargetWindow.load(), geometry))
    {
        return false;
    }

    return MapWithGeometry(geometry, normalizedX, normalizedY, outPoint, outTarget);
}

struct InputEvent
{
    char device = '\0';
    char phase = '\0';
    double x = 0.0;
    double y = 0.0;
    double pressure = 0.0;
    int tiltX = 0;
    int tiltY = 0;
    int pointerId = 0;
};

bool MapPenToTarget(
    const InputEvent& event,
    POINT& outPoint,
    HWND& outTarget)
{
    if (gScreenRectActive.load(std::memory_order_relaxed))
    {
        return MapToScreenRect(
            event.x,
            event.y,
            outPoint,
            outTarget);
    }

    const HWND selectedTarget = gTargetWindow.load();

    if (event.phase == 'd' ||
        !gPenDown ||
        !gPenTargetGeometry.valid ||
        gPenTargetGeometry.target != selectedTarget)
    {
        if (!CaptureTargetGeometry(selectedTarget, gPenTargetGeometry))
        {
            return false;
        }
    }

    return MapWithGeometry(
        gPenTargetGeometry,
        event.x,
        event.y,
        outPoint,
        outTarget);
}

bool ParseDouble(std::string_view text, double& value)
{
    const char* begin = text.data();
    const char* end = begin + text.size();
    const auto result = std::from_chars(begin, end, value);
    return result.ec == std::errc{} &&
           result.ptr == end &&
           std::isfinite(value);
}

bool ParseInt(std::string_view text, int& value)
{
    const char* begin = text.data();
    const char* end = begin + text.size();
    const auto result = std::from_chars(begin, end, value);
    return result.ec == std::errc{} && result.ptr == end;
}

bool ParseInputEvent(const std::string& message, InputEvent& event)
{
    std::array<std::string_view, 8> parts;
    const std::string_view view(message);
    size_t partIndex = 0;
    size_t start = 0;

    while (partIndex < parts.size())
    {
        const size_t comma = view.find(',', start);
        if (comma == std::string_view::npos)
        {
            parts[partIndex++] = view.substr(start);
            break;
        }

        parts[partIndex++] = view.substr(start, comma - start);
        start = comma + 1;
    }

    if (partIndex != parts.size() ||
        start < view.size() && view.find(',', start) != std::string_view::npos ||
        parts[0].size() != 1 ||
        parts[1].size() != 1)
    {
        return false;
    }

    event.device = parts[0][0];
    event.phase = parts[1][0];

    if (!ParseDouble(parts[2], event.x) ||
        !ParseDouble(parts[3], event.y) ||
        !ParseDouble(parts[4], event.pressure) ||
        !ParseInt(parts[5], event.tiltX) ||
        !ParseInt(parts[6], event.tiltY) ||
        !ParseInt(parts[7], event.pointerId))
    {
        return false;
    }

    event.x = std::clamp(event.x, 0.0, 1.0);
    event.y = std::clamp(event.y, 0.0, 1.0);
    event.pressure = std::clamp(event.pressure, 0.0, 1.0);
    event.tiltX = std::clamp(event.tiltX, -90, 90);
    event.tiltY = std::clamp(event.tiltY, -90, 90);
    return true;
}

void RefreshVirtualDesktopGeometry()
{
    gVirtualDesktopX.store(GetSystemMetrics(SM_XVIRTUALSCREEN), std::memory_order_relaxed);
    gVirtualDesktopY.store(GetSystemMetrics(SM_YVIRTUALSCREEN), std::memory_order_relaxed);
    gVirtualDesktopWidth.store(
        std::max(1, GetSystemMetrics(SM_CXVIRTUALSCREEN)),
        std::memory_order_relaxed);
    gVirtualDesktopHeight.store(
        std::max(1, GetSystemMetrics(SM_CYVIRTUALSCREEN)),
        std::memory_order_relaxed);
}

POINT ToSyntheticPenPoint(POINT screenPoint)
{
    const int virtualX = gVirtualDesktopX.load(std::memory_order_relaxed);
    const int virtualY = gVirtualDesktopY.load(std::memory_order_relaxed);
    const int virtualWidth = gVirtualDesktopWidth.load(std::memory_order_relaxed);
    const int virtualHeight = gVirtualDesktopHeight.load(std::memory_order_relaxed);

    POINT result{
        screenPoint.x - virtualX,
        screenPoint.y - virtualY
    };

    result.x = std::clamp<LONG>(result.x, 0, virtualWidth - 1);
    result.y = std::clamp<LONG>(result.y, 0, virtualHeight - 1);
    return result;
}

bool InjectPenPacket(
    POINT screenPoint,
    POINTER_FLAGS pointerFlags,
    UINT32 pressure,
    int tiltX,
    int tiltY)
{
    POINTER_TYPE_INFO info{};
    info.type = PT_PEN;

    POINTER_PEN_INFO& pen = info.penInfo;
    pen.pointerInfo.pointerType = PT_PEN;
    pen.pointerInfo.pointerId = 1;
    pen.pointerInfo.pointerFlags = pointerFlags;
    pen.pointerInfo.ptPixelLocation = ToSyntheticPenPoint(screenPoint);
    pen.penMask = static_cast<PEN_MASK>(PEN_MASK_PRESSURE | PEN_MASK_TILT_X | PEN_MASK_TILT_Y);
    pen.penFlags = PEN_FLAG_NONE;
    pen.pressure = std::min<UINT32>(pressure, 1024);
    pen.tiltX = std::clamp(tiltX, -90, 90);
    pen.tiltY = std::clamp(tiltY, -90, 90);

    if (!InjectSyntheticPointerInput(gPenDevice, &info, 1))
    {
        const DWORD error = GetLastError();
        PostStatus(L"Synthetic pen injection failed. GetLastError=" + std::to_wstring(error));
        return false;
    }

    gLastPenScreenPoint = screenPoint;
    gLastPenTiltX = pen.tiltX;
    gLastPenTiltY = pen.tiltY;
    return true;
}

void InjectPen(const InputEvent& event, POINT point, HWND target)
{
    if (!gPenDevice)
    {
        return;
    }

    std::lock_guard lock(gInputMutex);

    if (event.phase == 'h')
    {
        return;
    }

    if (event.phase == 'd')
    {
        // Treat every fresh browser DOWN as authoritative. If an UP/CANCEL was
        // lost anywhere in Safari/network delivery, release the old synthetic
        // stroke before beginning the new one. Injecting DOWN while already down
        // can cause Windows Ink to drop the entire next stroke.
        if (gPenDown)
        {
            InjectPenPacket(
                gLastPenScreenPoint,
                POINTER_FLAG_UP | POINTER_FLAG_INRANGE,
                0,
                gLastPenTiltX,
                gLastPenTiltY);
            InjectPenPacket(
                gLastPenScreenPoint,
                POINTER_FLAG_UP,
                0,
                gLastPenTiltX,
                gLastPenTiltY);
            gPenDown = false;
            gPenInRange = false;
        }

        if (!gScreenRectActive.load(std::memory_order_relaxed))
        {
            ActivateTargetWindow(target);
            if (!PointBelongsToTarget(point, target))
            {
                PostStatus(L"Input blocked: selected target is not visible at the mapped pen position.");
                return;
            }
        }

        // Windows Ink behaves more reliably when a pen enters range before first contact.
        if (!gPenInRange)
        {
            if (!InjectPenPacket(
                    point,
                    POINTER_FLAG_UPDATE | POINTER_FLAG_INRANGE,
                    0,
                    event.tiltX,
                    event.tiltY))
            {
                return;
            }
            gPenInRange = true;
        }

        UINT32 pressure = static_cast<UINT32>(std::lround(event.pressure * 1024.0));
        if (pressure == 0)
        {
            pressure = 1;
        }

        if (InjectPenPacket(
                point,
                POINTER_FLAG_DOWN | POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT,
                pressure,
                event.tiltX,
                event.tiltY))
        {
            gPenDown = true;
            gPenInRange = true;
        }
        return;
    }

    if (event.phase == 'm')
    {
        if (!gPenDown)
        {
            return;
        }

        // Once a validated DOWN starts, keep ownership of the stroke until UP/CANCEL.
        // Re-hit-testing every move can break continuous strokes on transient overlays.
        UINT32 pressure = static_cast<UINT32>(std::lround(event.pressure * 1024.0));
        if (pressure == 0)
        {
            pressure = 1;
        }

        InjectPenPacket(
            point,
            POINTER_FLAG_UPDATE | POINTER_FLAG_INRANGE | POINTER_FLAG_INCONTACT,
            pressure,
            event.tiltX,
            event.tiltY);
        return;
    }

    if (event.phase == 'u' || event.phase == 'c')
    {
        if (!gPenDown)
        {
            return;
        }

        // Lift while still in range, then leave range completely.
        const bool lifted = InjectPenPacket(
            point,
            POINTER_FLAG_UP | POINTER_FLAG_INRANGE,
            0,
            event.tiltX,
            event.tiltY);

        if (lifted)
        {
            InjectPenPacket(
                point,
                POINTER_FLAG_UP,
                0,
                event.tiltX,
                event.tiltY);
        }

        gPenDown = false;
        gPenInRange = false;
        gPenTargetGeometry.valid = false;
    }
}


void SendKeyboardShortcut(WORD key)
{
    const bool screenRectMode =
        gScreenRectActive.load(std::memory_order_relaxed);
    const HWND target = gTargetWindow.load();

    if (!screenRectMode &&
        (!target || !IsWindow(target)))
    {
        return;
    }

    std::lock_guard lock(gInputMutex);

    // Window/zone mode deliberately targets the selected app. Screen Rect mode
    // is desktop-global, so leave focus alone and send the shortcut to whatever
    // window the user currently has active.
    if (!screenRectMode)
    {
        ActivateTargetWindow(target);
    }

    std::array<INPUT, 4> inputs{};
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = VK_CONTROL;
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = key;
    inputs[2].type = INPUT_KEYBOARD;
    inputs[2].ki.wVk = key;
    inputs[2].ki.dwFlags = KEYEVENTF_KEYUP;
    inputs[3].type = INPUT_KEYBOARD;
    inputs[3].ki.wVk = VK_CONTROL;
    inputs[3].ki.dwFlags = KEYEVENTF_KEYUP;
    SendInput(static_cast<UINT>(inputs.size()), inputs.data(), sizeof(INPUT));
}

void SendCtrlWheelZoom(int steps, double normalizedX, double normalizedY)
{
    if (steps == 0)
    {
        return;
    }

    POINT point{};
    HWND target = nullptr;
    if (!MapToTarget(
            normalizedX,
            normalizedY,
            point,
            target))
    {
        return;
    }

    std::lock_guard lock(gInputMutex);

    if (!gScreenRectActive.load(std::memory_order_relaxed))
    {
        ActivateTargetWindow(target);
    }

    SetCursorPos(point.x, point.y);

    INPUT ctrlDown{};
    ctrlDown.type = INPUT_KEYBOARD;
    ctrlDown.ki.wVk = VK_CONTROL;

    INPUT wheel{};
    wheel.type = INPUT_MOUSE;
    wheel.mi.dwFlags = MOUSEEVENTF_WHEEL;
    wheel.mi.mouseData = static_cast<DWORD>(
        static_cast<LONG>(std::clamp(steps, -8, 8) * WHEEL_DELTA));

    INPUT ctrlUp{};
    ctrlUp.type = INPUT_KEYBOARD;
    ctrlUp.ki.wVk = VK_CONTROL;
    ctrlUp.ki.dwFlags = KEYEVENTF_KEYUP;

    std::array<INPUT, 3> inputs{
        ctrlDown,
        wheel,
        ctrlUp};
    SendInput(
        static_cast<UINT>(inputs.size()),
        inputs.data(),
        sizeof(INPUT));
}

bool ProcessCommandMessage(std::string_view message)
{
    constexpr std::string_view zoomPrefix = "cmd,zoom,";
    if (message.starts_with(zoomPrefix))
    {
        const std::string_view payload =
            message.substr(zoomPrefix.size());
        const size_t firstComma = payload.find(',');
        const size_t secondComma =
            firstComma == std::string_view::npos
                ? std::string_view::npos
                : payload.find(',', firstComma + 1);

        int steps = 0;
        double x = 0.5;
        double y = 0.5;
        if (firstComma != std::string_view::npos &&
            secondComma != std::string_view::npos &&
            ParseInt(payload.substr(0, firstComma), steps) &&
            ParseDouble(
                payload.substr(
                    firstComma + 1,
                    secondComma - firstComma - 1),
                x) &&
            ParseDouble(payload.substr(secondComma + 1), y))
        {
            SendCtrlWheelZoom(
                steps,
                std::clamp(x, 0.0, 1.0),
                std::clamp(y, 0.0, 1.0));
        }
        return true;
    }

    constexpr std::string_view colorPrefix =
        "cmd,whiteboard,color,";
    if (message.starts_with(colorPrefix))
    {
        const std::string_view payload =
            message.substr(colorPrefix.size());
        const size_t firstComma = payload.find(',');
        const size_t secondComma =
            firstComma == std::string_view::npos
                ? std::string_view::npos
                : payload.find(',', firstComma + 1);

        int red = 255;
        int green = 70;
        int blue = 60;
        if (firstComma != std::string_view::npos &&
            secondComma != std::string_view::npos &&
            ParseInt(payload.substr(0, firstComma), red) &&
            ParseInt(
                payload.substr(
                    firstComma + 1,
                    secondComma - firstComma - 1),
                green) &&
            ParseInt(payload.substr(secondComma + 1), blue))
        {
            gWhiteboardBrushColor.store(
                RGB(
                    std::clamp(red, 0, 255),
                    std::clamp(green, 0, 255),
                    std::clamp(blue, 0, 255)),
                std::memory_order_relaxed);
        }
        return true;
    }

    constexpr std::string_view sizePrefix =
        "cmd,whiteboard,size,";
    if (message.starts_with(sizePrefix))
    {
        int size = 7;
        if (ParseInt(message.substr(sizePrefix.size()), size))
        {
            gWhiteboardBrushSize.store(
                std::clamp(size, 1, 32),
                std::memory_order_relaxed);
        }
        return true;
    }

    if (message == "cmd,undo")
    {
        SendKeyboardShortcut('Z');
        PostStatus(L"Undo");
        return true;
    }

    if (message == "cmd,redo")
    {
        SendKeyboardShortcut('Y');
        PostStatus(L"Redo");
        return true;
    }

    if (message == "cmd,whiteboard,on")
    {
        SetWhiteboardActive(true);
        return true;
    }
    if (message == "cmd,whiteboard,off")
    {
        SetWhiteboardActive(false);
        return true;
    }
    if (message == "cmd,whiteboard,toggle")
    {
        SetWhiteboardActive(
            !gWhiteboardActive.load(std::memory_order_relaxed));
        return true;
    }
    if (message == "cmd,whiteboard,clear")
    {
        ClearWhiteboardInk();
        return true;
    }
    if (message == "cmd,whiteboard,clip")
    {
        ClipWhiteboardToClipboard();
        return true;
    }

    return false;
}

void SendMouseButton(DWORD flags)
{
    INPUT input{};
    input.type = INPUT_MOUSE;
    input.mi.dwFlags = flags;
    SendInput(1, &input, sizeof(input));
}

void InjectTouchMouse(const InputEvent& event, POINT point, HWND target)
{
    std::lock_guard lock(gInputMutex);

    if (event.phase == 'd')
    {
        if (gTouchDown)
        {
            return;
        }
        if (!gScreenRectActive.load(std::memory_order_relaxed))
        {
            ActivateTargetWindow(target);
            if (!PointBelongsToTarget(point, target))
            {
                PostStatus(L"Input blocked: selected target is not visible at the mapped touch position.");
                return;
            }
        }

        gTouchDown = true;
        gActiveTouchPointer = event.pointerId;
        SetCursorPos(point.x, point.y);
        SendMouseButton(MOUSEEVENTF_LEFTDOWN);
    }
    else if (event.phase == 'm')
    {
        if (!gTouchDown || event.pointerId != gActiveTouchPointer)
        {
            return;
        }
        SetCursorPos(point.x, point.y);
    }
    else if (event.phase == 'u' || event.phase == 'c')
    {
        if (!gTouchDown || event.pointerId != gActiveTouchPointer)
        {
            return;
        }
        SetCursorPos(point.x, point.y);
        SendMouseButton(MOUSEEVENTF_LEFTUP);
        gTouchDown = false;
        gActiveTouchPointer = -1;
    }
}

GestureTouchState* FindGestureTouch(int id)
{
    for (GestureTouchState& touch : gGestureTouches)
    {
        if (touch.active && touch.id == id)
        {
            return &touch;
        }
    }
    return nullptr;
}

GestureTouchState* AllocateGestureTouch(int id)
{
    if (GestureTouchState* existing = FindGestureTouch(id))
    {
        return existing;
    }

    bool hasActiveTouch = false;
    for (const GestureTouchState& touch : gGestureTouches)
    {
        if (touch.active)
        {
            hasActiveTouch = true;
            break;
        }
    }

    for (size_t index = 0; index < gGestureTouches.size(); ++index)
    {
        GestureTouchState& touch = gGestureTouches[index];
        if (!touch.active)
        {
            touch.id = id;
            touch.injectedId = static_cast<UINT32>(index + 1);
            touch.active = true;
            touch.primary = !hasActiveTouch;
            return &touch;
        }
    }
    return nullptr;
}

void ClearGestureTouches()
{
    for (GestureTouchState& touch : gGestureTouches)
    {
        touch = {};
        touch.id = -1;
    }
}

void FillTouchInfo(
    POINTER_TOUCH_INFO& info,
    const GestureTouchState& touch,
    POINTER_FLAGS flags)
{
    ZeroMemory(&info, sizeof(info));
    info.pointerInfo.pointerType = PT_TOUCH;
    info.pointerInfo.pointerId = touch.injectedId;
    info.pointerInfo.pointerFlags =
        touch.primary
            ? static_cast<POINTER_FLAGS>(flags | POINTER_FLAG_PRIMARY)
            : flags;
    info.pointerInfo.ptPixelLocation = touch.point;
    info.touchFlags = TOUCH_FLAG_NONE;
    info.touchMask = static_cast<TOUCH_MASK>(
        TOUCH_MASK_CONTACTAREA |
        TOUCH_MASK_ORIENTATION |
        TOUCH_MASK_PRESSURE);
    info.rcContact.left = touch.point.x - 4;
    info.rcContact.top = touch.point.y - 4;
    info.rcContact.right = touch.point.x + 4;
    info.rcContact.bottom = touch.point.y + 4;
    info.orientation = 90;
    info.pressure = 512;
}

void ReleaseGestureTouches()
{
    if (!gTouchInjectionReady)
    {
        ClearGestureTouches();
        return;
    }

    std::array<POINTER_TOUCH_INFO, 4> infos{};
    UINT32 count = 0;

    for (const GestureTouchState& touch : gGestureTouches)
    {
        if (!touch.active)
        {
            continue;
        }

        FillTouchInfo(
            infos[count++],
            touch,
            POINTER_FLAG_UP);
    }

    if (count > 0 &&
        !InjectTouchInput(count, infos.data()))
    {
        const DWORD error = GetLastError();
        PostStatus(
            L"Synthetic touch release failed. GetLastError=" +
            std::to_wstring(error));
    }

    ClearGestureTouches();
}

void InjectGestureTouch(const InputEvent& event, POINT point, HWND target)
{
    if (!gTouchInjectionReady)
    {
        return;
    }

    std::lock_guard lock(gInputMutex);

    if (event.phase == 'd' &&
        !gScreenRectActive.load(std::memory_order_relaxed))
    {
        ActivateTargetWindow(target);
    }

    GestureTouchState* changed = nullptr;
    if (event.phase == 'd')
    {
        changed = AllocateGestureTouch(event.pointerId);
        if (!changed)
        {
            return;
        }
        changed->point = point;
    }
    else
    {
        changed = FindGestureTouch(event.pointerId);
        if (!changed)
        {
            return;
        }
        changed->point = point;
    }

    std::array<POINTER_TOUCH_INFO, 4> infos{};
    UINT32 count = 0;

    for (const GestureTouchState& touch : gGestureTouches)
    {
        if (!touch.active)
        {
            continue;
        }

        POINTER_FLAGS flags =
            POINTER_FLAG_UPDATE |
            POINTER_FLAG_INRANGE |
            POINTER_FLAG_INCONTACT;

        if (touch.id == event.pointerId)
        {
            if (event.phase == 'd')
            {
                flags =
                    POINTER_FLAG_DOWN |
                    POINTER_FLAG_INRANGE |
                    POINTER_FLAG_INCONTACT;
            }
            else if (event.phase == 'u' || event.phase == 'c')
            {
                flags = POINTER_FLAG_UP;
            }
        }

        FillTouchInfo(infos[count++], touch, flags);
    }

    if (count > 0 &&
        !InjectTouchInput(count, infos.data()))
    {
        const DWORD error = GetLastError();
        PostStatus(
            L"Synthetic touch gesture failed. GetLastError=" +
            std::to_wstring(error));
    }

    if (event.phase == 'u' || event.phase == 'c')
    {
        changed->active = false;
        changed->id = -1;
    }
}

void DrawWhiteboardInput(const InputEvent& event)
{
    if (!gWhiteboardActive.load(std::memory_order_relaxed))
    {
        return;
    }

    std::lock_guard lock(gWhiteboardMutex);

    const int baseSize =
        gWhiteboardBrushSize.load(std::memory_order_relaxed);
    const int width =
        std::clamp(
            static_cast<int>(std::lround(
                static_cast<double>(baseSize) *
                (0.65 + event.pressure * 0.7))),
            1,
            48);
    const COLORREF color =
        gWhiteboardBrushColor.load(std::memory_order_relaxed);

    if (event.phase == 'd')
    {
        gWhiteboardStrokeActive = true;
        gWhiteboardLastX = event.x;
        gWhiteboardLastY = event.y;

        // Keep very short Pencil taps/strokes visible.
        gWhiteboardSegments.push_back({
            event.x,
            event.y,
            event.x,
            event.y,
            width,
            color});
    }
    else if (event.phase == 'm' && gWhiteboardStrokeActive)
    {
        gWhiteboardSegments.push_back({
            gWhiteboardLastX,
            gWhiteboardLastY,
            event.x,
            event.y,
            width,
            color});

        gWhiteboardLastX = event.x;
        gWhiteboardLastY = event.y;
    }
    else if ((event.phase == 'u' || event.phase == 'c') &&
             gWhiteboardStrokeActive)
    {
        if (event.phase == 'u' &&
            (event.x != gWhiteboardLastX ||
             event.y != gWhiteboardLastY))
        {
            gWhiteboardSegments.push_back({
                gWhiteboardLastX,
                gWhiteboardLastY,
                event.x,
                event.y,
                width,
            color});
        }

        gWhiteboardStrokeActive = false;
    }

    if (gWhiteboardWindow)
    {
        RedrawWindow(
            gWhiteboardWindow,
            nullptr,
            nullptr,
            RDW_INVALIDATE | RDW_UPDATENOW);
    }
}

void ProcessInputMessage(const std::string& message)
{
    if (ProcessCommandMessage(message))
    {
        return;
    }

    InputEvent event;
    if (!ParseInputEvent(message, event))
    {
        return;
    }

    if (gWhiteboardActive.load(std::memory_order_relaxed) &&
        event.device == 'p')
    {
        DrawWhiteboardInput(event);
        return;
    }

    if (gWhiteboardActive.load(std::memory_order_relaxed) &&
        event.device == 'g')
    {
        return;
    }

    POINT point{};
    HWND target = nullptr;

    if (event.device == 'p')
    {
        if (!MapPenToTarget(event, point, target))
        {
            return;
        }
        InjectPen(event, point, target);
    }
    else if (event.device == 't')
    {
        if (!MapToTarget(event.x, event.y, point, target))
        {
            return;
        }
        InjectTouchMouse(event, point, target);
    }
    else if (event.device == 'g')
    {
        if (!MapToTarget(event.x, event.y, point, target))
        {
            return;
        }
        InjectGestureTouch(event, point, target);
    }
}

void ReleaseActiveInputState()
{
    std::lock_guard lock(gInputMutex);

    if (gTouchDown)
    {
        SendMouseButton(MOUSEEVENTF_LEFTUP);
        gTouchDown = false;
        gActiveTouchPointer = -1;
    }

    ReleaseGestureTouches();

    if (gPenDown && gPenDevice)
    {
        InjectPenPacket(
            gLastPenScreenPoint,
            POINTER_FLAG_UP | POINTER_FLAG_INRANGE,
            0,
            gLastPenTiltX,
            gLastPenTiltY);
        InjectPenPacket(
            gLastPenScreenPoint,
            POINTER_FLAG_UP,
            0,
            gLastPenTiltX,
            gLastPenTiltY);
    }

    gPenDown = false;
    gPenInRange = false;
    gPenTargetGeometry.valid = false;
}

void WebSocketLoop(SOCKET socket)
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

    const SOCKET previous = gWebSocketClient.exchange(socket);
    if (previous != INVALID_SOCKET && previous != socket)
    {
        shutdown(previous, SD_BOTH);
        ReleaseActiveInputState();
        PostStatus(L"New browser connection took over the active session.");
    }
    else
    {
        PostStatus(L"iPad/browser connected. Input is live.");
    }

    SendWhiteboardStateToBrowser();

    while (gRunning.load() && gWebSocketClient.load() == socket)
    {
        uint8_t header[2]{};
        if (!RecvExact(socket, header, sizeof(header)))
        {
            break;
        }

        const uint8_t opcode = static_cast<uint8_t>(header[0] & 0x0F);
        const bool masked = (header[1] & 0x80) != 0;
        uint64_t length = header[1] & 0x7F;

        if (length == 126)
        {
            uint8_t extended[2]{};
            if (!RecvExact(socket, extended, sizeof(extended)))
            {
                break;
            }
            length = (static_cast<uint64_t>(extended[0]) << 8) | extended[1];
        }
        else if (length == 127)
        {
            uint8_t extended[8]{};
            if (!RecvExact(socket, extended, sizeof(extended)))
            {
                break;
            }

            length = 0;
            for (uint8_t byte : extended)
            {
                length = (length << 8) | byte;
            }
        }

        if (length > 32ull * 1024ull * 1024ull)
        {
            break;
        }

        uint8_t mask[4]{};
        if (masked && !RecvExact(socket, mask, sizeof(mask)))
        {
            break;
        }

        std::string payload(static_cast<size_t>(length), '\0');
        if (length > 0 && !RecvExact(socket, reinterpret_cast<uint8_t*>(payload.data()), static_cast<size_t>(length)))
        {
            break;
        }

        if (masked)
        {
            for (size_t i = 0; i < payload.size(); ++i)
            {
                payload[i] = static_cast<char>(
                    static_cast<uint8_t>(payload[i]) ^ mask[i % 4]);
            }
        }

        if (gWebSocketClient.load() != socket)
        {
            break;
        }

        if (opcode == 0x8)
        {
            {
                std::lock_guard lock(gWebSocketSendMutex);
                SendWebSocketFrame(socket, 0x8, {});
            }
            break;
        }
        if (opcode == 0x9)
        {
            {
                std::lock_guard lock(gWebSocketSendMutex);
                if (!SendWebSocketFrame(socket, 0xA, payload))
                {
                    break;
                }
            }
            continue;
        }
        if (opcode == 0x1)
        {
            ProcessInputMessage(payload);
        }
        else if (opcode == 0x2)
        {
            if (PutPngOnClipboard(
                    reinterpret_cast<const uint8_t*>(payload.data()),
                    payload.size()))
            {
                PostStatus(L"Marked-up image copied back to the Windows clipboard.");
            }
            else
            {
                PostStatus(L"Could not decode the marked-up PNG from iPad.");
            }
        }
    }

    SOCKET expected = socket;
    if (gWebSocketClient.compare_exchange_strong(
            expected,
            INVALID_SOCKET))
    {
        ReleaseActiveInputState();
        PostStatus(L"Client disconnected. Waiting for iPad/browser...");
    }
}

bool ReadHttpRequest(SOCKET socket, std::string& request)
{
    request.clear();
    std::array<char, 2048> buffer{};

    while (request.find("\r\n\r\n") == std::string::npos)
    {
        const int received =
            RecvSome(
                socket,
                buffer.data(),
                static_cast<int>(buffer.size()));
        if (received <= 0)
        {
            return false;
        }

        request.append(buffer.data(), static_cast<size_t>(received));
        if (request.size() > 16 * 1024)
        {
            return false;
        }
    }

    return true;
}

void SendHttp(SOCKET socket, std::string_view status, std::string_view contentType, std::string_view body)
{
    std::ostringstream headers;
    headers << "HTTP/1.1 " << status << "\r\n"
            << "Content-Type: " << contentType << "\r\n"
            << "Content-Length: " << body.size() << "\r\n"
            << "Cache-Control: no-store\r\n"
            << "Connection: close\r\n\r\n";

    const std::string headerText = headers.str();
    SendAll(socket, headerText);
    SendAll(socket, body.data(), body.size());
}

void HandleClient(SOCKET socket)
{
    RegisterOpenClientSocket(socket);

    auto tls =
        std::make_shared<TlsConnection>();
    if (!AcceptTlsConnection(socket, tls))
    {
        UnregisterOpenClientSocket(socket);
        shutdown(socket, SD_BOTH);
        closesocket(socket);
        return;
    }
    RegisterTlsConnection(socket, tls);

    std::string request;
    if (!ReadHttpRequest(socket, request))
    {
        UnregisterTlsConnection(socket);
        UnregisterOpenClientSocket(socket);
        closesocket(socket);
        return;
    }

    std::istringstream firstLineStream(request);
    std::string method;
    std::string path;
    std::string version;
    firstLineStream >> method >> path >> version;

    const std::string upgrade = ToLower(GetHeaderValue(request, "Upgrade"));
    if (method == "GET" && path == "/ws" && upgrade == "websocket")
    {
        const std::string key = GetHeaderValue(request, "Sec-WebSocket-Key");
        const std::string accept = MakeWebSocketAccept(key);
        if (accept.empty())
        {
            SendHttp(socket, "400 Bad Request", "text/plain; charset=utf-8", "Bad WebSocket handshake");
        }
        else
        {
            std::ostringstream response;
            response << "HTTP/1.1 101 Switching Protocols\r\n"
                     << "Upgrade: websocket\r\n"
                     << "Connection: Upgrade\r\n"
                     << "Sec-WebSocket-Accept: " << accept << "\r\n\r\n";
            if (SendAll(socket, response.str()))
            {
                WebSocketLoop(socket);
            }
        }
    }
    else if (method == "GET" && (path == "/" || path == "/index.html"))
    {
        SendHttp(socket, "200 OK", "text/html; charset=utf-8", kWebUi);
    }
    else if (method == "GET" && path == "/health")
    {
        SendHttp(socket, "200 OK", "text/plain; charset=utf-8", "ok");
    }
    else
    {
        SendHttp(socket, "404 Not Found", "text/plain; charset=utf-8", "Not found");
    }

    UnregisterTlsConnection(socket);
    UnregisterOpenClientSocket(socket);
    shutdown(socket, SD_BOTH);
    closesocket(socket);
}

std::string ReadFileBytes(const std::string& path)
{
    std::ifstream file(
        path,
        std::ios::binary);
    if (!file)
    {
        return {};
    }

    return std::string(
        std::istreambuf_iterator<char>(file),
        std::istreambuf_iterator<char>());
}

void SendRawHttp(
    SOCKET socket,
    std::string_view status,
    std::string_view contentType,
    std::string_view body,
    std::string_view extraHeaders = {})
{
    std::ostringstream headers;
    headers
        << "HTTP/1.1 " << status << "\r\n"
        << "Content-Type: " << contentType << "\r\n"
        << "Content-Length: " << body.size() << "\r\n"
        << "Cache-Control: no-store\r\n";
    if (!extraHeaders.empty())
    {
        headers << extraHeaders;
    }
    headers << "Connection: close\r\n\r\n";

    const std::string headerText =
        headers.str();
    RawSendAll(
        socket,
        headerText.data(),
        headerText.size());
    RawSendAll(
        socket,
        body.data(),
        body.size());
}

bool ReadRawHttpRequest(
    SOCKET socket,
    std::string& request)
{
    request.clear();
    std::array<char, 2048> buffer{};

    while (request.find("\r\n\r\n") ==
           std::string::npos)
    {
        const int received =
            RawRecvSome(
                socket,
                buffer.data(),
                static_cast<int>(buffer.size()));
        if (received <= 0)
        {
            return false;
        }

        request.append(
            buffer.data(),
            static_cast<size_t>(received));
        if (request.size() > 16 * 1024)
        {
            return false;
        }
    }

    return true;
}

void HandleBootstrapClient(
    SOCKET socket,
    const std::string& ip)
{
    RegisterOpenClientSocket(socket);

    std::string request;
    if (!ReadRawHttpRequest(socket, request))
    {
        UnregisterOpenClientSocket(socket);
        closesocket(socket);
        return;
    }

    std::istringstream firstLine(request);
    std::string method;
    std::string path;
    std::string version;
    firstLine >> method >> path >> version;

    if (method == "GET" &&
        path == "/PencilBridge-CA.mobileconfig")
    {
        const std::string profile =
            ReadFileBytes(
                "Saved/PencilBridge/TLS/"
                "PencilBridge-CA.mobileconfig");

        if (profile.empty())
        {
            SendRawHttp(
                socket,
                "404 Not Found",
                "text/plain; charset=utf-8",
                "PencilBridge CA profile is missing. "
                "Run Run.ps1 again.");
        }
        else
        {
            SendRawHttp(
                socket,
                "200 OK",
                "application/x-apple-aspen-config",
                profile,
                "Content-Disposition: attachment; "
                "filename=\"PencilBridge-CA.mobileconfig\"\r\n");
        }
    }
    else if (method == "GET" &&
             path == "/PencilBridge-CA.cer")
    {
        const std::string certificate =
            ReadFileBytes(
                "Saved/PencilBridge/TLS/"
                "PencilBridge-CA.cer");

        if (certificate.empty())
        {
            SendRawHttp(
                socket,
                "404 Not Found",
                "text/plain; charset=utf-8",
                "PencilBridge CA certificate is missing.");
        }
        else
        {
            SendRawHttp(
                socket,
                "200 OK",
                "application/x-x509-ca-cert",
                certificate,
                "Content-Disposition: attachment; "
                "filename=\"PencilBridge-CA.cer\"\r\n");
        }
    }
    else if (method == "GET" &&
             (path == "/" ||
              path == "/index.html"))
    {
        const std::string secureUrl =
            "https://" + ip + ":8765";

        const std::string page =
            "<!doctype html>"
            "<meta name=\"viewport\" "
            "content=\"width=device-width,initial-scale=1\">"
            "<meta name=\"color-scheme\" content=\"dark\">"
            "<title>PencilBridge HTTPS Setup</title>"
            "<style>"
            "body{font:17px -apple-system,BlinkMacSystemFont,"
            "sans-serif;background:#111318;color:#e8ebef;"
            "max-width:720px;margin:0 auto;padding:28px 20px;"
            "line-height:1.45}"
            "a{display:block;background:#2a7fff;color:white;"
            "padding:14px 16px;border-radius:10px;"
            "text-decoration:none;margin:14px 0}"
            "code{background:#222730;padding:2px 5px;"
            "border-radius:4px}"
            "li{margin:12px 0}"
            "</style>"
            "<h1>PencilBridge HTTPS setup</h1>"
            "<p>This is a one-time setup for this PencilBridge CA.</p>"
            "<ol>"
            "<li><a href=\"/PencilBridge-CA.mobileconfig\">"
            "Download PencilBridge CA profile</a>"
            "<small>If Safari does not offer the profile, "
            "<a href=\"/PencilBridge-CA.cer\">download the raw CA certificate</a>.</small>"
            "</li>"
            "<li>Open <b>Settings → General → "
            "VPN &amp; Device Management</b>, select "
            "<b>PencilBridge Local HTTPS</b>, and install it.</li>"
            "<li>Open <b>Settings → General → About → "
            "Certificate Trust Settings</b> and enable "
            "<b>full trust</b> for <b>PencilBridge Local CA</b>.</li>"
            "<li><a href=\"" +
            secureUrl +
            "\">Open secure PencilBridge</a></li>"
            "</ol>"
            "<p>After this, use <code>" +
            secureUrl +
            "</code>. Safari will treat PencilBridge as a "
            "secure context and the real Screen Wake Lock API "
            "can be used.</p>";

        SendRawHttp(
            socket,
            "200 OK",
            "text/html; charset=utf-8",
            page);
    }
    else
    {
        SendRawHttp(
            socket,
            "404 Not Found",
            "text/plain; charset=utf-8",
            "Not found");
    }

    UnregisterOpenClientSocket(socket);
    shutdown(socket, SD_BOTH);
    closesocket(socket);
}

void BootstrapServerMain()
{
    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        return;
    }

    SOCKET listenSocket =
        socket(
            AF_INET,
            SOCK_STREAM,
            IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET)
    {
        WSACleanup();
        return;
    }

    gBootstrapListenSocket.store(listenSocket);

    BOOL reuse = TRUE;
    setsockopt(
        listenSocket,
        SOL_SOCKET,
        SO_REUSEADDR,
        reinterpret_cast<const char*>(&reuse),
        sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr =
        htonl(INADDR_ANY);
    address.sin_port = htons(kBootstrapPort);

    if (bind(
            listenSocket,
            reinterpret_cast<sockaddr*>(&address),
            sizeof(address)) == SOCKET_ERROR ||
        listen(listenSocket, 4) == SOCKET_ERROR)
    {
        closesocket(listenSocket);
        gBootstrapListenSocket.store(
            INVALID_SOCKET);
        WSACleanup();
        return;
    }

    const std::string ip = GetConfiguredServerIPv4();

    while (gRunning.load())
    {
        SOCKET client =
            accept(
                listenSocket,
                nullptr,
                nullptr);
        if (client == INVALID_SOCKET)
        {
            if (!gRunning.load())
            {
                break;
            }
            continue;
        }

        HandleBootstrapClient(client, ip);
    }

    const SOCKET current =
        gBootstrapListenSocket.exchange(
            INVALID_SOCKET);
    if (current != INVALID_SOCKET)
    {
        closesocket(current);
    }

    WSACleanup();
}

void ServerMain()
{
    SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

    WSADATA wsa{};
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0)
    {
        PostStatus(L"Winsock startup failed.");
        return;
    }

    if (!InitializeTlsCredentials())
    {
        PostStatus(
            L"HTTPS certificate missing. Run PencilBridge with Run.ps1.");
        WSACleanup();
        return;
    }

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET)
    {
        PostStatus(L"Could not create listening socket.");
        ShutdownTlsCredentials();
        WSACleanup();
        return;
    }
    gListenSocket.store(listenSocket);

    BOOL reuse = TRUE;
    setsockopt(listenSocket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(kPort);

    if (bind(listenSocket, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == SOCKET_ERROR ||
        listen(listenSocket, 4) == SOCKET_ERROR)
    {
        PostStatus(L"Could not listen on HTTPS port 8765. Is another PencilBridge running?");
        closesocket(listenSocket);
        gListenSocket.store(INVALID_SOCKET);
        ShutdownTlsCredentials();
        WSACleanup();
        return;
    }

    const std::string ip = GetConfiguredServerIPv4();
    const std::wstring ready =
        L"HTTPS ready: https://" +
        std::wstring(ip.begin(), ip.end()) +
        L":8765  |  first-time iPad setup: http://" +
        std::wstring(ip.begin(), ip.end()) +
        L":8764";
    PostStatus(ready);

    std::vector<std::thread> clientThreads;

    while (gRunning.load())
    {
        SOCKET client = accept(listenSocket, nullptr, nullptr);
        if (client != INVALID_SOCKET)
        {
            BOOL noDelay = TRUE;
            setsockopt(
                client,
                IPPROTO_TCP,
                TCP_NODELAY,
                reinterpret_cast<const char*>(&noDelay),
                sizeof(noDelay));
        }

        if (client == INVALID_SOCKET)
        {
            if (!gRunning.load())
            {
                break;
            }
            continue;
        }

        clientThreads.emplace_back(HandleClient, client);
    }

    ShutdownOpenClientSockets();
    for (std::thread& clientThread : clientThreads)
    {
        if (clientThread.joinable())
        {
            clientThread.join();
        }
    }

    const SOCKET current = gListenSocket.exchange(INVALID_SOCKET);
    if (current != INVALID_SOCKET)
    {
        closesocket(current);
    }
    ShutdownTlsCredentials();
    WSACleanup();
}


LRESULT CALLBACK WhiteboardProc(
    HWND hwnd,
    UINT message,
    WPARAM,
    LPARAM)
{
    switch (message)
    {
    case WM_NCHITTEST:
        return HTTRANSPARENT;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(hwnd, &paint);

        RECT rect{};
        GetClientRect(hwnd, &rect);

        const COLORREF keyColor = RGB(1, 2, 3);
        HBRUSH background = CreateSolidBrush(keyColor);
        FillRect(dc, &rect, background);
        DeleteObject(background);

        const int width = std::max(1L, rect.right - rect.left);
        const int height = std::max(1L, rect.bottom - rect.top);

        HPEN modePen = CreatePen(PS_SOLID, 2, RGB(255, 65, 180));
        HGDIOBJ oldModePen = SelectObject(dc, modePen);
        HGDIOBJ oldModeBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        Rectangle(dc, 1, 1, std::max(2L, rect.right - 1), std::max(2L, rect.bottom - 1));
        SelectObject(dc, oldModeBrush);
        SelectObject(dc, oldModePen);
        DeleteObject(modePen);

        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 65, 180));
        TextOutW(dc, 10, 8, L"WHITEBOARD", 10);

        std::lock_guard lock(gWhiteboardMutex);
        for (const WhiteboardSegment& segment : gWhiteboardSegments)
        {
            HPEN pen = CreatePen(
                PS_SOLID,
                segment.width,
                segment.color);
            HGDIOBJ oldPen = SelectObject(dc, pen);

            const int x1 =
                static_cast<int>(std::lround(
                    segment.x1 * static_cast<double>(width - 1)));
            const int y1 =
                static_cast<int>(std::lround(
                    segment.y1 * static_cast<double>(height - 1)));
            const int x2 =
                static_cast<int>(std::lround(
                    segment.x2 * static_cast<double>(width - 1)));
            const int y2 =
                static_cast<int>(std::lround(
                    segment.y2 * static_cast<double>(height - 1)));

            if (x1 == x2 && y1 == y2)
            {
                HGDIOBJ oldBrush =
                    SelectObject(dc, CreateSolidBrush(RGB(255, 70, 60)));
                Ellipse(
                    dc,
                    x1 - segment.width / 2,
                    y1 - segment.width / 2,
                    x1 + segment.width / 2 + 1,
                    y1 + segment.width / 2 + 1);
                HGDIOBJ brush = SelectObject(dc, oldBrush);
                DeleteObject(brush);
            }
            else
            {
                MoveToEx(dc, x1, y1, nullptr);
                LineTo(dc, x2, y2);
            }

            SelectObject(dc, oldPen);
            DeleteObject(pen);
        }

        EndPaint(hwnd, &paint);
        return 0;
    }
    }

    return DefWindowProcW(hwnd, message, 0, 0);
}

void DestroyWhiteboardOverlay()
{
    if (gWhiteboardWindow)
    {
        HWND window = gWhiteboardWindow;
        gWhiteboardWindow = nullptr;
        DestroyWindow(window);
    }
}

void UpdateWhiteboardOverlay()
{
    if (!gWhiteboardActive.load(std::memory_order_relaxed))
    {
        if (gWhiteboardWindow)
        {
            ShowWindow(gWhiteboardWindow, SW_HIDE);
        }
        return;
    }

    RECT mapping{};
    if (!GetCurrentMappingRect(mapping))
    {
        if (gWhiteboardWindow)
        {
            ShowWindow(gWhiteboardWindow, SW_HIDE);
        }
        return;
    }

    if (!gWhiteboardWindow)
    {
        gWhiteboardWindow = CreateWindowExW(
            WS_EX_LAYERED |
                WS_EX_TRANSPARENT |
                WS_EX_TOOLWINDOW |
                WS_EX_NOACTIVATE |
                WS_EX_TOPMOST,
            kWhiteboardClassName,
            L"",
            WS_POPUP,
            mapping.left,
            mapping.top,
            mapping.right - mapping.left,
            mapping.bottom - mapping.top,
            nullptr,
            nullptr,
            gInstance,
            nullptr);

        if (!gWhiteboardWindow)
        {
            PostStatus(L"Could not create whiteboard overlay.");
            return;
        }

        SetLayeredWindowAttributes(
            gWhiteboardWindow,
            RGB(1, 2, 3),
            255,
            LWA_COLORKEY);
    }

    SetWindowPos(
        gWhiteboardWindow,
        HWND_TOPMOST,
        mapping.left,
        mapping.top,
        std::max(1L, mapping.right - mapping.left),
        std::max(1L, mapping.bottom - mapping.top),
        SWP_NOACTIVATE | SWP_SHOWWINDOW);
    InvalidateRect(gWhiteboardWindow, nullptr, FALSE);
}

void SetWhiteboardActive(bool active)
{
    gWhiteboardActive.store(active, std::memory_order_release);
    gWhiteboardStrokeActive = false;

    if (gWhiteboardToggle)
    {
        SendMessageW(
            gWhiteboardToggle,
            BM_SETCHECK,
            active ? BST_CHECKED : BST_UNCHECKED,
            0);
    }

    SendWhiteboardStateToBrowser();

    if (active)
    {
        UpdateWhiteboardOverlay();
        PostStatus(
            L"Whiteboard enabled. Pencil/finger-draw now inks the transparent overlay.");
    }
    else
    {
        if (gWhiteboardWindow)
        {
            ShowWindow(gWhiteboardWindow, SW_HIDE);
        }
        PostStatus(L"Whiteboard disabled.");
    }
}

void ClearWhiteboardInk()
{
    {
        std::lock_guard lock(gWhiteboardMutex);
        gWhiteboardSegments.clear();
        gWhiteboardStrokeActive = false;
    }

    if (gWhiteboardWindow)
    {
        InvalidateRect(gWhiteboardWindow, nullptr, FALSE);
    }
    PostStatus(L"Whiteboard ink cleared.");
}

HBITMAP CaptureMappingBitmap(const RECT& mapping)
{
    const int width = mapping.right - mapping.left;
    const int height = mapping.bottom - mapping.top;
    if (width <= 0 || height <= 0)
    {
        return nullptr;
    }

    HDC screenDc = GetDC(nullptr);
    if (!screenDc)
    {
        return nullptr;
    }

    HDC memoryDc = CreateCompatibleDC(screenDc);
    HBITMAP bitmap =
        CreateCompatibleBitmap(screenDc, width, height);

    if (!memoryDc || !bitmap)
    {
        if (bitmap) DeleteObject(bitmap);
        if (memoryDc) DeleteDC(memoryDc);
        ReleaseDC(nullptr, screenDc);
        return nullptr;
    }

    HGDIOBJ oldBitmap = SelectObject(memoryDc, bitmap);
    const BOOL copied = BitBlt(
        memoryDc,
        0,
        0,
        width,
        height,
        screenDc,
        mapping.left,
        mapping.top,
        SRCCOPY);

    if (copied)
    {
        std::lock_guard lock(gWhiteboardMutex);
        for (const WhiteboardSegment& segment : gWhiteboardSegments)
        {
            HPEN pen = CreatePen(
                PS_SOLID,
                segment.width,
                segment.color);
            HGDIOBJ oldPen = SelectObject(memoryDc, pen);

            const int x1 =
                static_cast<int>(std::lround(
                    segment.x1 * static_cast<double>(width - 1)));
            const int y1 =
                static_cast<int>(std::lround(
                    segment.y1 * static_cast<double>(height - 1)));
            const int x2 =
                static_cast<int>(std::lround(
                    segment.x2 * static_cast<double>(width - 1)));
            const int y2 =
                static_cast<int>(std::lround(
                    segment.y2 * static_cast<double>(height - 1)));

            if (x1 == x2 && y1 == y2)
            {
                HGDIOBJ oldBrush =
                    SelectObject(
                        memoryDc,
                        CreateSolidBrush(RGB(255, 70, 60)));
                Ellipse(
                    memoryDc,
                    x1 - segment.width / 2,
                    y1 - segment.width / 2,
                    x1 + segment.width / 2 + 1,
                    y1 + segment.width / 2 + 1);
                HGDIOBJ brush = SelectObject(memoryDc, oldBrush);
                DeleteObject(brush);
            }
            else
            {
                MoveToEx(memoryDc, x1, y1, nullptr);
                LineTo(memoryDc, x2, y2);
            }

            SelectObject(memoryDc, oldPen);
            DeleteObject(pen);
        }
    }

    SelectObject(memoryDc, oldBitmap);
    DeleteDC(memoryDc);
    ReleaseDC(nullptr, screenDc);

    if (!copied)
    {
        DeleteObject(bitmap);
        return nullptr;
    }

    return bitmap;
}

void ClipWhiteboardToClipboard()
{
    RECT mapping{};
    if (!GetCurrentMappingRect(mapping))
    {
        PostStatus(L"Whiteboard clip failed: no valid mapping area.");
        return;
    }

    const bool wasVisible =
        gWhiteboardWindow &&
        IsWindowVisible(gWhiteboardWindow);
    if (wasVisible)
    {
        ShowWindow(gWhiteboardWindow, SW_HIDE);
        DwmFlush();
    }

    HBITMAP bitmap = CaptureMappingBitmap(mapping);

    if (wasVisible)
    {
        UpdateWhiteboardOverlay();
    }

    if (!bitmap)
    {
        PostStatus(L"Whiteboard clip failed: could not capture the screen.");
        return;
    }

    std::vector<uint8_t> png;
    const bool encoded = EncodeBitmapToPng(bitmap, png);
    DeleteObject(bitmap);

    if (!encoded ||
        !PutPngOnClipboard(png.data(), png.size()))
    {
        PostStatus(L"Whiteboard clip failed: could not write the clipboard.");
        return;
    }

    PostStatus(
        L"Whiteboard clip copied to clipboard (" +
        std::to_wstring(png.size() / 1024) +
        L" KB).");
}

void DestroyZoneSelector()
{
    if (gZoneSelectWindow)
    {
        HWND window = gZoneSelectWindow;
        gZoneSelectWindow = nullptr;
        DestroyWindow(window);
    }
    gZoneDragging = false;
}

void SetZoneStatusText()
{
    if (!gZoneText)
    {
        return;
    }

    if (gScreenRectActive.load(std::memory_order_relaxed))
    {
        const int left =
            gScreenRectLeft.load(std::memory_order_relaxed);
        const int top =
            gScreenRectTop.load(std::memory_order_relaxed);
        const int width =
            gScreenRectWidth.load(std::memory_order_relaxed);
        const int height =
            gScreenRectHeight.load(std::memory_order_relaxed);

        const std::wstring text =
            L"Screen rect: " +
            std::to_wstring(width) + L"x" +
            std::to_wstring(height) + L" @ " +
            std::to_wstring(left) + L"," +
            std::to_wstring(top);
        SetWindowTextW(gZoneText, text.c_str());
        return;
    }

    if (!gZoneActive.load(std::memory_order_relaxed))
    {
        SetWindowTextW(gZoneText, L"Mapping: full selected window");
        return;
    }

    const int left = static_cast<int>(std::lround(gZoneX.load(std::memory_order_relaxed) * 100.0));
    const int top = static_cast<int>(std::lround(gZoneY.load(std::memory_order_relaxed) * 100.0));
    const int width = static_cast<int>(std::lround(gZoneWidth.load(std::memory_order_relaxed) * 100.0));
    const int height = static_cast<int>(std::lround(gZoneHeight.load(std::memory_order_relaxed) * 100.0));

    const std::wstring text =
        L"Zone: " + std::to_wstring(width) + L"% x " + std::to_wstring(height) +
        L"%  @ " + std::to_wstring(left) + L"%, " + std::to_wstring(top) + L"%";
    SetWindowTextW(gZoneText, text.c_str());
}

void DestroyZoneOutline()
{
    if (gZoneOutlineWindow)
    {
        HWND window = gZoneOutlineWindow;
        gZoneOutlineWindow = nullptr;
        gZoneOutlineOwner = nullptr;
        DestroyWindow(window);
    }
}

void ClearZone()
{
    gScreenRectActive.store(false, std::memory_order_relaxed);
    gScreenRectLeft.store(0, std::memory_order_relaxed);
    gScreenRectTop.store(0, std::memory_order_relaxed);
    gScreenRectWidth.store(1, std::memory_order_relaxed);
    gScreenRectHeight.store(1, std::memory_order_relaxed);
    gSelectingScreenRect = false;

    gZoneActive.store(false, std::memory_order_relaxed);
    gZoneTarget.store(nullptr, std::memory_order_relaxed);
    gZoneX.store(0.0, std::memory_order_relaxed);
    gZoneY.store(0.0, std::memory_order_relaxed);
    gZoneWidth.store(1.0, std::memory_order_relaxed);
    gZoneHeight.store(1.0, std::memory_order_relaxed);
    gPenTargetGeometry.valid = false;
    DestroyZoneSelector();
    DestroyZoneOutline();
    SetZoneStatusText();
}

LRESULT CALLBACK ZoneOutlineProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_NCHITTEST:
        return HTTRANSPARENT;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(hwnd, &paint);
        RECT rect{};
        GetClientRect(hwnd, &rect);

        const COLORREF keyColor = RGB(1, 2, 3);
        HBRUSH background = CreateSolidBrush(keyColor);
        FillRect(dc, &rect, background);
        DeleteObject(background);

        HPEN pen = CreatePen(PS_SOLID, 3, RGB(0, 220, 255));
        HGDIOBJ oldPen = SelectObject(dc, pen);
        HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
        Rectangle(dc, 2, 2, std::max(3L, rect.right - 2), std::max(3L, rect.bottom - 2));
        SelectObject(dc, oldBrush);
        SelectObject(dc, oldPen);
        DeleteObject(pen);

        EndPaint(hwnd, &paint);
        return 0;
    }
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void UpdateZoneOutline()
{
    if (gScreenRectActive.load(std::memory_order_relaxed))
    {
        const int left =
            gScreenRectLeft.load(std::memory_order_relaxed);
        const int top =
            gScreenRectTop.load(std::memory_order_relaxed);
        const int width = std::max(
            1,
            gScreenRectWidth.load(std::memory_order_relaxed));
        const int height = std::max(
            1,
            gScreenRectHeight.load(std::memory_order_relaxed));

        if (!gZoneOutlineWindow || gZoneOutlineOwner != nullptr)
        {
            DestroyZoneOutline();

            gZoneOutlineWindow = CreateWindowExW(
                WS_EX_LAYERED |
                    WS_EX_TRANSPARENT |
                    WS_EX_TOOLWINDOW |
                    WS_EX_NOACTIVATE,
                kZoneOutlineClassName,
                L"",
                WS_POPUP,
                0, 0, 1, 1,
                nullptr,
                nullptr,
                gInstance,
                nullptr);

            if (!gZoneOutlineWindow)
            {
                return;
            }

            gZoneOutlineOwner = nullptr;
            SetLayeredWindowAttributes(
                gZoneOutlineWindow,
                RGB(1, 2, 3),
                255,
                LWA_COLORKEY);
        }

        constexpr int margin = 4;
        SetWindowPos(
            gZoneOutlineWindow,
            HWND_TOPMOST,
            left - margin,
            top - margin,
            std::max(8, width + margin * 2),
            std::max(8, height + margin * 2),
            SWP_NOACTIVATE | SWP_SHOWWINDOW);
        InvalidateRect(gZoneOutlineWindow, nullptr, FALSE);
        return;
    }

    if (!gZoneActive.load(std::memory_order_relaxed))
    {
        if (gZoneOutlineWindow)
        {
            ShowWindow(gZoneOutlineWindow, SW_HIDE);
        }
        return;
    }

    const HWND target = gZoneTarget.load(std::memory_order_relaxed);
    if (!target || !IsWindow(target) || !IsWindowVisible(target) || IsIconic(target))
    {
        if (gZoneOutlineWindow)
        {
            ShowWindow(gZoneOutlineWindow, SW_HIDE);
        }
        return;
    }

    TargetGeometry geometry;
    if (!CaptureTargetGeometry(target, geometry))
    {
        return;
    }

    if (!gZoneOutlineWindow || gZoneOutlineOwner != target)
    {
        DestroyZoneOutline();

        gZoneOutlineWindow = CreateWindowExW(
            WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
            kZoneOutlineClassName,
            L"",
            WS_POPUP,
            0, 0, 1, 1,
            target,
            nullptr,
            gInstance,
            nullptr);

        if (!gZoneOutlineWindow)
        {
            return;
        }

        gZoneOutlineOwner = target;
        SetLayeredWindowAttributes(gZoneOutlineWindow, RGB(1, 2, 3), 255, LWA_COLORKEY);
    }

    const double zoneX = gZoneX.load(std::memory_order_relaxed);
    const double zoneY = gZoneY.load(std::memory_order_relaxed);
    const double zoneWidth = gZoneWidth.load(std::memory_order_relaxed);
    const double zoneHeight = gZoneHeight.load(std::memory_order_relaxed);

    const int left =
        geometry.origin.x +
        static_cast<int>(std::lround(zoneX * static_cast<double>(geometry.width)));
    const int top =
        geometry.origin.y +
        static_cast<int>(std::lround(zoneY * static_cast<double>(geometry.height)));
    const int right =
        geometry.origin.x +
        static_cast<int>(std::lround((zoneX + zoneWidth) * static_cast<double>(geometry.width)));
    const int bottom =
        geometry.origin.y +
        static_cast<int>(std::lround((zoneY + zoneHeight) * static_cast<double>(geometry.height)));

    constexpr int margin = 4;
    SetWindowPos(
        gZoneOutlineWindow,
        nullptr,
        left - margin,
        top - margin,
        std::max(8, right - left + margin * 2),
        std::max(8, bottom - top + margin * 2),
        SWP_NOACTIVATE | SWP_NOZORDER | SWP_SHOWWINDOW);
    InvalidateRect(gZoneOutlineWindow, nullptr, FALSE);
}

void CompleteZoneSelection(HWND hwnd)
{
    RECT client{};
    GetClientRect(hwnd, &client);
    const int width = client.right - client.left;
    const int height = client.bottom - client.top;

    const auto ClampCoordinate = [](LONG value, int maximum) -> int
    {
        const int coordinate = static_cast<int>(value);
        if (coordinate < 0)
        {
            return 0;
        }
        if (coordinate > maximum)
        {
            return maximum;
        }
        return coordinate;
    };

    const int left = ClampCoordinate(
        std::min(gZoneDragStart.x, gZoneDragCurrent.x),
        width);
    const int right = ClampCoordinate(
        std::max(gZoneDragStart.x, gZoneDragCurrent.x),
        width);
    const int top = ClampCoordinate(
        std::min(gZoneDragStart.y, gZoneDragCurrent.y),
        height);
    const int bottom = ClampCoordinate(
        std::max(gZoneDragStart.y, gZoneDragCurrent.y),
        height);

    if (right - left < 12 || bottom - top < 12 || width <= 0 || height <= 0)
    {
        DestroyZoneSelector();
        UpdateZoneOutline();
        PostStatus(L"Zone selection cancelled: drag a larger rectangle.");
        return;
    }

    if (gSelectingScreenRect)
    {
        const int virtualX =
            gVirtualDesktopX.load(std::memory_order_relaxed);
        const int virtualY =
            gVirtualDesktopY.load(std::memory_order_relaxed);

        gScreenRectLeft.store(
            virtualX + left,
            std::memory_order_relaxed);
        gScreenRectTop.store(
            virtualY + top,
            std::memory_order_relaxed);
        gScreenRectWidth.store(
            right - left,
            std::memory_order_relaxed);
        gScreenRectHeight.store(
            bottom - top,
            std::memory_order_relaxed);

        gZoneActive.store(false, std::memory_order_relaxed);
        gZoneTarget.store(nullptr, std::memory_order_relaxed);
        gScreenRectActive.store(true, std::memory_order_release);
        gPenTargetGeometry.valid = false;
        gSelectingScreenRect = false;

        DestroyZoneSelector();
        ClearWhiteboardInk();
        SetZoneStatusText();
        UpdateZoneOutline();
        PostStatus(
            L"Screen rect selected. Input now maps to this fixed desktop area regardless of the window underneath.");
        return;
    }

    const HWND target = gTargetWindow.load();
    gScreenRectActive.store(false, std::memory_order_relaxed);
    gZoneTarget.store(target, std::memory_order_relaxed);
    gZoneX.store(static_cast<double>(left) / static_cast<double>(width), std::memory_order_relaxed);
    gZoneY.store(static_cast<double>(top) / static_cast<double>(height), std::memory_order_relaxed);
    gZoneWidth.store(static_cast<double>(right - left) / static_cast<double>(width), std::memory_order_relaxed);
    gZoneHeight.store(static_cast<double>(bottom - top) / static_cast<double>(height), std::memory_order_relaxed);
    gZoneActive.store(true, std::memory_order_release);
    gPenTargetGeometry.valid = false;

    DestroyZoneSelector();
    ClearWhiteboardInk();
    SetZoneStatusText();
    UpdateZoneOutline();
    ActivateTargetWindow(target);
    PostStatus(L"Window zone selected. PencilBridge now maps the iPad into the outlined region.");
}

LRESULT CALLBACK ZoneSelectProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_LBUTTONDOWN:
        gZoneDragging = true;
        gZoneDragStart = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        gZoneDragCurrent = gZoneDragStart;
        SetCapture(hwnd);
        InvalidateRect(hwnd, nullptr, FALSE);
        return 0;

    case WM_MOUSEMOVE:
        if (gZoneDragging)
        {
            gZoneDragCurrent = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            InvalidateRect(hwnd, nullptr, FALSE);
        }
        return 0;

    case WM_LBUTTONUP:
        if (gZoneDragging)
        {
            gZoneDragCurrent = {GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
            gZoneDragging = false;
            ReleaseCapture();
            CompleteZoneSelection(hwnd);
        }
        return 0;

    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            DestroyZoneSelector();
            gSelectingScreenRect = false;
            UpdateZoneOutline();
            PostStatus(L"Zone selection cancelled.");
            return 0;
        }
        break;

    case WM_ERASEBKGND:
        return 1;

    case WM_PAINT:
    {
        PAINTSTRUCT paint{};
        HDC dc = BeginPaint(hwnd, &paint);
        RECT rect{};
        GetClientRect(hwnd, &rect);

        HBRUSH background = CreateSolidBrush(RGB(18, 22, 28));
        FillRect(dc, &rect, background);
        DeleteObject(background);

        SetBkMode(dc, TRANSPARENT);
        SetTextColor(dc, RGB(255, 255, 255));
        RECT instruction = rect;
        instruction.top += 16;
        DrawTextW(
            dc,
            gSelectingScreenRect
                ? L"Drag anywhere to define the fixed desktop screen rect  |  Esc to cancel"
                : L"Drag to define the selected-window input zone  |  Esc to cancel",
            -1,
            &instruction,
            DT_CENTER | DT_TOP | DT_SINGLELINE);

        if (gZoneDragging)
        {
            RECT selection{
                std::min(gZoneDragStart.x, gZoneDragCurrent.x),
                std::min(gZoneDragStart.y, gZoneDragCurrent.y),
                std::max(gZoneDragStart.x, gZoneDragCurrent.x),
                std::max(gZoneDragStart.y, gZoneDragCurrent.y)};

            HBRUSH fill = CreateSolidBrush(RGB(0, 100, 120));
            FillRect(dc, &selection, fill);
            DeleteObject(fill);

            HPEN pen = CreatePen(PS_SOLID, 3, RGB(0, 240, 255));
            HGDIOBJ oldPen = SelectObject(dc, pen);
            HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
            Rectangle(dc, selection.left, selection.top, selection.right, selection.bottom);
            SelectObject(dc, oldBrush);
            SelectObject(dc, oldPen);
            DeleteObject(pen);
        }

        EndPaint(hwnd, &paint);
        return 0;
    }
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}

void BeginZoneSelection()
{
    gSelectingScreenRect = false;
    const HWND target = gTargetWindow.load();
    if (!target || !IsWindow(target) || !IsWindowVisible(target))
    {
        PostStatus(L"Select a valid target window first.");
        return;
    }

    ActivateTargetWindow(target);

    TargetGeometry geometry;
    if (!CaptureTargetGeometry(target, geometry))
    {
        PostStatus(L"Could not determine the target client area.");
        return;
    }

    if (gZoneOutlineWindow)
    {
        ShowWindow(gZoneOutlineWindow, SW_HIDE);
    }

    DestroyZoneSelector();
    gZoneDragging = false;

    gZoneSelectWindow = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        kZoneSelectClassName,
        L"",
        WS_POPUP | WS_VISIBLE,
        geometry.origin.x,
        geometry.origin.y,
        geometry.width,
        geometry.height,
        target,
        nullptr,
        gInstance,
        nullptr);

    if (!gZoneSelectWindow)
    {
        UpdateZoneOutline();
        PostStatus(L"Could not create the zone selector overlay.");
        return;
    }

    SetLayeredWindowAttributes(gZoneSelectWindow, 0, 150, LWA_ALPHA);
    SetWindowPos(
        gZoneSelectWindow,
        HWND_TOP,
        geometry.origin.x,
        geometry.origin.y,
        geometry.width,
        geometry.height,
        SWP_SHOWWINDOW);
    SetForegroundWindow(gZoneSelectWindow);
    SetFocus(gZoneSelectWindow);
    PostStatus(L"Drag over the part of the target window you want the iPad to control.");
}

void BeginScreenRectSelection()
{
    RefreshVirtualDesktopGeometry();

    const int virtualX =
        gVirtualDesktopX.load(std::memory_order_relaxed);
    const int virtualY =
        gVirtualDesktopY.load(std::memory_order_relaxed);
    const int virtualWidth =
        gVirtualDesktopWidth.load(std::memory_order_relaxed);
    const int virtualHeight =
        gVirtualDesktopHeight.load(std::memory_order_relaxed);

    if (virtualWidth <= 1 || virtualHeight <= 1)
    {
        PostStatus(L"Could not determine the virtual desktop bounds.");
        return;
    }

    if (gZoneOutlineWindow)
    {
        ShowWindow(gZoneOutlineWindow, SW_HIDE);
    }

    DestroyZoneSelector();
    gSelectingScreenRect = true;
    gZoneDragging = false;

    gZoneSelectWindow = CreateWindowExW(
        WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_TOPMOST,
        kZoneSelectClassName,
        L"",
        WS_POPUP | WS_VISIBLE,
        virtualX,
        virtualY,
        virtualWidth,
        virtualHeight,
        nullptr,
        nullptr,
        gInstance,
        nullptr);

    if (!gZoneSelectWindow)
    {
        gSelectingScreenRect = false;
        UpdateZoneOutline();
        PostStatus(L"Could not create the screen-rect selector overlay.");
        return;
    }

    SetLayeredWindowAttributes(
        gZoneSelectWindow,
        0,
        150,
        LWA_ALPHA);
    SetWindowPos(
        gZoneSelectWindow,
        HWND_TOPMOST,
        virtualX,
        virtualY,
        virtualWidth,
        virtualHeight,
        SWP_SHOWWINDOW);
    SetForegroundWindow(gZoneSelectWindow);
    SetFocus(gZoneSelectWindow);
    PostStatus(
        L"Drag anywhere on the desktop to define the fixed PencilBridge screen rect.");
}

void StopServer()
{
    gRunning.store(false);

    const SOCKET activeWebSocket = gWebSocketClient.exchange(INVALID_SOCKET);
    if (activeWebSocket != INVALID_SOCKET)
    {
        shutdown(activeWebSocket, SD_BOTH);
    }

    ReleaseActiveInputState();
    ShutdownOpenClientSockets();

    const SOCKET listener = gListenSocket.exchange(INVALID_SOCKET);
    if (listener != INVALID_SOCKET)
    {
        shutdown(listener, SD_BOTH);
        closesocket(listener);
    }

    const SOCKET bootstrap =
        gBootstrapListenSocket.exchange(INVALID_SOCKET);
    if (bootstrap != INVALID_SOCKET)
    {
        shutdown(bootstrap, SD_BOTH);
        closesocket(bootstrap);
    }
}

LRESULT CALLBACK WindowProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch (message)
    {
    case WM_CREATE:
    {
        HWND label = CreateWindowExW(
            0, L"STATIC", L"Target window",
            WS_CHILD | WS_VISIBLE,
            20, 20, 200, 20,
            hwnd, nullptr, nullptr, nullptr);

        gTargetCombo = CreateWindowExW(
            0, L"COMBOBOX", L"",
            WS_CHILD | WS_VISIBLE | CBS_DROPDOWNLIST | WS_VSCROLL,
            20, 44, 540, 300,
            hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_TARGET_COMBO)), nullptr, nullptr);

        HWND refresh = CreateWindowExW(
            0, L"BUTTON", L"Refresh",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            570, 44, 100, 28,
            hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_REFRESH_BUTTON)), nullptr, nullptr);

        HWND selectZone = CreateWindowExW(
            0, L"BUTTON", L"Select Zone",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            20, 82, 110, 28,
            hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SELECT_ZONE_BUTTON)), nullptr, nullptr);

        HWND selectScreenRect = CreateWindowExW(
            0, L"BUTTON", L"Screen Rect",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            140, 82, 110, 28,
            hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(ID_SELECT_SCREEN_RECT_BUTTON)),
            nullptr,
            nullptr);

        HWND clearZone = CreateWindowExW(
            0, L"BUTTON", L"Clear",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            260, 82, 80, 28,
            hwnd, reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_CLEAR_ZONE_BUTTON)), nullptr, nullptr);

        gZoneText = CreateWindowExW(
            0, L"STATIC", L"Mapping: full selected window",
            WS_CHILD | WS_VISIBLE,
            355, 87, 315, 20,
            hwnd, nullptr, nullptr, nullptr);

        gWhiteboardToggle = CreateWindowExW(
            0, L"BUTTON", L"Whiteboard",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            20, 120, 105, 28,
            hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(ID_WHITEBOARD_TOGGLE)),
            nullptr,
            nullptr);

        HWND whiteboardClip = CreateWindowExW(
            0, L"BUTTON", L"Clip Ink",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            135, 120, 90, 28,
            hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(ID_WHITEBOARD_CLIP)),
            nullptr,
            nullptr);

        HWND whiteboardClear = CreateWindowExW(
            0, L"BUTTON", L"Clear Ink",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            235, 120, 90, 28,
            hwnd,
            reinterpret_cast<HMENU>(
                static_cast<INT_PTR>(ID_WHITEBOARD_CLEAR)),
            nullptr,
            nullptr);

        HWND sendClipboard = CreateWindowExW(
            0, L"BUTTON", L"Send Clipboard",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            335, 120, 125, 28,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SEND_CLIPBOARD_BUTTON)),
            nullptr,
            nullptr);

        gAutoClipboardCheck = CreateWindowExW(
            0, L"BUTTON", L"Auto-send clipboard images",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            475, 122, 195, 24,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_AUTO_CLIPBOARD_CHECK)),
            nullptr,
            nullptr);
        SendMessageW(gAutoClipboardCheck, BM_SETCHECK, BST_CHECKED, 0);

        HWND urlLabel = CreateWindowExW(
            0, L"STATIC", L"Open this on the iPad (same LAN):",
            WS_CHILD | WS_VISIBLE,
            20, 164, 260, 20,
            hwnd, nullptr, nullptr, nullptr);

        gUrlText = CreateWindowExW(
            0, L"STATIC", L"Starting server...",
            WS_CHILD | WS_VISIBLE,
            20, 188, 650, 38,
            hwnd, nullptr, nullptr, nullptr);

        gStatusText = CreateWindowExW(
            0, L"STATIC", L"Starting...",
            WS_CHILD | WS_VISIBLE,
            20, 236, 650, 44,
            hwnd, nullptr, nullptr, nullptr);

        ApplyDefaultFont(label);
        ApplyDefaultFont(gTargetCombo);
        ApplyDefaultFont(refresh);
        ApplyDefaultFont(selectZone);
        ApplyDefaultFont(selectScreenRect);
        ApplyDefaultFont(clearZone);
        ApplyDefaultFont(gZoneText);
        ApplyDefaultFont(gWhiteboardToggle);
        ApplyDefaultFont(whiteboardClip);
        ApplyDefaultFont(whiteboardClear);
        ApplyDefaultFont(sendClipboard);
        ApplyDefaultFont(gAutoClipboardCheck);
        ApplyDefaultFont(urlLabel);
        ApplyDefaultFont(gUrlText);
        ApplyDefaultFont(gStatusText);

        ApplyDarkWindowTheme(hwnd);
        ApplyDarkControlTheme(gTargetCombo);
        ApplyDarkControlTheme(refresh);
        ApplyDarkControlTheme(selectZone);
        ApplyDarkControlTheme(selectScreenRect);
        ApplyDarkControlTheme(clearZone);
        ApplyDarkControlTheme(gWhiteboardToggle);
        ApplyDarkControlTheme(whiteboardClip);
        ApplyDarkControlTheme(whiteboardClear);
        ApplyDarkControlTheme(sendClipboard);
        ApplyDarkControlTheme(gAutoClipboardCheck);

        AddClipboardFormatListener(hwnd);
        SetTimer(hwnd, ID_ZONE_TRACK_TIMER, 100, nullptr);
        return 0;
    }

    case WM_COMMAND:
        if (LOWORD(wParam) == ID_REFRESH_BUTTON && HIWORD(wParam) == BN_CLICKED)
        {
            RefreshWindows();
            return 0;
        }
        if (LOWORD(wParam) == ID_TARGET_COMBO && HIWORD(wParam) == CBN_SELCHANGE)
        {
            const int index = static_cast<int>(SendMessageW(gTargetCombo, CB_GETCURSEL, 0, 0));
            SelectComboIndex(index, true);
            return 0;
        }
        if (LOWORD(wParam) == ID_SELECT_ZONE_BUTTON && HIWORD(wParam) == BN_CLICKED)
        {
            BeginZoneSelection();
            return 0;
        }
        if (LOWORD(wParam) == ID_SELECT_SCREEN_RECT_BUTTON &&
            HIWORD(wParam) == BN_CLICKED)
        {
            BeginScreenRectSelection();
            return 0;
        }
        if (LOWORD(wParam) == ID_CLEAR_ZONE_BUTTON && HIWORD(wParam) == BN_CLICKED)
        {
            ClearZone();
            PostStatus(L"Mapping cleared. Input uses the full selected window.");
            return 0;
        }
        if (LOWORD(wParam) == ID_WHITEBOARD_TOGGLE &&
            HIWORD(wParam) == BN_CLICKED)
        {
            const LRESULT checked =
                SendMessageW(
                    gWhiteboardToggle,
                    BM_GETCHECK,
                    0,
                    0);
            SetWhiteboardActive(checked == BST_CHECKED);
            return 0;
        }
        if (LOWORD(wParam) == ID_WHITEBOARD_CLIP &&
            HIWORD(wParam) == BN_CLICKED)
        {
            ClipWhiteboardToClipboard();
            return 0;
        }
        if (LOWORD(wParam) == ID_WHITEBOARD_CLEAR &&
            HIWORD(wParam) == BN_CLICKED)
        {
            ClearWhiteboardInk();
            return 0;
        }
        if (LOWORD(wParam) == ID_SEND_CLIPBOARD_BUTTON && HIWORD(wParam) == BN_CLICKED)
        {
            SendClipboardToIpad();
            return 0;
        }
        if (LOWORD(wParam) == ID_AUTO_CLIPBOARD_CHECK && HIWORD(wParam) == BN_CLICKED)
        {
            const LRESULT checked = SendMessageW(gAutoClipboardCheck, BM_GETCHECK, 0, 0);
            gAutoSendClipboard.store(checked == BST_CHECKED, std::memory_order_relaxed);
            PostStatus(
                checked == BST_CHECKED
                    ? L"Automatic clipboard image markup enabled."
                    : L"Automatic clipboard image markup disabled.");
            return 0;
        }
        break;

    case WM_CTLCOLORSTATIC:
    {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, kDarkText);
        SetBkColor(dc, kDarkBackground);
        SetBkMode(dc, TRANSPARENT);
        return reinterpret_cast<LRESULT>(gDarkBackgroundBrush);
    }

    case WM_CTLCOLORBTN:
    case WM_CTLCOLOREDIT:
    case WM_CTLCOLORLISTBOX:
    {
        HDC dc = reinterpret_cast<HDC>(wParam);
        SetTextColor(dc, kDarkText);
        SetBkColor(dc, kDarkControl);
        return reinterpret_cast<LRESULT>(gDarkControlBrush);
    }

    case WM_CLIPBOARDUPDATE:
        if (gIgnoreNextClipboardUpdate.exchange(false, std::memory_order_relaxed))
        {
            return 0;
        }
        if (gAutoSendClipboard.load(std::memory_order_relaxed) &&
            (IsClipboardFormatAvailable(CF_BITMAP) ||
             IsClipboardFormatAvailable(CF_DIBV5) ||
             IsClipboardFormatAvailable(CF_DIB)))
        {
            SendClipboardToIpad();
        }
        return 0;

    case WM_TIMER:
        if (wParam == ID_ZONE_TRACK_TIMER)
        {
            UpdateZoneOutline();
            UpdateWhiteboardOverlay();
            return 0;
        }
        break;

    case WM_DISPLAYCHANGE:
        RefreshVirtualDesktopGeometry();
        return 0;

    case WM_SIZE:
    {
        const int width = LOWORD(lParam);
        MoveWindow(gTargetCombo, 20, 44, std::max(180, width - 150), 300, TRUE);
        HWND refresh = GetDlgItem(hwnd, ID_REFRESH_BUTTON);
        MoveWindow(refresh, std::max(20, width - 120), 44, 100, 28, TRUE);
        MoveWindow(gZoneText, 355, 87, std::max(100, width - 375), 20, TRUE);
        MoveWindow(gUrlText, 20, 188, std::max(100, width - 40), 38, TRUE);
        MoveWindow(gStatusText, 20, 236, std::max(100, width - 40), 44, TRUE);
        return 0;
    }

    case WM_APP_STATUS:
    {
        auto* text = reinterpret_cast<std::wstring*>(lParam);
        if (text)
        {
            SetWindowTextW(gStatusText, text->c_str());
            delete text;
        }
        return 0;
    }

    case WM_DESTROY:
        RemoveClipboardFormatListener(hwnd);
        KillTimer(hwnd, ID_ZONE_TRACK_TIMER);
        DestroyZoneSelector();
        DestroyZoneOutline();
        DestroyWhiteboardOverlay();
        StopServer();
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcW(hwnd, message, wParam, lParam);
}
}

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCommand)
{
    gInstance = instance;
    gDarkBackgroundBrush = CreateSolidBrush(kDarkBackground);
    gDarkControlBrush = CreateSolidBrush(kDarkControl);
    const HRESULT comResult = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    RefreshVirtualDesktopGeometry();

    gPenDevice = CreateSyntheticPointerDevice(PT_PEN, 1, POINTER_FEEDBACK_NONE);
    gTouchInjectionReady =
        InitializeTouchInjection(10, TOUCH_FEEDBACK_NONE) != FALSE;
    if (!gTouchInjectionReady)
    {
        PostStatus(
            L"Warning: Windows touch injection initialization failed.");
    }

    const wchar_t kClassName[] = L"PencilBridgeWindow";

    WNDCLASSW windowClass{};
    windowClass.lpfnWndProc = WindowProc;
    windowClass.hInstance = instance;
    windowClass.lpszClassName = kClassName;
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.hbrBackground = gDarkBackgroundBrush;

    if (!RegisterClassW(&windowClass))
    {
        MessageBoxW(nullptr, L"Could not register PencilBridge window class.", L"PencilBridge", MB_ICONERROR);
        return 1;
    }

    WNDCLASSW zoneSelectClass{};
    zoneSelectClass.lpfnWndProc = ZoneSelectProc;
    zoneSelectClass.hInstance = instance;
    zoneSelectClass.lpszClassName = kZoneSelectClassName;
    zoneSelectClass.hCursor = LoadCursor(nullptr, IDC_CROSS);
    zoneSelectClass.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));

    WNDCLASSW zoneOutlineClass{};
    zoneOutlineClass.lpfnWndProc = ZoneOutlineProc;
    zoneOutlineClass.hInstance = instance;
    zoneOutlineClass.lpszClassName = kZoneOutlineClassName;
    zoneOutlineClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    zoneOutlineClass.hbrBackground = reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));

    WNDCLASSW whiteboardClass{};
    whiteboardClass.lpfnWndProc = WhiteboardProc;
    whiteboardClass.hInstance = instance;
    whiteboardClass.lpszClassName = kWhiteboardClassName;
    whiteboardClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    whiteboardClass.hbrBackground =
        reinterpret_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));

    if (!RegisterClassW(&zoneSelectClass) ||
        !RegisterClassW(&zoneOutlineClass) ||
        !RegisterClassW(&whiteboardClass))
    {
        MessageBoxW(
            nullptr,
            L"Could not register PencilBridge overlay classes.",
            L"PencilBridge",
            MB_ICONERROR);
        return 1;
    }

    gMainWindow = CreateWindowExW(
        0,
        kClassName,
        L"PencilBridge",
        WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,
        CW_USEDEFAULT,
        720,
        360,
        nullptr,
        nullptr,
        instance,
        nullptr);

    if (!gMainWindow)
    {
        MessageBoxW(nullptr, L"Could not create PencilBridge window.", L"PencilBridge", MB_ICONERROR);
        if (gPenDevice)
        {
            DestroySyntheticPointerDevice(gPenDevice);
        }
        return 1;
    }

    ApplyDarkWindowTheme(gMainWindow);
    ShowWindow(gMainWindow, showCommand);
    UpdateWindow(gMainWindow);

    RefreshWindows();

    WSADATA addressWsa{};
    std::string ip = "127.0.0.1";
    if (WSAStartup(MAKEWORD(2, 2), &addressWsa) == 0)
    {
        ip = GetConfiguredServerIPv4();
        WSACleanup();
    }

    const std::wstring url =
        L"https://" +
        std::wstring(ip.begin(), ip.end()) +
        L":8765    setup: http://" +
        std::wstring(ip.begin(), ip.end()) +
        L":8764";
    SetWindowTextW(gUrlText, url.c_str());

    if (!gPenDevice)
    {
        SetWindowTextW(
            gStatusText,
            L"Warning: Windows synthetic pen device creation failed. Finger mouse can still work.");
    }

    gBootstrapThread =
        std::thread(BootstrapServerMain);
    gServerThread = std::thread(ServerMain);

    MSG message{};
    while (GetMessageW(&message, nullptr, 0, 0) > 0)
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }

    StopServer();
    if (gServerThread.joinable())
    {
        gServerThread.join();
    }
    if (gBootstrapThread.joinable())
    {
        gBootstrapThread.join();
    }

    if (gPenDevice)
    {
        DestroySyntheticPointerDevice(gPenDevice);
        gPenDevice = nullptr;
    }

    if (gWicFactory)
    {
        gWicFactory->Release();
        gWicFactory = nullptr;
    }

    if (SUCCEEDED(comResult))
    {
        CoUninitialize();
    }

    if (gDarkControlBrush)
    {
        DeleteObject(gDarkControlBrush);
        gDarkControlBrush = nullptr;
    }
    if (gDarkBackgroundBrush)
    {
        DeleteObject(gDarkBackgroundBrush);
        gDarkBackgroundBrush = nullptr;
    }

    return 0;
}
