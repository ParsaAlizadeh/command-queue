// SPDX-License-Identifier: GPL-2.0-or-later
#include "MainWindow.h"
#include "ViewManager.h"
#include "profile/Profile.h"
#include "profile/ProfileManager.h"
#include "pluginsystem/IKonsolePlugin.h"

#include <KPluginFactory>
#include <KPluginMetaData>
#include <QDockWidget>
#include <QTest>

class ToggleTest : public QObject
{
    Q_OBJECT
    static Konsole::Profile::Ptr testProfile()
    {
        auto profile = Konsole::Profile::Ptr(new Konsole::Profile(Konsole::ProfileManager::instance()->defaultProfile()));
        // Splits start processes. Keep them alive without reading user shell
        // startup files or running user-defined commands.
        profile->setProperty(Konsole::Profile::Command, QStringLiteral("/usr/bin/sleep"));
        profile->setProperty(Konsole::Profile::Arguments, QStringList{QStringLiteral("/usr/bin/sleep"), QStringLiteral("600")});
        return profile;
    }
private Q_SLOTS:
    void initTestCase()
    {
        // Use Konsole's real XMLGUI resources, including layout restoration.
        QCoreApplication::setApplicationName(QStringLiteral("konsole"));
    }

    void splitWithoutPanel()
    {
        Konsole::MainWindow window;
        window.createSession(testProfile(), QString());
        window.show();
        window.activateWindow();
        auto *terminal = window.findChild<Konsole::TerminalDisplay *>();
        QVERIFY(terminal);
        terminal->setFocus();
        QTest::qWait(50);
        QVERIFY(QMetaObject::invokeMethod(window.viewManager(), "splitLeftRight", Qt::DirectConnection));
        QCoreApplication::processEvents();
        QCOMPARE(window.findChildren<Konsole::TerminalDisplay *>().size(), 2);
    }

    void discoveryAndToggle()
    {
        const auto plugins = KPluginMetaData::findPlugins(QStringLiteral("konsoleplugins"));
        KPluginMetaData metadata;
        for (const auto &candidate : plugins) {
            if (candidate.pluginId() == QStringLiteral("konsole_commandqueue")) {
                metadata = candidate;
            }
        }
        QVERIFY(metadata.isValid());
        QCOMPARE(metadata.version(), QStringLiteral(EXPECTED_KONSOLE_VERSION));
        const auto result = KPluginFactory::instantiatePlugin<Konsole::IKonsolePlugin>(metadata);
        QVERIFY2(result.plugin, qPrintable(result.errorString));
        std::unique_ptr<Konsole::IKonsolePlugin> plugin(result.plugin);

        Konsole::MainWindow window;
        window.createSession(testProfile(), QString());
        auto *terminal = window.findChild<Konsole::TerminalDisplay *>();
        QVERIFY(terminal);
        plugin->createWidgetsForMainWindow(&window);
        window.addPlugin(plugin.get());
        const auto actions = plugin->menuBarActions(&window);
        window.setPluginsActions(actions);
        QCOMPARE(actions.size(), 1);
        auto *dock = window.findChild<QDockWidget *>(QStringLiteral("CommandQueueDock"));
        QVERIFY(dock);
        QVERIFY(dock->isHidden());
        QCOMPARE(window.dockWidgetArea(dock), Qt::BottomDockWidgetArea);
        window.show();
        window.activateWindow();
        terminal->setFocus();
        QTest::qWait(50);

        // Exercise the actual shortcut from the terminal, then from the panel.
        const auto closedLayout = window.saveState();
        QTest::keyClick(terminal, Qt::Key_Q, Qt::ControlModifier);
        QTRY_VERIFY(dock->isVisible());
        QVERIFY(actions.first()->isChecked());
        QTRY_VERIFY(dock->widget()->hasFocus());

        // Layout visibility is not a user toggle and must not steal focus.
        terminal->setFocus();
        QTRY_VERIFY(terminal->hasFocus());
        QVERIFY(window.restoreState(closedLayout));
        QVERIFY(actions.first()->isChecked());
        QVERIFY(dock->isVisible());
        QVERIFY(terminal->hasFocus());
        const auto openLayout = window.saveState();
        dock->hide();
        QVERIFY(actions.first()->isChecked());
        QTRY_VERIFY(dock->isVisible());
        QTRY_VERIFY(terminal->hasFocus());
        QMetaObject::invokeMethod(dock, "visibilityChanged", Qt::DirectConnection, Q_ARG(bool, true));
        QVERIFY(terminal->hasFocus());

        dock->widget()->setFocus();
        QTRY_VERIFY(dock->widget()->hasFocus());
        QTest::keyClick(dock->widget(), Qt::Key_Q, Qt::ControlModifier);
        QTRY_VERIFY(dock->isHidden());
        QVERIFY(!actions.first()->isChecked());
        QTRY_VERIFY(terminal->hasFocus());

        // Exercise both real split paths while the panel itself has focus.
        actions.first()->trigger();
        QTRY_VERIFY(dock->widget()->hasFocus());
        QVERIFY(QMetaObject::invokeMethod(window.viewManager(), "splitLeftRight", Qt::DirectConnection));
        QCoreApplication::processEvents();
        QCOMPARE(window.findChildren<Konsole::TerminalDisplay *>().size(), 2);
        QVERIFY(actions.first()->isChecked());
        QVERIFY(dock->isVisible());
        dock->widget()->setFocus();
        QTRY_VERIFY(dock->widget()->hasFocus());
        QVERIFY(QMetaObject::invokeMethod(window.viewManager(), "splitTopBottom", Qt::DirectConnection));
        QCoreApplication::processEvents();
        QCOMPARE(window.findChildren<Konsole::TerminalDisplay *>().size(), 3);
        QVERIFY(actions.first()->isChecked());
        QVERIFY(dock->isVisible());
        actions.first()->trigger();
        QTRY_VERIFY(dock->isHidden());
        QVERIFY(!actions.first()->isChecked());
        // The offscreen platform has no window manager to reactivate the main
        // window after dock/layout changes. Restore activation, not focus.
        window.activateWindow();
        QTRY_VERIFY(qobject_cast<Konsole::TerminalDisplay *>(QApplication::focusWidget()));
        auto *currentTerminal = QApplication::focusWidget();

        // A layout restore must not reopen a panel the user has closed.
        QVERIFY(window.restoreState(openLayout));
        QVERIFY(!actions.first()->isChecked());
        QVERIFY(dock->isHidden());
        QVERIFY(currentTerminal->hasFocus());
        dock->show();
        QVERIFY(!actions.first()->isChecked());
        QTRY_VERIFY(dock->isHidden());
        QTRY_VERIFY(currentTerminal->hasFocus());

        actions.first()->trigger();
        QTRY_VERIFY(dock->isVisible());
        dock->close();
        QTRY_VERIFY(dock->isHidden());
        QVERIFY(!actions.first()->isChecked());
        QTRY_VERIFY(currentTerminal->hasFocus());
    }
};

QTEST_MAIN(ToggleTest)
#include "toggle_test.moc"
