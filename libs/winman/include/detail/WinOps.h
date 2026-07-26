#pragma once

#include <detail/Common.h>

#include <vector>

namespace winman {

// Stateless facade for enumerating and manipulating top-level windows.
class WINMAN_API WinOps
{
public:
    // Returns visible, title-bearing top-level windows in current Z-order.
    static std::vector<WinInfo> enumerateWindows();

    // Refreshes one entry in place. Returns false if the window no longer exists.
    static bool refreshWindowInfo(WinInfo &info);

    // Resizes the target window to width x height in pixels.
    static bool resizeWindow(std::uintptr_t hwnd, int width, int height);

    // Centers the target window on its monitor work area.
    static bool centerWindow(std::uintptr_t hwnd);

    // Applies resize and center as one operation sequence.
    static bool resizeAndCenterWindow(std::uintptr_t hwnd, int width, int height);
};

} // namespace winman
