// SPDX-License-Identifier: GPL-2.0-or-later
#include "MainWindow.h"
#include "Emulation.h"
#include "ViewManager.h"
#include "profile/Profile.h"
#include "profile/ProfileManager.h"
#include "pluginsystem/IKonsolePlugin.h"
#include "session/SessionController.h"

#include <KActionCollection>
#include <KPluginFactory>
#include <KPluginMetaData>
#include <QDockWidget>
#include <QJsonObject>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QFile>
#include <QProcess>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
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
        QCOMPARE(metadata.rawData().value(QStringLiteral("CommandQueueVersion")).toString(), QStringLiteral(EXPECTED_PLUGIN_VERSION));
        QCOMPARE(metadata.rawData().value(QStringLiteral("KonsoleVersion")).toString(), QStringLiteral(EXPECTED_KONSOLE_VERSION));
        QCOMPARE(metadata.rawData().value(QStringLiteral("KonsoleRevision")).toString().size(), 40);
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
        QTRY_VERIFY((dock->widget()->hasFocus() || dock->widget()->isAncestorOf(QApplication::focusWidget())));

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
        QTRY_VERIFY((dock->widget()->hasFocus() || dock->widget()->isAncestorOf(QApplication::focusWidget())));
        QTest::keyClick(dock->widget(), Qt::Key_Q, Qt::ControlModifier);
        QTRY_VERIFY(dock->isHidden());
        QVERIFY(!actions.first()->isChecked());
        QTRY_VERIFY(terminal->hasFocus());

        // Exercise both real split paths while the panel itself has focus.
        actions.first()->trigger();
        QTRY_VERIFY((dock->widget()->hasFocus() || dock->widget()->isAncestorOf(QApplication::focusWidget())));
        QVERIFY(QMetaObject::invokeMethod(window.viewManager(), "splitLeftRight", Qt::DirectConnection));
        QCoreApplication::processEvents();
        QCOMPARE(window.findChildren<Konsole::TerminalDisplay *>().size(), 2);
        QVERIFY(actions.first()->isChecked());
        QVERIFY(dock->isVisible());
        dock->widget()->setFocus();
        QTRY_VERIFY((dock->widget()->hasFocus() || dock->widget()->isAncestorOf(QApplication::focusWidget())));
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

    void focusedSessionsAndReadiness()
    {
        KPluginMetaData metadata;
        for (const auto &candidate : KPluginMetaData::findPlugins(QStringLiteral("konsoleplugins"))) {
            if (candidate.pluginId() == QStringLiteral("konsole_commandqueue")) metadata = candidate;
        }
        const auto result = KPluginFactory::instantiatePlugin<Konsole::IKonsolePlugin>(metadata);
        QVERIFY2(result.plugin, qPrintable(result.errorString));
        std::unique_ptr<Konsole::IKonsolePlugin> plugin(result.plugin);
        Konsole::MainWindow window;
        auto *first = window.createSession(testProfile(), QString());
        plugin->createWidgetsForMainWindow(&window);
        window.addPlugin(plugin.get());
        const auto actions = plugin->menuBarActions(&window);
        window.setPluginsActions(actions);
        auto *dock = window.findChild<QDockWidget *>(QStringLiteral("CommandQueueDock"));
        QVERIFY(dock);
        auto *contents = dock->widget();
        auto state = [contents] { return contents->property("readinessState").toString(); };
        auto marker = [](Konsole::Session *session, const QByteArray &event) {
            const auto bytes = QByteArray("\033]777;command-queue;1;") + event + '\a';
            session->emulation()->receiveData(bytes.constData(), bytes.size());
        };
        window.show();
        window.activateWindow();
        first->views().first()->setFocus();
        QTRY_COMPARE(contents->property("focusedSessionId").toInt(), first->sessionId());
        QCOMPARE(state(), QStringLiteral("unknown"));
        QVERIFY(dock->isHidden());

        // Namespace/version validation and split terminal-protocol sequences.
        const QByteArray unrelated("\033]777;command-queue;2;prompt\a");
        first->emulation()->receiveData(unrelated.constData(), unrelated.size());
        QCOMPARE(state(), QStringLiteral("unknown"));
        const QByteArray partial("\033]777;command-queue;1;pro");
        first->emulation()->receiveData(partial.constData(), partial.size());
        QCOMPARE(state(), QStringLiteral("unknown"));
        first->emulation()->receiveData("mpt\a", 4);
        QCOMPARE(state(), QStringLiteral("prompt-reported"));
        QSignalSpy input(first->emulation(), &Konsole::Emulation::sendData);
        first->emulation()->sendText(QStringLiteral("x"));
        QCOMPARE(state(), QStringLiteral("input-pending"));
        QCOMPARE(input.size(), 1);
        QCOMPARE(input.first().first().toByteArray(), QByteArray("x"));
        marker(first, "running");
        QCOMPARE(state(), QStringLiteral("running"));
        marker(first, "continuation");
        QCOMPARE(state(), QStringLiteral("continuation"));
        marker(first, "prompt");
        auto *readOnly = first->views().first()->sessionController()->actionCollection()->action(QStringLiteral("view-readonly"));
        QVERIFY(readOnly);
        readOnly->trigger();
        QCOMPARE(state(), QStringLiteral("read-only"));
        readOnly->trigger();
        QCOMPARE(state(), QStringLiteral("prompt-reported"));

        // A second tab/session has independent readiness. The panel follows
        // focus; its open state and focus are never changed by status reports.
        auto *second = window.createSession(testProfile(), QString());
        second->views().first()->setFocus();
        QTRY_COMPARE(contents->property("focusedSessionId").toInt(), second->sessionId());
        QCOMPARE(state(), QStringLiteral("unknown"));
        actions.first()->trigger();
        QTRY_VERIFY((contents->hasFocus() || contents->isAncestorOf(QApplication::focusWidget())));
        marker(second, "running");
        QCOMPARE(state(), QStringLiteral("running"));
        QVERIFY((contents->hasFocus() || contents->isAncestorOf(QApplication::focusWidget())));
        QVERIFY(dock->isVisible());
        marker(first, "continuation");
        QCOMPARE(state(), QStringLiteral("running"));
        first->views().first()->setFocus();
        QTRY_COMPARE(contents->property("focusedSessionId").toInt(), first->sessionId());
        QCOMPARE(state(), QStringLiteral("continuation"));
        QVERIFY(dock->isVisible());
        second->views().first()->setFocus();
        QTRY_COMPARE(contents->property("focusedSessionId").toInt(), second->sessionId());
        QCOMPARE(state(), QStringLiteral("running"));

        // Alternate-screen transitions cannot restore an old prompt report.
        second->emulation()->receiveData("\033[?1049h", 8);
        QCOMPARE(state(), QStringLiteral("unknown"));
        second->emulation()->receiveData("\033[?1049l", 8);
        QCOMPARE(state(), QStringLiteral("unknown"));
        const QString trackerName = QStringLiteral("CommandQueueSessionTracker_%1").arg(second->sessionId());
        second->close();
        QTRY_VERIFY(!plugin->findChild<QObject *>(trackerName));
        QTRY_COMPARE(contents->property("focusedSessionId").toInt(), first->sessionId());
        QCOMPARE(state(), QStringLiteral("continuation"));
    }

    void promptDispatch()
    {
        KPluginMetaData metadata;
        for (const auto &candidate : KPluginMetaData::findPlugins(QStringLiteral("konsoleplugins"))) {
            if (candidate.pluginId() == QStringLiteral("konsole_commandqueue")) metadata = candidate;
        }
        const auto result = KPluginFactory::instantiatePlugin<Konsole::IKonsolePlugin>(metadata);
        QVERIFY2(result.plugin, qPrintable(result.errorString));
        std::unique_ptr<Konsole::IKonsolePlugin> plugin(result.plugin);
        Konsole::MainWindow window;
        auto *session = window.createSession(testProfile(), QString());
        plugin->createWidgetsForMainWindow(&window);
        window.addPlugin(plugin.get());
        const auto actions = plugin->menuBarActions(&window);
        window.setPluginsActions(actions);
        window.show();
        window.activateWindow();
        session->views().first()->setFocus();
        actions.first()->trigger();
        auto *input = window.findChild<QLineEdit *>(QStringLiteral("CommandQueueInput"));
        auto *list = window.findChild<QListWidget *>(QStringLiteral("CommandQueueList"));
        QTRY_VERIFY(session->isRunning());
        QSignalSpy sent(session->emulation(), &Konsole::Emulation::sendData);
        QTest::keyClicks(input, QStringLiteral("first"));
        QTest::keyClick(input, Qt::Key_Return);
        QTest::keyClicks(input, QStringLiteral("second"));
        QTest::keyClick(input, Qt::Key_Return);
        QCOMPARE(sent.size(), 0); // Unknown readiness never dispatches.
        QCOMPARE(list->count(), 2);
        QTest::keyClick(input, Qt::Key_Up);
        const QByteArray prompt("\033]777;command-queue;1;prompt\a");
        session->emulation()->receiveData(prompt.constData(), prompt.size());
        QCOMPARE(sent.size(), 0); // Editing holds dispatch even at a prompt.
        QTest::keyClick(input, Qt::Key_Escape);
        QCOMPARE(sent.size(), 1); // Synchronous: no timer/event-loop wait.
        QCOMPARE(sent.at(0).first().toByteArray(), QByteArray("first\r"));
        QCOMPARE(list->count(), 1);
        session->emulation()->receiveData(prompt.constData(), prompt.size());
        QCOMPARE(sent.size(), 2);
        QCOMPARE(sent.at(1).first().toByteArray(), QByteArray("second\r"));
        QCOMPARE(list->count(), 0);
    }

    void backgroundQueue()
    {
        KPluginMetaData metadata;
        for (const auto &candidate : KPluginMetaData::findPlugins(QStringLiteral("konsoleplugins"))) {
            if (candidate.pluginId() == QStringLiteral("konsole_commandqueue")) metadata = candidate;
        }
        const auto result = KPluginFactory::instantiatePlugin<Konsole::IKonsolePlugin>(metadata);
        QVERIFY2(result.plugin, qPrintable(result.errorString));
        std::unique_ptr<Konsole::IKonsolePlugin> plugin(result.plugin);
        QTemporaryDir temporary;
        QVERIFY(temporary.isValid());
        const QString rcPath = temporary.filePath(QStringLiteral("bashrc"));
        QFile rc(rcPath);
        QVERIFY(rc.open(QIODevice::WriteOnly));
        rc.write("PS1='CQ> '; source '" PLUGIN_SOURCE_DIR "/shell/integration.bash'\n");
        rc.close();
        auto profile = testProfile();
        profile->setProperty(Konsole::Profile::Command, QStringLiteral("/usr/bin/bash"));
        profile->setProperty(Konsole::Profile::Arguments,
            QStringList{QStringLiteral("/usr/bin/bash"), QStringLiteral("--noprofile"),
                QStringLiteral("--rcfile"), rcPath, QStringLiteral("-i")});
        Konsole::MainWindow window;
        auto *first = window.createSession(profile, temporary.path());
        plugin->createWidgetsForMainWindow(&window);
        window.addPlugin(plugin.get());
        const auto actions = plugin->menuBarActions(&window);
        window.setPluginsActions(actions);
        window.show();
        window.activateWindow();
        first->views().first()->setFocus();
        auto *dock = window.findChild<QDockWidget *>(QStringLiteral("CommandQueueDock"));
        auto *input = dock->findChild<QLineEdit *>(QStringLiteral("CommandQueueInput"));
        auto *list = dock->findChild<QListWidget *>(QStringLiteral("CommandQueueList"));
        auto *remove = dock->findChild<QPushButton *>(QStringLiteral("CommandQueueDelete"));
        auto state = [dock] { return dock->widget()->property("readinessState").toString(); };
        auto enqueue = [input](const QString &command) {
            QTest::keyClicks(input, command);
            QTest::keyClick(input, Qt::Key_Return);
        };
        QTRY_COMPARE(state(), QStringLiteral("prompt-reported"));
        first->sendTextToTerminal(QStringLiteral("sleep 1"), QLatin1Char('\r'));
        QTRY_COMPARE(state(), QStringLiteral("running"));
        actions.first()->trigger();
        enqueue(QStringLiteral("cq_value=first; printf '%s\\n' \"$cq_value\" > result"));
        enqueue(QStringLiteral("printf '%s-second\\n' \"$cq_value\" >> result"));
        QCOMPARE(list->count(), 2);
        // Editing holds this session's dispatch until saved or cancelled.
        QTest::keyClick(input, Qt::Key_Up);
        QVERIFY(input->text().contains(QStringLiteral("second")));
        input->selectAll();
        QTest::keyClicks(input, QStringLiteral("printf '%s-edited\\n' \"$cq_value\" >> result"));
        QTest::keyClick(input, Qt::Key_Return);
        enqueue(QStringLiteral("echo unwanted"));
        list->setCurrentRow(2);
        remove->click();
        QCOMPARE(list->count(), 2);
        QTest::keyClicks(input, QStringLiteral("first draft"));
        auto *second = window.createSession(profile, temporary.path());
        second->views().first()->setFocus();
        QTRY_COMPARE(dock->widget()->property("focusedSessionId").toInt(), second->sessionId());
        QCOMPARE(list->count(), 0);
        QCOMPARE(input->text(), QString());
        QTRY_COMPARE(state(), QStringLiteral("prompt-reported"));
        enqueue(QStringLiteral("printf 'other\\n' > other"));
        // First session keeps executing while another tab owns the panel.
        auto readFile = [](const QString &path) {
            QFile file(path);
            return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
        };
        QTRY_COMPARE_WITH_TIMEOUT(readFile(temporary.filePath(QStringLiteral("result"))),
            QByteArray("first\nfirst-edited\n"), 5000);
        QTRY_COMPARE(readFile(temporary.filePath(QStringLiteral("other"))), QByteArray("other\n"));
        first->views().first()->setFocus();
        QTRY_COMPARE(dock->widget()->property("focusedSessionId").toInt(), first->sessionId());
        QCOMPARE(list->count(), 0);
        QCOMPARE(input->text(), QStringLiteral("first draft"));
        QTRY_COMPARE(state(), QStringLiteral("prompt-reported"));
    }

    void shellHooks_data()
    {
        QTest::addColumn<QString>("shell");
        QTest::newRow("bash") << QStringLiteral("bash");
        QTest::newRow("zsh") << QStringLiteral("zsh");
    }

    void shellHooks()
    {
        QFETCH(QString, shell);
        const auto executable = QStandardPaths::findExecutable(shell);
        if (executable.isEmpty()) QSKIP("Shell is not installed");
        QTemporaryDir home;
        QVERIFY(home.isValid());
        QByteArray output;
        QProcess process;
        auto environment = QProcessEnvironment::systemEnvironment();
        environment.insert(QStringLiteral("HOME"), home.path());
        environment.insert(QStringLiteral("ZDOTDIR"), home.path());
        process.setProcessEnvironment(environment);
        process.setProcessChannelMode(QProcess::MergedChannels);
        process.start(executable, shell == QLatin1String("bash")
            ? QStringList{QStringLiteral("--noprofile"), QStringLiteral("--norc"), QStringLiteral("-i")}
            : QStringList{QStringLiteral("-f"), QStringLiteral("-i")});
        QVERIFY(process.waitForStarted());
        connect(&process, &QProcess::readyRead, this, [&] { output += process.readAll(); });
        // Quoted shell path is controlled by the project, but escape apostrophes.
        QString path = QStringLiteral(PLUGIN_SOURCE_DIR "/shell/integration.") + shell;
        path.replace(QLatin1Char('\''), QStringLiteral("'\\''"));
        const auto source = QStringLiteral("source '%1'").arg(path).toUtf8();
        const QByteArray prompt("\033]777;command-queue;1;prompt\a");
        const QByteArray running("\033]777;command-queue;1;running\a");
        const QByteArray continuation("\033]777;command-queue;1;continuation\a");
        process.write("PS1='CQ> '; PS2='CQ_MORE> '; PS0='CQ_OLD0'; PROMPT_COMMAND=': keep_prompt_hook'\n");
        process.write(source + '\n');
        QTRY_VERIFY_WITH_TIMEOUT(output.contains(prompt), 5000);
        QVERIFY(output.contains("CQ> "));
        output.clear();
        process.write("printf '__CQ_BODY__\\n'\n");
        QTRY_VERIFY_WITH_TIMEOUT(output.contains(prompt), 5000);
        QVERIFY(output.indexOf(running) >= 0);
        QVERIFY(output.indexOf(running) < output.indexOf("__CQ_BODY__\n"));
        if (shell == QLatin1String("bash")) QVERIFY(output.contains("CQ_OLD0"));
        output.clear();
        process.write("before1=$PS1; before2=$PS2; before0=$PS0; " + source
            + "; [[ $before1 == \"$PS1\" && $before2 == \"$PS2\" && $before0 == \"$PS0\" ]] && printf '__CQ_UNCHANGED__\\n'\n");
        QTRY_VERIFY_WITH_TIMEOUT(output.contains(prompt), 5000);
        QVERIFY(output.contains("__CQ_UNCHANGED__\n"));
        output.clear();
        process.write("if true; then\n");
        QTRY_VERIFY_WITH_TIMEOUT(output.contains(continuation), 5000);
        output.clear();
        process.write(":; fi\n");
        QTRY_VERIFY_WITH_TIMEOUT(output.contains(prompt), 5000);
        process.write("exit\n");
        QVERIFY(process.waitForFinished());
    }
};

QTEST_MAIN(ToggleTest)
#include "toggle_test.moc"
