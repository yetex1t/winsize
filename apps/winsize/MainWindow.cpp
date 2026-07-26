#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QGuiApplication>
#include <QMessageBox>
#include <QScreen>
#include <QTimer>

#include <algorithm>

#include <Windows.h>

namespace winsize {

// Check if the current process is running with administrator privileges
static bool isProcessElevated()
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token))
        return false;

    TOKEN_ELEVATION elevation{};
    DWORD outSize = 0;
    const BOOL ok = GetTokenInformation(token, TokenElevation, &elevation, sizeof(elevation), &outSize);
    CloseHandle(token);
    return ok && elevation.TokenIsElevated;
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , refreshTimer(new QTimer(this))
{
    ui->setupUi(this);

#ifdef _WIN32
    if (isProcessElevated())
        setWindowTitle(windowTitle() + tr(" (Administrator)"));
#endif

    QScreen *screenObj = QGuiApplication::primaryScreen();
    QRect screen = screenObj ? screenObj->availableGeometry() : QRect(0, 0, 1200, 800);
    // Start with a wide layout that keeps table columns readable.
    int h = screen.height() * 0.6;
    int w = h * 1.4;
    resize(w, h);

    ui->tableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    ui->tableWidget->verticalHeader()->setVisible(false);
    ui->tableWidget->setAlternatingRowColors(true);
    ui->tableWidget->setShowGrid(false);
    auto *header = ui->tableWidget->horizontalHeader();
    header->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(1, QHeaderView::Stretch);
    header->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    header->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    header->setMinimumSectionSize(60);

    connect(ui->btnRefresh, &QPushButton::clicked, this, &MainWindow::refreshWindowList);
    connect(ui->btnResize, &QPushButton::clicked, this, &MainWindow::onResizeClicked);
    connect(ui->btnCenter, &QPushButton::clicked, this, &MainWindow::onCenterClicked);
    connect(ui->btnResizeAndCenter,
            &QPushButton::clicked,
            this,
            &MainWindow::onResizeAndCenterClicked);
    connect(ui->tableWidget,
            &QTableWidget::itemSelectionChanged,
            this,
            &MainWindow::onSelectionChanged);
    connect(refreshTimer, &QTimer::timeout, this, &MainWindow::refreshWindowList);
    refreshTimer->start(2000);

    QTimer::singleShot(0, this, &MainWindow::refreshWindowList);
    onSelectionChanged();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::refreshWindowList()
{
    auto newWindowList = winman::WinOps::enumerateWindows();

    // Filter out self-window using erase-remove idiom
    const uint32_t selfPid = static_cast<uint32_t>(QCoreApplication::applicationPid());
    newWindowList.erase(std::remove_if(newWindowList.begin(),
                                       newWindowList.end(),
                                       [selfPid](const winman::WinInfo &info) { return info.pid == selfPid; }),
                        newWindowList.end());

    auto oldHwnd = selectedHwnd();

    // Check if the window list has changed
    bool listChanged = (newWindowList.size() != windowList.size());
    if (!listChanged) {
        for (size_t i = 0; i < newWindowList.size(); ++i) {
            if (newWindowList[i].hwnd != windowList[i].hwnd ||
                newWindowList[i].x != windowList[i].x ||
                newWindowList[i].y != windowList[i].y ||
                newWindowList[i].width != windowList[i].width ||
                newWindowList[i].height != windowList[i].height ||
                newWindowList[i].windowTitle != windowList[i].windowTitle ||
                newWindowList[i].isElevated != windowList[i].isElevated) {
                listChanged = true;
                break;
            }
        }
    }

    // Skip UI update if nothing changed (avoids per-cell repaints)
    if (!listChanged)
        return;

    windowList = std::move(newWindowList);

    ui->tableWidget->setRowCount(static_cast<int>(windowList.size()));

    // Populate table cells
    int restoreRow = -1;
    for (int i = 0; i < static_cast<int>(windowList.size()); ++i) {
        const auto &w = windowList[static_cast<size_t>(i)];

        auto setCell = [&](int col, const QString &text) {
            auto *item = ui->tableWidget->item(i, col);
            if (!item) {
                item = new QTableWidgetItem();
                ui->tableWidget->setItem(i, col, item);
            }
            item->setText(text);
        };

        setCell(0, QString::fromStdWString(w.processName));
        setCell(1, QString::fromStdWString(w.windowTitle));
        setCell(2, QString("%1 x %2").arg(w.width).arg(w.height));
        setCell(3, QString("(%1, %2)").arg(w.x).arg(w.y));
        setCell(4, w.isElevated ? tr("Admin") : tr("User"));

        if (w.hwnd == oldHwnd)
            restoreRow = i;
    }

    if (restoreRow >= 0) {
        // Preserve selection across periodic refreshes.
        ui->tableWidget->selectRow(restoreRow);
    } else if (!windowList.empty()) {
        ui->tableWidget->selectRow(0);
    }

    onSelectionChanged();
}

// Enable/disable action buttons based on table selection
void MainWindow::onSelectionChanged()
{
    bool hasSel = ui->tableWidget->currentRow() >= 0;
    ui->btnResize->setEnabled(hasSel);
    ui->btnCenter->setEnabled(hasSel);
    ui->btnResizeAndCenter->setEnabled(hasSel);
}

void MainWindow::onResizeClicked()
{
    int w = ui->spinWidth->value();
    int h = ui->spinHeight->value();

    runSelectedWindowAction(tr("Failed to resize the window.\nPlease run WinSize as administrator."),
                            [w, h](std::uintptr_t hwnd) {
                                return winman::WinOps::resizeWindow(hwnd, w, h);
                            });
}

void MainWindow::onCenterClicked()
{
    runSelectedWindowAction(tr("Failed to center the window.\nPlease run WinSize as administrator."),
                            [](std::uintptr_t hwnd) { return winman::WinOps::centerWindow(hwnd); });
}

void MainWindow::onResizeAndCenterClicked()
{
    int w = ui->spinWidth->value();
    int h = ui->spinHeight->value();

    runSelectedWindowAction(tr("Failed to resize and center the window."
                               "\nPlease run WinSize as administrator."),
                            [w, h](std::uintptr_t hwnd) {
                                return winman::WinOps::resizeAndCenterWindow(hwnd, w, h);
                            });
}

// Execute an action on the selected window (template to avoid std::function overhead)
template<typename Func>
bool MainWindow::runSelectedWindowAction(const QString &errorMessage, Func &&action)
{
    auto hwnd = selectedHwnd();
    if (!hwnd)
        return false;

    bool ok = action(hwnd);
    if (!ok)
        QMessageBox::warning(this, tr("Error"), errorMessage);

    refreshWindowList();
    return ok;
}

// Return the HWND of the currently selected table row, or 0 if none
std::uintptr_t MainWindow::selectedHwnd() const
{
    int row = ui->tableWidget->currentRow();
    if (row < 0 || row >= static_cast<int>(windowList.size()))
        return 0;
    return windowList[static_cast<size_t>(row)].hwnd;
}

} // namespace winsize
