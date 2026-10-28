// SPDX-License-Identifier: GPL-2.0-or-later

#include "MainWindow.h"
#include "pluginsystem/IKonsolePlugin.h"

#include <KActionCollection>
#include <KPluginFactory>
#include <QAction>
#include <QDockWidget>
#include <QMap>
#include <QPointer>
#include <QWidget>

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
        auto *dock = new QDockWidget(tr("Command Queue"), window);
        dock->setObjectName(QStringLiteral("CommandQueueDock"));
        dock->setAllowedAreas(Qt::BottomDockWidgetArea);

        // Intentionally empty: no editor, queue model, or command dispatch yet.
        auto *contents = new QWidget(dock);
        contents->setMinimumHeight(100);
        contents->setFocusPolicy(Qt::StrongFocus);
        dock->setWidget(contents);
        window->addDockWidget(Qt::BottomDockWidgetArea, dock);
        dock->hide();

        auto *toggle = dock->toggleViewAction();
        toggle->setText(tr("Show Command Queue"));
        window->actionCollection()->addAction(QStringLiteral("show-command-queue"), toggle);
        window->actionCollection()->setDefaultShortcut(toggle, QKeySequence(Qt::CTRL | Qt::Key_Q));
        toggle->setShortcutContext(Qt::WindowShortcut);
        window->addAction(toggle);

        connect(dock, &QDockWidget::visibilityChanged, window, [window, contents](bool visible) {
            if (visible) {
                contents->setFocus(Qt::ShortcutFocusReason);
            } else {
                window->setFocus(); // Konsole's helper focuses the active terminal.
            }
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
