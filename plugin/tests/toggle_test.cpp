// SPDX-License-Identifier: GPL-2.0-or-later
#include "MainWindow.h"
#include "profile/Profile.h"
#include "pluginsystem/IKonsolePlugin.h"

#include <KPluginFactory>
#include <KPluginMetaData>
#include <QDockWidget>
#include <QTest>

class ToggleTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
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
        window.createSession({}, QString());
        auto *terminal = window.findChild<Konsole::TerminalDisplay *>();
        QVERIFY(terminal);
        plugin->createWidgetsForMainWindow(&window);
        const auto actions = plugin->menuBarActions(&window);
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
        QTest::keyClick(terminal, Qt::Key_Q, Qt::ControlModifier);
        QTRY_VERIFY(dock->isVisible());
        QVERIFY(actions.first()->isChecked());
        QTRY_VERIFY(dock->widget()->hasFocus());
        QTest::keyClick(dock->widget(), Qt::Key_Q, Qt::ControlModifier);
        QTRY_VERIFY(dock->isHidden());
        QVERIFY(!actions.first()->isChecked());
        QTRY_VERIFY(terminal->hasFocus());

        actions.first()->trigger();
        QTRY_VERIFY(dock->isVisible());
        dock->close();
        QTRY_VERIFY(dock->isHidden());
        QVERIFY(!actions.first()->isChecked());
        QTRY_VERIFY(terminal->hasFocus());
    }
};

QTEST_MAIN(ToggleTest)
#include "toggle_test.moc"
