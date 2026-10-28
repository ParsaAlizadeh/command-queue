// SPDX-License-Identifier: GPL-2.0-or-later

#include "MainWindow.h"
#include "ViewManager.h"
#include "pluginsystem/IKonsolePlugin.h"
#include "session/SessionController.h"
#include "sessiontracker.h"

#include <KActionCollection>
#include <KConfigGroup>
#include <KPluginFactory>
#include <QAction>
#include <QCloseEvent>
#include <QDockWidget>
#include <QLabel>
#include <QMap>
#include <QPointer>
#include <QWidget>
#include <QVBoxLayout>

// Qt may hide/show docks while restoring layouts. That is presentation, not
// the user's requested state. Do not feed Qt's toggleViewAction back into our
// own action, or move focus in response to visibilityChanged.
class CommandQueueDock final : public QDockWidget
{
    Q_OBJECT

public:
    using QDockWidget::QDockWidget;

    bool requestedOpen() const { return m_open; }

    void setRequestedOpen(bool open)
    {
        m_open = open;
        QDockWidget::setVisible(open);
    }

    void setVisible(bool visible) override
    {
        // Reject layout-restoration requests *before* they hide a focused
        // dock. Restoring it later is too late: hiding can synchronously move
        // focus and reenter Konsole's XMLGUI rebuild while it is in progress.
        if (visible == m_open) {
            QDockWidget::setVisible(visible);
        }
    }

Q_SIGNALS:
    void closedByUser();

protected:
    void closeEvent(QCloseEvent *event) override
    {
        QDockWidget::closeEvent(event);
        if (event->isAccepted()) {
            setRequestedOpen(false);
            Q_EMIT closedByUser();
        }
    }

private:
    bool m_open = false;
};

class CommandQueuePlugin final : public Konsole::IKonsolePlugin
{
    Q_OBJECT

public:
    CommandQueuePlugin(QObject *parent, const QVariantList &args)
        : IKonsolePlugin(parent, args)
    {
        setName(QStringLiteral("CommandQueue"));
    }

    void createWidgetsForMainWindow(Konsole::MainWindow *window) override
    {
        auto *dock = new CommandQueueDock(tr("Command Queue"), window);
        dock->setObjectName(QStringLiteral("CommandQueueDock"));
        dock->setAllowedAreas(Qt::BottomDockWidgetArea);

        // Status only: no editor, queue model, or command dispatch yet.
        auto *contents = new QWidget(dock);
        contents->setMinimumHeight(100);
        contents->setFocusPolicy(Qt::StrongFocus);
        auto *layout = new QVBoxLayout(contents);
        auto *status = new QLabel(tr("No active terminal"), contents);
        status->setObjectName(QStringLiteral("CommandQueueSessionStatus"));
        status->setWordWrap(true);
        layout->addWidget(status);
        layout->addStretch();
        dock->setWidget(contents);
        window->addDockWidget(Qt::BottomDockWidgetArea, dock);
        dock->hide();

        auto *toggle = new QAction(tr("Show Command Queue"), dock);
        toggle->setCheckable(true);
        window->actionCollection()->addAction(QStringLiteral("show-command-queue"), toggle);
        window->actionCollection()->setDefaultShortcut(toggle, QKeySequence(Qt::CTRL | Qt::Key_Q));
        toggle->setShortcutContext(Qt::WindowShortcut);
        window->addAction(toggle);

        const auto restoreTerminalFocus = [window] {
            if (window->isVisible()) {
                if (auto *controller = window->viewManager()->activeViewController()) {
                    if (auto view = controller->view()) {
                        view->setFocus(Qt::ShortcutFocusReason);
                    }
                }
            }
        };
        // XMLGUI reapplies KMainWindow's in-memory layout snapshot when the
        // active terminal changes. Keep it current on explicit user actions;
        // a stale snapshot can hide a focused dock during a GUI rebuild and
        // trigger a nested terminal-focus/menu rebuild inside restoreState.
        const auto rememberLayout = [window] {
            window->stateConfigGroup().writeEntry("State", window->saveState().toBase64());
        };
        connect(toggle, &QAction::triggered, dock, [dock, contents, restoreTerminalFocus, rememberLayout](bool open) {
            dock->setRequestedOpen(open);
            rememberLayout();
            if (open) {
                contents->setFocus(Qt::ShortcutFocusReason);
            } else {
                restoreTerminalFocus();
            }
        });
        connect(dock, &CommandQueueDock::closedByUser, toggle, [toggle, restoreTerminalFocus, rememberLayout] {
            toggle->setChecked(false);
            rememberLayout();
            restoreTerminalFocus();
        });
        WindowState state;
        state.action = toggle;
        state.contents = contents;
        state.status = status;
        m_windows.insert(window, state);
        connect(window, &QObject::destroyed, this, [this, window] {
            disconnect(m_windows.value(window).changedConnection);
            m_windows.remove(window);
        });
        activeViewChanged(window->viewManager()->activeViewController(), window);
    }

    void activeViewChanged(Konsole::SessionController *controller, Konsole::MainWindow *window) override
    {
        auto it = m_windows.find(window);
        if (it == m_windows.end()) {
            return;
        }
        auto session = controller ? controller->session() : QPointer<Konsole::Session>();
        SessionTracker *tracker = session ? observe(session) : nullptr;
        if (it->tracker == tracker) {
            refreshStatus(window);
            return;
        }
        disconnect(it->changedConnection);
        it->tracker = tracker;
        if (tracker) {
            it->changedConnection = connect(tracker, &SessionTracker::changed, it->contents, [this, window] {
                refreshStatus(window);
            });
        }
        refreshStatus(window);
    }

    QList<QAction *> menuBarActions(Konsole::MainWindow *window) const override
    {
        if (auto *action = m_windows.value(window).action.data()) {
            return {action};
        }
        return {};
    }

private:
    struct WindowState {
        QPointer<QAction> action;
        QPointer<QWidget> contents;
        QPointer<QLabel> status;
        QPointer<SessionTracker> tracker;
        QMetaObject::Connection changedConnection;
    };

    SessionTracker *observe(Konsole::Session *session)
    {
        if (auto tracker = m_sessions.value(session)) {
            return tracker;
        }
        auto *tracker = new SessionTracker(session, this);
        tracker->setObjectName(QStringLiteral("CommandQueueSessionTracker_%1").arg(tracker->sessionId()));
        m_sessions.insert(session, tracker);
        connect(session, &QObject::destroyed, this, [this, session, tracker] {
            m_sessions.remove(session);
            tracker->deleteLater();
        });
        return tracker;
    }

    void refreshStatus(Konsole::MainWindow *window)
    {
        auto it = m_windows.find(window);
        if (it == m_windows.end() || !it->contents || !it->status) {
            return;
        }
        const auto tracker = it->tracker;
        it->contents->setProperty("focusedSessionId", tracker ? tracker->sessionId() : -1);
        it->contents->setProperty("readinessState", tracker ? tracker->stateName() : QStringLiteral("no-session"));
        it->status->setText(tracker
            ? tr("Session %1 — %2").arg(tracker->sessionId()).arg(tracker->description())
            : tr("No active terminal"));
        // Updating status must not alter requested visibility or keyboard focus.
    }

    QMap<Konsole::MainWindow *, WindowState> m_windows;
    QMap<Konsole::Session *, QPointer<SessionTracker>> m_sessions;
};

K_PLUGIN_CLASS_WITH_JSON(CommandQueuePlugin, "commandqueue.json")

#include "commandqueueplugin.moc"
