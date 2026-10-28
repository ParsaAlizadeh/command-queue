// SPDX-License-Identifier: MIT
#pragma once

#include <QObject>
#include <QPointer>
#include <QStringList>

namespace Konsole { class Session; }
class SessionTracker;

// Execution belongs to the session, never to the currently displayed panel.
class SessionQueue final : public QObject
{
    Q_OBJECT
public:
    SessionQueue(Konsole::Session *session, SessionTracker *tracker);
    QStringList commands() const { return m_commands; }
    QString draft() const { return m_draft; }
    int editingIndex() const { return m_editing; }
    void setDraft(const QString &text);
    bool submitDraft();
    void navigate(int direction);
    void beginEdit(int row);
    void cancelEdit();
    void remove(int row);

Q_SIGNALS:
    void changed();

private:
    void dispatch();
    bool eligible() const;
    QPointer<Konsole::Session> m_session;
    SessionTracker *m_tracker;
    QStringList m_commands;
    QString m_draft;
    QString m_savedDraft;
    int m_editing = -1;
};
