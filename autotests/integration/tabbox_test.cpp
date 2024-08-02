/*
    KWin - the KDE window manager
    This file is part of the KDE project.

    SPDX-FileCopyrightText: 2016 Martin Gräßlin <mgraesslin@kde.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/
#include "kwin_wayland_test.h"

#include "input.h"
#include "pointer_input.h"
#include "tabbox/tabbox.h"
#include "wayland_server.h"
#include "window.h"
#include "workspace.h"

#include <KConfigGroup>
#include <KWayland/Client/keyboard.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/surface.h>

#include <linux/input.h>

using namespace KWin;

static const QString s_socketName = QStringLiteral("wayland_test_kwin_tabbox-0");

class TabBoxTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();

    void testMoveForward();
    void testMoveBackward();
    void testCapsLock();
    void testKeyboardFocus();
};

void TabBoxTest::initTestCase()
{
    qRegisterMetaType<KWin::Window *>();
    QSignalSpy applicationStartedSpy(kwinApp(), &Application::started);
    QVERIFY(waylandServer()->init(s_socketName));
    Test::setOutputConfig({
        QRect(0, 0, 1280, 1024),
        QRect(1280, 0, 1280, 1024),
    });

    KSharedConfigPtr c = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    c->group(QStringLiteral("TabBox")).writeEntry("ShowTabBox", false);
    c->sync();
    kwinApp()->setConfig(c);
    qputenv("KWIN_XKB_DEFAULT_KEYMAP", "1");

    kwinApp()->start();
    QVERIFY(applicationStartedSpy.wait());
}

void TabBoxTest::init()
{
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat));
    workspace()->setActiveOutput(QPoint(640, 512));
    KWin::input()->pointer()->warp(QPoint(640, 512));
}

void TabBoxTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void TabBoxTest::testCapsLock()
{
#if !KWIN_BUILD_GLOBALSHORTCUTS
    QSKIP("Can't test shortcuts without shortcuts");
    return;
#endif

    // this test verifies that Alt+tab works correctly also when Capslock is on
    // bug 368590

    // first create three windows
    std::unique_ptr<KWayland::Client::Surface> surface1(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface1(Test::createXdgToplevelSurface(surface1.get()));
    auto c1 = Test::renderAndWaitForShown(surface1.get(), QSize(100, 50), Qt::blue);
    QVERIFY(c1);
    QVERIFY(c1->isActive());
    std::unique_ptr<KWayland::Client::Surface> surface2(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.get()));
    auto c2 = Test::renderAndWaitForShown(surface2.get(), QSize(100, 50), Qt::red);
    QVERIFY(c2);
    QVERIFY(c2->isActive());
    std::unique_ptr<KWayland::Client::Surface> surface3(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface3(Test::createXdgToplevelSurface(surface3.get()));
    auto c3 = Test::renderAndWaitForShown(surface3.get(), QSize(100, 50), Qt::red);
    QVERIFY(c3);
    QVERIFY(c3->isActive());

    // Setup tabbox signal spies
    QSignalSpy tabboxAddedSpy(workspace()->tabbox(), &TabBox::TabBox::tabBoxAdded);
    QSignalSpy tabboxClosedSpy(workspace()->tabbox(), &TabBox::TabBox::tabBoxClosed);

    // enable capslock
    quint32 timestamp = 0;
    Test::keyboardKeyPressed(KEY_CAPSLOCK, timestamp++);
    Test::keyboardKeyReleased(KEY_CAPSLOCK, timestamp++);
    QCOMPARE(input()->keyboardModifiers(), Qt::NoModifier);

    // press alt+tab
    Test::keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    QCOMPARE(input()->keyboardModifiers(), Qt::AltModifier);
    Test::keyboardKeyPressed(KEY_TAB, timestamp++);
    Test::keyboardKeyReleased(KEY_TAB, timestamp++);

    QVERIFY(tabboxAddedSpy.wait());
    QVERIFY(workspace()->tabbox()->isGrabbed());

    // release alt
    Test::keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    QCOMPARE(tabboxClosedSpy.count(), 1);
    QCOMPARE(workspace()->tabbox()->isGrabbed(), false);

    // release caps lock
    Test::keyboardKeyPressed(KEY_CAPSLOCK, timestamp++);
    Test::keyboardKeyReleased(KEY_CAPSLOCK, timestamp++);
    QCOMPARE(input()->keyboardModifiers(), Qt::NoModifier);
    QCOMPARE(tabboxClosedSpy.count(), 1);
    QCOMPARE(workspace()->tabbox()->isGrabbed(), false);
    QCOMPARE(workspace()->activeWindow(), c2);

    surface3.reset();
    QVERIFY(Test::waitForWindowClosed(c3));
    surface2.reset();
    QVERIFY(Test::waitForWindowClosed(c2));
    surface1.reset();
    QVERIFY(Test::waitForWindowClosed(c1));
}

void TabBoxTest::testMoveForward()
{
#if !KWIN_BUILD_GLOBALSHORTCUTS
    QSKIP("Can't test shortcuts without shortcuts");
    return;
#endif

    // this test verifies that Alt+tab works correctly moving forward

    // first create three windows
    std::unique_ptr<KWayland::Client::Surface> surface1(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface1(Test::createXdgToplevelSurface(surface1.get()));
    auto c1 = Test::renderAndWaitForShown(surface1.get(), QSize(100, 50), Qt::blue);
    QVERIFY(c1);
    QVERIFY(c1->isActive());
    std::unique_ptr<KWayland::Client::Surface> surface2(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.get()));
    auto c2 = Test::renderAndWaitForShown(surface2.get(), QSize(100, 50), Qt::red);
    QVERIFY(c2);
    QVERIFY(c2->isActive());
    std::unique_ptr<KWayland::Client::Surface> surface3(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface3(Test::createXdgToplevelSurface(surface3.get()));
    auto c3 = Test::renderAndWaitForShown(surface3.get(), QSize(100, 50), Qt::red);
    QVERIFY(c3);
    QVERIFY(c3->isActive());

    // Setup tabbox signal spies
    QSignalSpy tabboxAddedSpy(workspace()->tabbox(), &TabBox::TabBox::tabBoxAdded);
    QSignalSpy tabboxClosedSpy(workspace()->tabbox(), &TabBox::TabBox::tabBoxClosed);

    // press alt+tab
    quint32 timestamp = 0;
    Test::keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    QCOMPARE(input()->keyboardModifiers(), Qt::AltModifier);
    Test::keyboardKeyPressed(KEY_TAB, timestamp++);
    Test::keyboardKeyReleased(KEY_TAB, timestamp++);

    QVERIFY(tabboxAddedSpy.wait());
    QVERIFY(workspace()->tabbox()->isGrabbed());

    // release alt
    Test::keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    QCOMPARE(tabboxClosedSpy.count(), 1);
    QCOMPARE(workspace()->tabbox()->isGrabbed(), false);
    QCOMPARE(workspace()->activeWindow(), c2);

    surface3.reset();
    QVERIFY(Test::waitForWindowClosed(c3));
    surface2.reset();
    QVERIFY(Test::waitForWindowClosed(c2));
    surface1.reset();
    QVERIFY(Test::waitForWindowClosed(c1));
}

void TabBoxTest::testMoveBackward()
{
#if !KWIN_BUILD_GLOBALSHORTCUTS
    QSKIP("Can't test shortcuts without shortcuts");
    return;
#endif

    // this test verifies that Alt+Shift+tab works correctly moving backward

    // first create three windows
    std::unique_ptr<KWayland::Client::Surface> surface1(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface1(Test::createXdgToplevelSurface(surface1.get()));
    auto c1 = Test::renderAndWaitForShown(surface1.get(), QSize(100, 50), Qt::blue);
    QVERIFY(c1);
    QVERIFY(c1->isActive());
    std::unique_ptr<KWayland::Client::Surface> surface2(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface2(Test::createXdgToplevelSurface(surface2.get()));
    auto c2 = Test::renderAndWaitForShown(surface2.get(), QSize(100, 50), Qt::red);
    QVERIFY(c2);
    QVERIFY(c2->isActive());
    std::unique_ptr<KWayland::Client::Surface> surface3(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface3(Test::createXdgToplevelSurface(surface3.get()));
    auto c3 = Test::renderAndWaitForShown(surface3.get(), QSize(100, 50), Qt::red);
    QVERIFY(c3);
    QVERIFY(c3->isActive());

    // Setup tabbox signal spies
    QSignalSpy tabboxAddedSpy(workspace()->tabbox(), &TabBox::TabBox::tabBoxAdded);
    QSignalSpy tabboxClosedSpy(workspace()->tabbox(), &TabBox::TabBox::tabBoxClosed);

    // press alt+shift+tab
    quint32 timestamp = 0;
    Test::keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    QCOMPARE(input()->keyboardModifiers(), Qt::AltModifier);
    Test::keyboardKeyPressed(KEY_LEFTSHIFT, timestamp++);
    QCOMPARE(input()->keyboardModifiers(), Qt::AltModifier | Qt::ShiftModifier);
    Test::keyboardKeyPressed(KEY_TAB, timestamp++);
    Test::keyboardKeyReleased(KEY_TAB, timestamp++);

    QVERIFY(tabboxAddedSpy.wait());
    QVERIFY(workspace()->tabbox()->isGrabbed());

    // release alt
    Test::keyboardKeyReleased(KEY_LEFTSHIFT, timestamp++);
    QCOMPARE(tabboxClosedSpy.count(), 0);
    Test::keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    QCOMPARE(tabboxClosedSpy.count(), 1);
    QCOMPARE(workspace()->tabbox()->isGrabbed(), false);
    QCOMPARE(workspace()->activeWindow(), c1);

    surface3.reset();
    QVERIFY(Test::waitForWindowClosed(c3));
    surface2.reset();
    QVERIFY(Test::waitForWindowClosed(c2));
    surface1.reset();
    QVERIFY(Test::waitForWindowClosed(c1));
}

void TabBoxTest::testKeyboardFocus()
{
    // This test verifies that the keyboard focus will be withdrawn from the currently activated
    // window when the task switcher is active and restored once the task switcher is dismissed.

    QVERIFY(Test::waitForWaylandKeyboard());

    std::unique_ptr<KWayland::Client::Keyboard> keyboard(Test::waylandSeat()->createKeyboard());
    QSignalSpy enteredSpy(keyboard.get(), &KWayland::Client::Keyboard::entered);
    QSignalSpy leftSpy(keyboard.get(), &KWayland::Client::Keyboard::left);

    // add a window
    std::unique_ptr<KWayland::Client::Surface> surface(Test::createSurface());
    std::unique_ptr<Test::XdgToplevel> shellSurface(Test::createXdgToplevelSurface(surface.get()));
    Test::renderAndWaitForShown(surface.get(), QSize(100, 50), Qt::blue);

    // the keyboard focus will be moved to the surface after it's mapped
    QVERIFY(enteredSpy.wait());

    QSignalSpy tabboxAddedSpy(workspace()->tabbox(), &TabBox::TabBox::tabBoxAdded);
    QSignalSpy tabboxClosedSpy(workspace()->tabbox(), &TabBox::TabBox::tabBoxClosed);

    // press alt+tab
    quint32 timestamp = 0;
    Test::keyboardKeyPressed(KEY_LEFTALT, timestamp++);
    Test::keyboardKeyPressed(KEY_TAB, timestamp++);
    Test::keyboardKeyReleased(KEY_TAB, timestamp++);
    QVERIFY(tabboxAddedSpy.wait());

    // the surface should have no keyboard focus anymore because tabbox grabs input
    QCOMPARE(leftSpy.count(), 1);

    // release alt
    Test::keyboardKeyReleased(KEY_LEFTALT, timestamp++);
    QCOMPARE(tabboxClosedSpy.count(), 1);

    // the surface should regain keyboard focus after the tabbox is dismissed
    QVERIFY(enteredSpy.wait());
}

WAYLANDTEST_MAIN(TabBoxTest)
#include "tabbox_test.moc"
