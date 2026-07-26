#pragma once

#ifdef _WIN32
#ifdef WINMAN_LIBRARY
#define WINMAN_API __declspec(dllexport)
#else
#define WINMAN_API __declspec(dllimport)
#endif
#else
#define WINMAN_API
#endif

#include <cstdint>
#include <string>

namespace winman {

// Snapshot of a visible top-level window.
struct WINMAN_API WinInfo
{
    std::uintptr_t hwnd = 0;       // Native window handle for ABI-safe transport.
    std::wstring processName;      // Executable name of the owning process.
    uint32_t pid = 0;              // Process identifier of the owning process.
    uint64_t processStartTime = 0; // Process creation time in FILETIME ticks.
    bool isElevated = false;       // Whether the owning process runs elevated.
    std::wstring windowTitle;      // Current title text shown by the window.
    int x = 0;                     // Left edge in virtual screen coordinates.
    int y = 0;                     // Top edge in virtual screen coordinates.
    int width = 0;                 // Current window width in pixels.
    int height = 0;                // Current window height in pixels.
};

} // namespace winman
