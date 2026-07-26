#include "MainWindow.h"

#include <winman>

#include <QApplication>
#include <QFile>
#include <QIcon>
#include <QLocale>
#include <QTranslator>

#ifdef _WIN32
#include <Windows.h>
#endif

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

#ifdef _WIN32
    // Single-instance enforcement via named mutex
    static const wchar_t kMutexName[] = L"winsize_single_instance_mutex";
    static const wchar_t kSharedMemName[] = L"winsize_shared_mem";

    HANDLE hMutex = CreateMutexW(nullptr, FALSE, kMutexName);
    if (!hMutex) {
        // Failed to create mutex, continue anyway
    } else if (GetLastError() == ERROR_ALREADY_EXISTS) {
        // Another instance is running: read its HWND from shared memory and activate it
        HANDLE hMap = OpenFileMappingW(FILE_MAP_READ, FALSE, kSharedMemName);
        if (hMap) {
            HWND *pHwnd = static_cast<HWND *>(MapViewOfFile(hMap, FILE_MAP_READ, 0, 0, sizeof(HWND)));
            if (pHwnd) {
                HWND hwnd = *pHwnd;
                UnmapViewOfFile(pHwnd);

                AllowSetForegroundWindow(ASFW_ANY);

                if (IsIconic(hwnd))
                    ShowWindow(hwnd, SW_RESTORE);

                SetForegroundWindow(hwnd);

                // Briefly bring to top then release topmost flag
                SetWindowPos(hwnd,
                             HWND_TOPMOST,
                             0,
                             0,
                             0,
                             0,
                             SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
                SetWindowPos(hwnd,
                             HWND_NOTOPMOST,
                             0,
                             0,
                             0,
                             0,
                             SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW);
            }
            CloseHandle(hMap);
        }
        CloseHandle(hMutex);
        return 0;
    }
#endif

    app.setApplicationName("WinSize");
    app.setApplicationVersion(WINSIZE_APP_VERSION);
    app.setWindowIcon(QIcon(":/icons/winsize.png"));

    // Load stylesheet
    QFile qss(":/styles/fluent.qss");
    if (qss.open(QFile::ReadOnly | QFile::Text))
        app.setStyleSheet(qss.readAll());

    // Load translations (first matching locale wins)
    QTranslator translator;
    const QString qmDir = QApplication::applicationDirPath() + "/translations";
    const QStringList uiLanguages = QLocale::system().uiLanguages();
    for (const QString &locale : uiLanguages) {
        const QString baseName = QLocale(locale).name();
        const QString qmName = QString("winsize_%1").arg(baseName);
        if (translator.load(qmName, qmDir)) {
            app.installTranslator(&translator);
            break;
        }
    }

    winsize::MainWindow w;

    w.show();

#ifdef _WIN32
    // Publish our HWND via shared memory so a second instance can find us
    HANDLE hMap = CreateFileMappingW(INVALID_HANDLE_VALUE,
                                     nullptr,
                                     PAGE_READWRITE,
                                     0,
                                     sizeof(HWND),
                                     kSharedMemName);
    HWND *sharedHwnd = nullptr;
    if (hMap) {
        sharedHwnd = static_cast<HWND *>(MapViewOfFile(hMap, FILE_MAP_WRITE, 0, 0, sizeof(HWND)));
        if (sharedHwnd)
            *sharedHwnd = reinterpret_cast<HWND>(w.winId());
    }
#endif

    // Start centered so the first render appears in a predictable location.
    winman::WinOps::centerWindow(static_cast<std::uintptr_t>(w.winId()));

    int ret = app.exec();

#ifdef _WIN32
    // Clean up shared memory and mutex on exit
    if (sharedHwnd)
        UnmapViewOfFile(sharedHwnd);
    if (hMap)
        CloseHandle(hMap);
    CloseHandle(hMutex);
#endif

    return ret;
}
