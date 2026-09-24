#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <windowsx.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <bcrypt.h>
#include <wincrypt.h>
#include <wincodec.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <charconv>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "web_ui.h"

#pragma comment(lib, "bcrypt.lib")
#pragma comment(lib, "crypt32.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "ole32.lib")
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
constexpr UINT_PTR ID_ZONE_TRACK_TIMER = 2001;
constexpr int kPort = 8765;
constexpr wchar_t kZoneSelectClassName[] = L"PencilBridgeZoneSelect";
constexpr wchar_t kZoneOutlineClassName[] = L"PencilBridgeZoneOutline";

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

bool gSelectingScreenRect = false;
bool gZoneDragging = false;
POINT gZoneDragStart{};
POINT gZoneDragCurrent{};

std::vector<WindowEntry> gWindows;
std::atomic<HWND> gTargetWindow{nullptr};
std::atomic<bool> gRunning{true};
std::atomic<SOCKET> gListenSocket{INVALID_SOCKET};
std::atomic<SOCKET> gWebSocketClient{INVALID_SOCKET};
std::mutex gWebSocketSendMutex;
std::mutex gOpenClientSocketsMutex;
std::vector<SOCKET> gOpenClientSockets;
std::thread gServerThread;

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

bool SendAll(SOCKET socket, const char* data, size_t size)
{
    size_t sent = 0;
    while (sent < size)
    {
        const int chunk = send(socket, data + sent, static_cast<int>(size - sent), 0);
        if (chunk <= 0)
        {
            return false;
        }
        sent += static_cast<size_t>(chunk);
    }
    return true;
}

bool SendAll(SOCKET socket, const std::string& data)
{
    return SendAll(socket, data.data(), data.size());
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
        const int chunk = recv(socket, reinterpret_cast<char*>(data + received), static_cast<int>(size - received), 0);
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
    const HWND target = gTargetWindow.load();
    if (!target || !IsWindow(target))
    {
        return;
    }

    std::lock_guard lock(gInputMutex);
    ActivateTargetWindow(target);

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

bool ProcessCommandMessage(std::string_view message)
{
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
        const int received = recv(socket, buffer.data(), static_cast<int>(buffer.size()), 0);
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

    std::string request;
    if (!ReadHttpRequest(socket, request))
    {
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

    UnregisterOpenClientSocket(socket);
    shutdown(socket, SD_BOTH);
    closesocket(socket);
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

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET)
    {
        PostStatus(L"Could not create listening socket.");
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
        PostStatus(L"Could not listen on port 8765. Is another PencilBridge running?");
        closesocket(listenSocket);
        gListenSocket.store(INVALID_SOCKET);
        WSACleanup();
        return;
    }

    const std::string ip = GetLocalIPv4();
    const std::wstring ready =
        L"Waiting for iPad/browser on http://" +
        std::wstring(ip.begin(), ip.end()) +
        L":8765";
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
    WSACleanup();
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

        HWND sendClipboard = CreateWindowExW(
            0, L"BUTTON", L"Send Clipboard",
            WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON,
            20, 120, 125, 28,
            hwnd,
            reinterpret_cast<HMENU>(static_cast<INT_PTR>(ID_SEND_CLIPBOARD_BUTTON)),
            nullptr,
            nullptr);

        gAutoClipboardCheck = CreateWindowExW(
            0, L"BUTTON", L"Auto-send clipboard images",
            WS_CHILD | WS_VISIBLE | BS_AUTOCHECKBOX,
            160, 122, 220, 24,
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
            20, 188, 650, 24,
            hwnd, nullptr, nullptr, nullptr);

        gStatusText = CreateWindowExW(
            0, L"STATIC", L"Starting...",
            WS_CHILD | WS_VISIBLE,
            20, 228, 650, 44,
            hwnd, nullptr, nullptr, nullptr);

        ApplyDefaultFont(label);
        ApplyDefaultFont(gTargetCombo);
        ApplyDefaultFont(refresh);
        ApplyDefaultFont(selectZone);
        ApplyDefaultFont(selectScreenRect);
        ApplyDefaultFont(clearZone);
        ApplyDefaultFont(gZoneText);
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
        MoveWindow(gUrlText, 20, 188, std::max(100, width - 40), 24, TRUE);
        MoveWindow(gStatusText, 20, 228, std::max(100, width - 40), 44, TRUE);
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

    if (!RegisterClassW(&zoneSelectClass) || !RegisterClassW(&zoneOutlineClass))
    {
        MessageBoxW(nullptr, L"Could not register PencilBridge zone overlay classes.", L"PencilBridge", MB_ICONERROR);
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
        ip = GetLocalIPv4();
        WSACleanup();
    }

    const std::wstring url =
        L"http://" + std::wstring(ip.begin(), ip.end()) + L":8765";
    SetWindowTextW(gUrlText, url.c_str());

    if (!gPenDevice)
    {
        SetWindowTextW(
            gStatusText,
            L"Warning: Windows synthetic pen device creation failed. Finger mouse can still work.");
    }

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
