#pragma once

#include <QMainWindow>
#include <QString>

#include <winman>

#include <vector>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class QTimer;

namespace winsize {

// Main window that lists top-level windows and runs window operations.
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    void refreshWindowList();      // Re-enumerate windows and update the table
    void onSelectionChanged();     // Enable/disable action buttons
    void onResizeClicked();        // Resize the selected window
    void onCenterClicked();        // Center the selected window
    void onResizeAndCenterClicked();
private:
    std::uintptr_t selectedHwnd() const;
    template<typename Func>
    bool runSelectedWindowAction(const QString &errorMessage, Func &&action);

    Ui::MainWindow *ui;
    QTimer *refreshTimer;                      // Periodic refresh timer
    std::vector<winman::WinInfo> windowList;   // Cached window list for diffing
};

} // namespace winsize
