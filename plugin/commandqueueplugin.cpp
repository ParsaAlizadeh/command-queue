// SPDX-License-Identifier: GPL-2.0-or-later

#include "MainWindow.h"
#include "ViewManager.h"
#include "pluginsystem/IKonsolePlugin.h"
#include "session/SessionController.h"

#include <KActionCollection>
#include <KConfigGroup>
#include <KPluginFactory>
#include <QAction>
#include <QCloseEvent>
#include <QDockWidget>
#include <QMap>
#include <QPointer>
#include <QWidget>

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

        // Intentionally empty: no editor, queue model, or command dispatch yet.
        auto *contents = new QWidget(dock);
        contents->setMinimumHeight(100);
        contents->setFocusPolicy(Qt::StrongFocus);
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
        m_actions.insert(window, toggle);
        connect(window, &QObject::destroyed, this, [this, window] { m_actions.remove(window); });
    }

    void activeViewChanged(Konsole::SessionController *, Konsole::MainWindow *) override
    {
        // There is no session-specific behavior in this first stage.
    }

    QList<QAction *> menuBarActions(Konsole::MainWindow *window) const override
    {
        if (auto *action = m_actions.value(window).data()) {
            return {action};
        }
        return {};
    }

private:
    QMap<Konsole::MainWindow *, QPointer<QAction>> m_actions;
};

K_PLUGIN_CLASS_WITH_JSON(CommandQueuePlugin, "commandqueue.json")

#include "commandqueueplugin.moc"
