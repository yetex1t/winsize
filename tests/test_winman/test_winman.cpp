#include <QtTest>

#include <winman>

class TestWinman : public QObject
{
    Q_OBJECT

private slots:
    void testWinInfoDefaults()
    {
        winman::WinInfo info;
        QCOMPARE(info.hwnd, 0u);
        QVERIFY(info.processName.empty());
        QCOMPARE(info.pid, 0u);
        QCOMPARE(info.processStartTime, 0u);
        QVERIFY(!info.isElevated);
        QVERIFY(info.windowTitle.empty());
        QCOMPARE(info.x, 0);
        QCOMPARE(info.y, 0);
        QCOMPARE(info.width, 0);
        QCOMPARE(info.height, 0);
    }

    void testEnumerateReturnsWindows()
    {
        auto windows = winman::WinOps::enumerateWindows();
        QVERIFY(!windows.empty());
        for (const auto &w : windows) {
            QVERIFY(w.hwnd != 0);
            QVERIFY(w.pid != 0);
            QVERIFY(w.width > 0);
            QVERIFY(w.height > 0);
        }
    }

    void testInvalidHandleReturnsFalse()
    {
        QVERIFY(!winman::WinOps::resizeWindow(0, 100, 100));
        QVERIFY(!winman::WinOps::centerWindow(0));
        QVERIFY(!winman::WinOps::resizeAndCenterWindow(0, 100, 100));
    }

    void testWindowRefreshInvalid()
    {
        winman::WinInfo info;
        QVERIFY(!winman::WinOps::refreshWindowInfo(info));
    }
};

QTEST_MAIN(TestWinman)
#include "test_winman.moc"
