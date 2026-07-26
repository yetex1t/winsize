#include <winman>

#include <Windows.h>

#include <TlHelp32.h>
#include <dwmapi.h>

#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QStringList>

#include <string>
#include <unordered_map>
#include <vector>
#include <mutex>

namespace winman {

// Filter configuration loaded from filters.json
struct Filters {
    QSet<QString> processes;  // Process names to exclude (stored lowercase)
    QSet<QString> classes;    // Window class names to exclude (stored lowercase)
};

// Returns filter configuration (thread-safe lazy initialization via std::call_once)
static const Filters& getFilters()
{
    static Filters filters;
    static std::once_flag flag;
    std::call_once(flag, []() {
        QFile file(":/winman/filters.json");
        if (!file.open(QIODevice::ReadOnly))
            return;

        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        if (doc.isNull() || !doc.isObject())
            return;

        QJsonObject root = doc.object();

        for (const QJsonValue &v : root.value("filtered_processes").toArray()) {
            QString s = v.toString();
            if (!s.isEmpty())
                filters.processes.insert(s.toLower());
        }

        for (const QJsonValue &v : root.value("filtered_classes").toArray()) {
            QString s = v.toString();
            if (!s.isEmpty())
                filters.classes.insert(s.toLower());
        }
    });
    return filters;
}

// Returns true if the process name should be filtered out (O(1) lookup)
static bool isFilteredProcessName(const std::wstring &name)
{
    const auto& filters = getFilters();
    return filters.processes.contains(QString::fromWCharArray(name.c_str()).toLower());
}

// Returns true if the window class name should be filtered out (O(1) lookup)
static bool isFilteredWindowClass(const wchar_t *className)
{
    const auto& filters = getFilters();
    return filters.classes.contains(QString::fromWCharArray(className).toLower());
}

// Single-process lookup fallback (used when cache misses)
static std::wstring processNameFromPid(DWORD pid)
{
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return {};

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);

    if (Process32FirstW(snap, &entry)) {
        do {
            if (entry.th32ProcessID == pid) {
                CloseHandle(snap);
                return entry.szExeFile;
            }
        } while (Process32NextW(snap, &entry));
    }

    CloseHandle(snap);
    return {};
}

// Build PID-to-process-name map from a single snapshot (O(N))
static std::unordered_map<DWORD, std::wstring> buildProcessNameMap()
{
    std::unordered_map<DWORD, std::wstring> map;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE)
        return map;

    PROCESSENTRY32W entry{};
    entry.dwSize = sizeof(entry);

    if (Process32FirstW(snap, &entry)) {
        do {
            map[entry.th32ProcessID] = entry.szExeFile;
        } while (Process32NextW(snap, &entry));
    }

    CloseHandle(snap);
    return map;
}

// Query process start time and elevation status
static bool processInfoFromPid(DWORD pid, uint64_t &startTime, bool &isElevated)
{
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!process)
        process = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
    if (!process)
        return false;

    startTime = 0;
    isElevated = false;

    FILETIME createTime{}, exitTime{}, kernelTime{}, userTime{};
    if (GetProcessTimes(process, &createTime, &exitTime, &kernelTime, &userTime)) {
        ULARGE_INTEGER ticks{};
        ticks.LowPart = createTime.dwLowDateTime;
        ticks.HighPart = createTime.dwHighDateTime;
        startTime = ticks.QuadPart;
    }

    HANDLE token = nullptr;
    if (OpenProcessToken(process, TOKEN_QUERY, &token)) {
        TOKEN_ELEVATION elevation{};
        DWORD outSize = 0;
        if (GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &outSize))
            isElevated = elevation.TokenIsElevated != 0;
        CloseHandle(token);
    }

    CloseHandle(process);
    return true;
}

// Populate WinInfo for a given window handle.
// knownProcessName avoids a redundant process snapshot lookup when already cached.
static bool fillInfo(HWND hwnd, WinInfo &info, const std::wstring *knownProcessName = nullptr)
{
    if (!IsWindow(hwnd))
        return false;

    info.hwnd = reinterpret_cast<std::uintptr_t>(hwnd);

    int len = GetWindowTextLengthW(hwnd);
    if (len > 0) {
        info.windowTitle.resize(static_cast<size_t>(len) + 1, L'\0');
        int copied = GetWindowTextW(hwnd, &info.windowTitle[0], len + 1);
        if (copied > 0) {
            info.windowTitle.resize(static_cast<size_t>(copied));
        } else {
            info.windowTitle.clear();
        }
    } else {
        info.windowTitle.clear();
    }

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    info.pid = pid;
    info.processName = knownProcessName ? *knownProcessName : processNameFromPid(pid);
    processInfoFromPid(pid, info.processStartTime, info.isElevated);

    RECT rc{};
    if (!GetWindowRect(hwnd, &rc))
        return false;

    info.x = rc.left;
    info.y = rc.top;
    info.width = rc.right - rc.left;
    info.height = rc.bottom - rc.top;

    return true;
}

// Context passed through LPARAM to the EnumWindows callback
struct EnumContext {
    std::vector<WinInfo> *result;
    const std::unordered_map<DWORD, std::wstring> *processMap;  // Pre-built PID->name cache
};

// EnumWindows callback: filters and collects visible window info
static BOOL CALLBACK enumCallback(HWND hwnd, LPARAM lParam)
{
    if (!IsWindowVisible(hwnd))
        return TRUE;

    if (GetWindowTextLengthW(hwnd) == 0)
        return TRUE;

    wchar_t className[256] = {};
    int classLen = GetClassNameW(hwnd, className, static_cast<int>(sizeof(className) / sizeof(wchar_t)));
    if (classLen > 0 && isFilteredWindowClass(className))
        return TRUE;

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    std::wstring processName;
    if (pid != 0) {
        auto *ctx = reinterpret_cast<EnumContext *>(lParam);
        auto it = ctx->processMap->find(pid);
        if (it != ctx->processMap->end()) {
            processName = it->second;
        } else {
            processName = processNameFromPid(pid);
        }
        if (!processName.empty() && isFilteredProcessName(processName))
            return TRUE;
    }

    DWORD cloaked = 0;
    HRESULT hr = DwmGetWindowAttribute(hwnd, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
    if (SUCCEEDED(hr) && cloaked)
        return TRUE;

    auto *ctx = reinterpret_cast<EnumContext *>(lParam);

    WinInfo info;
    if (fillInfo(hwnd, info, &processName))
        ctx->result->push_back(std::move(info));

    return TRUE;
}

// Get the work area of the monitor containing the window (excludes taskbar)
static RECT monitorWorkArea(HWND hwnd)
{
    HMONITOR mon = MonitorFromWindow(hwnd, MONITOR_DEFAULTTOPRIMARY);
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    if (GetMonitorInfoW(mon, &mi))
        return mi.rcWork;

    return {0, 0, GetSystemMetrics(SM_CXSCREEN), GetSystemMetrics(SM_CYSCREEN)};
}

// Update the window's normal position (used for restore-from-minimize)
static void updateNormalPlacement(HWND hwnd, const RECT &normalRect)
{
    WINDOWPLACEMENT wp{};
    wp.length = sizeof(wp);
    if (!GetWindowPlacement(hwnd, &wp))
        return;

    wp.showCmd = SW_SHOWNORMAL;
    wp.rcNormalPosition = normalRect;
    SetWindowPlacement(hwnd, &wp);
}

// Restore window to normal state if minimized or maximized
static void restoreIfMinimizedOrMaximized(HWND hwnd)
{
    if (!IsIconic(hwnd) && !IsZoomed(hwnd))
        return;

    ShowWindow(hwnd, SW_RESTORE);

    WINDOWPLACEMENT wp{};
    wp.length = sizeof(wp);
    if (GetWindowPlacement(hwnd, &wp) && wp.showCmd != SW_SHOWNORMAL) {
        wp.showCmd = SW_SHOWNORMAL;
        SetWindowPlacement(hwnd, &wp);
    }
}

// Build process name cache once, then enumerate all visible windows
std::vector<WinInfo> WinOps::enumerateWindows()
{
    std::vector<WinInfo> result;
    auto processMap = buildProcessNameMap();
    EnumContext ctx{&result, &processMap};
    EnumWindows(enumCallback, reinterpret_cast<LPARAM>(&ctx));
    return result;
}

bool WinOps::refreshWindowInfo(WinInfo &info)
{
    HWND hwnd = reinterpret_cast<HWND>(info.hwnd);
    return fillInfo(hwnd, info);
}

bool WinOps::resizeWindow(std::uintptr_t hwnd, int width, int height)
{
    HWND h = reinterpret_cast<HWND>(hwnd);
    if (!IsWindow(h))
        return false;

    restoreIfMinimizedOrMaximized(h);

    return SetWindowPos(h, nullptr, 0, 0, width, height, SWP_NOMOVE | SWP_NOZORDER | SWP_NOACTIVATE)
           != 0;
}

bool WinOps::centerWindow(std::uintptr_t hwnd)
{
    HWND h = reinterpret_cast<HWND>(hwnd);
    if (!IsWindow(h))
        return false;

    restoreIfMinimizedOrMaximized(h);

    RECT winRect{};
    if (!GetWindowRect(h, &winRect))
        return false;

    int winW = winRect.right - winRect.left;
    int winH = winRect.bottom - winRect.top;

    RECT work = monitorWorkArea(h);
    int newX = work.left + (work.right - work.left - winW) / 2;
    int newY = work.top + (work.bottom - work.top - winH) / 2;

    RECT normalRect{newX, newY, newX + winW, newY + winH};
    updateNormalPlacement(h, normalRect);

    return SetWindowPos(h, nullptr, newX, newY, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE)
           != 0;
}

bool WinOps::resizeAndCenterWindow(std::uintptr_t hwnd, int width, int height)
{
    HWND h = reinterpret_cast<HWND>(hwnd);
    if (!IsWindow(h))
        return false;

    restoreIfMinimizedOrMaximized(h);

    RECT work = monitorWorkArea(h);
    int newX = work.left + (work.right - work.left - width) / 2;
    int newY = work.top + (work.bottom - work.top - height) / 2;

    RECT normalRect{newX, newY, newX + width, newY + height};
    updateNormalPlacement(h, normalRect);

    return SetWindowPos(h, nullptr, newX, newY, width, height, SWP_NOZORDER | SWP_NOACTIVATE) != 0;
}

} // namespace winman
