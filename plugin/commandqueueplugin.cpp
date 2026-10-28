// SPDX-License-Identifier: GPL-2.0-or-later

#include "MainWindow.h"
#include "ViewManager.h"
#include "pluginsystem/IKonsolePlugin.h"
#include "session/SessionController.h"
#include "sessiontracker.h"
#include "sessionqueue.h"

#include <KActionCollection>
#include <KConfigGroup>
#include <KPluginFactory>
#include <QAction>
#include <QCloseEvent>
#include <QDockWidget>
#include <QKeyEvent>
#include <QLineEdit>
#include <QListWidget>
#include <QMap>
#include <QPointer>
#include <QPushButton>
#include <QHBoxLayout>
#include <QWidget>
#include <QVBoxLayout>

class CommandInput final : public QLineEdit
{
    Q_OBJECT
public:
    using QLineEdit::QLineEdit;
Q_SIGNALS:
    void navigate(int direction);
    void cancelEditing();
protected:
    void keyPressEvent(QKeyEvent *event) override
    {
        if (event->modifiers() == Qt::NoModifier) {
            if (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down) {
                Q_EMIT navigate(event->key() == Qt::Key_Up ? -1 : 1);
                event->accept();
                return;
            }
            if (event->key() == Qt::Key_Escape) {
                Q_EMIT cancelEditing();
                event->accept();
                return;
            }
        }
        QLineEdit::keyPressEvent(event);
    }
};

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

        auto *contents = new QWidget(dock);
        contents->setMinimumHeight(160);
        contents->setFocusPolicy(Qt::StrongFocus);
        auto *layout = new QVBoxLayout(contents);
        auto *status = new QLineEdit(tr("No active terminal"), contents);
        status->setObjectName(QStringLiteral("CommandQueueSessionStatus"));
        status->setReadOnly(true);
        layout->addWidget(status);
        auto *list = new QListWidget(contents);
        list->setObjectName(QStringLiteral("CommandQueueList"));
        list->setToolTip(tr("Double-click a queued command to edit it"));
        layout->addWidget(list);
        auto *entry = new QHBoxLayout;
        auto *input = new CommandInput(contents);
        input->setObjectName(QStringLiteral("CommandQueueInput"));
        input->setPlaceholderText(tr("Command — Enter to queue"));
        input->setToolTip(tr("Up/Down: edit queued commands. Enter: save. Escape: cancel edit."));
        auto *remove = new QPushButton(tr("Delete"), contents);
        remove->setObjectName(QStringLiteral("CommandQueueDelete"));
        entry->addWidget(input);
        entry->addWidget(remove);
        layout->addLayout(entry);
        contents->setFocusProxy(input);
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
        state.list = list;
        state.input = input;
        state.remove = remove;
        m_windows.insert(window, state);
        connect(input, &QLineEdit::textEdited, contents, [this, window](const QString &text) {
            if (auto queue = m_windows.value(window).queue) queue->setDraft(text);
        });
        connect(input, &QLineEdit::returnPressed, contents, [this, window] {
            if (auto queue = m_windows.value(window).queue) queue->submitDraft();
        });
        connect(input, &CommandInput::navigate, contents, [this, window](int direction) {
            if (auto queue = m_windows.value(window).queue) queue->navigate(direction);
        });
        connect(input, &CommandInput::cancelEditing, contents, [this, window] {
            if (auto queue = m_windows.value(window).queue) queue->cancelEdit();
        });
        connect(list, &QListWidget::itemDoubleClicked, contents, [this, window, list, input](QListWidgetItem *item) {
            if (auto queue = m_windows.value(window).queue) {
                queue->beginEdit(list->row(item));
                input->setFocus();
            }
        });
        connect(list, &QListWidget::currentRowChanged, contents, [this, window](int row) {
            auto state = m_windows.value(window);
            if (state.remove) state.remove->setEnabled(state.queue && row >= 0);
        });
        connect(remove, &QPushButton::clicked, contents, [this, window, list, input] {
            if (auto queue = m_windows.value(window).queue) queue->remove(list->currentRow());
            input->setFocus();
        });
        connect(window, &QObject::destroyed, this, [this, window] {
            disconnect(m_windows.value(window).changedConnection);
            disconnect(m_windows.value(window).queueConnection);
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
        disconnect(it->queueConnection);
        it->tracker = tracker;
        it->queue = tracker ? tracker->findChild<SessionQueue *>() : nullptr;
        it->list->clear();
        if (tracker) {
            it->changedConnection = connect(tracker, &SessionTracker::changed, it->contents, [this, window] {
                refreshStatus(window);
            });
        }
        if (it->queue) {
            it->queueConnection = connect(it->queue, &SessionQueue::changed, it->contents, [this, window] {
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
        QPointer<QLineEdit> status;
        QPointer<QListWidget> list;
        QPointer<CommandInput> input;
        QPointer<QPushButton> remove;
        QPointer<SessionTracker> tracker;
        QPointer<SessionQueue> queue;
        QMetaObject::Connection changedConnection;
        QMetaObject::Connection queueConnection;
    };

    SessionTracker *observe(Konsole::Session *session)
    {
        if (auto tracker = m_sessions.value(session)) {
            return tracker;
        }
        auto *tracker = new SessionTracker(session, this);
        new SessionQueue(session, tracker);
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
        const auto queue = it->queue;
        it->contents->setProperty("focusedSessionId", tracker ? tracker->sessionId() : -1);
        it->contents->setProperty("readinessState", tracker ? tracker->stateName() : QStringLiteral("no-session"));
        it->status->setText(tracker
            ? tr("Session %1 | %2 | %3 queued%4").arg(tracker->sessionId()).arg(tracker->stateName())
                .arg(queue ? queue->commands().size() : 0)
                .arg(queue && queue->editingIndex() >= 0 ? tr(" | editing (queue held)") : QString())
            : tr("No active terminal"));
        it->status->setToolTip(tracker ? tracker->description() : QString());
        QStringList displayed;
        for (int row = 0; row < it->list->count(); ++row) displayed.append(it->list->item(row)->text());
        const auto commands = queue ? queue->commands() : QStringList();
        if (displayed != commands) {
            const int selected = it->list->currentRow();
            it->list->clear();
            it->list->addItems(commands);
            if (selected >= 0 && !commands.isEmpty()) it->list->setCurrentRow(qMin(selected, int(commands.size()) - 1));
        }
        if (queue && queue->editingIndex() >= 0) it->list->setCurrentRow(queue->editingIndex());
        const auto draft = queue ? queue->draft() : QString();
        if (it->input->text() != draft) it->input->setText(draft);
        it->input->setPlaceholderText(queue && queue->editingIndex() >= 0
            ? tr("Editing — Enter saves; Escape cancels") : tr("Command — Enter to queue"));
        it->input->setEnabled(queue && tracker && tracker->stateName() != QLatin1String("closed"));
        it->remove->setEnabled(queue && it->list->currentRow() >= 0);
        // Updating status must not alter requested visibility or keyboard focus.
    }

    QMap<Konsole::MainWindow *, WindowState> m_windows;
    QMap<Konsole::Session *, QPointer<SessionTracker>> m_sessions;
};

K_PLUGIN_CLASS_WITH_JSON(CommandQueuePlugin, "commandqueue.json")

#include "commandqueueplugin.moc"
