// SPDX-License-Identifier: GPL-2.0-or-later
#include "sessionqueue.h"
#include "sessiontracker.h"
#include "session/Session.h"

SessionQueue::SessionQueue(Konsole::Session *session, SessionTracker *tracker)
    : QObject(tracker), m_session(session), m_tracker(tracker)
{
    setObjectName(QStringLiteral("CommandQueueSessionQueue"));
    connect(tracker, &SessionTracker::changed, this, &SessionQueue::dispatch);
}

bool SessionQueue::eligible() const
{
    return m_session && m_session->isRunning() && m_tracker->promptReported()
        && m_editing < 0 && !m_commands.isEmpty();
}

void SessionQueue::dispatch()
{
    if (!eligible()) return;
    const QString command = m_commands.takeFirst();
    // Consume the readiness candidate before sending anything. Another item
    // cannot run until a subsequent prompt report. No delay or leftover-input
    // handshake is used: this first version assumes a clean shell input line.
    m_tracker->invalidatePrompt();
    Q_EMIT changed();
    m_session->sendTextToTerminal(command, QLatin1Char('\r'));
}

void SessionQueue::setDraft(const QString &text)
{
    if (m_draft == text) return;
    m_draft = text;
    Q_EMIT changed();
}

bool SessionQueue::submitDraft()
{
    // This first UI accepts single-line commands only. Preserve meaningful
    // leading/trailing spaces, including Bash history-ignore conventions.
    if (m_draft.trimmed().isEmpty() || m_draft.contains(QChar::Null)
        || m_draft.contains(QLatin1Char('\n')) || m_draft.contains(QLatin1Char('\r'))) return false;
    if (m_editing >= 0) {
        m_commands[m_editing] = m_draft;
        m_editing = -1;
        m_draft = m_savedDraft;
        m_savedDraft.clear();
    } else {
        m_commands.append(m_draft);
        m_draft.clear();
    }
    Q_EMIT changed();
    dispatch();
    return true;
}

void SessionQueue::beginEdit(int row)
{
    if (row < 0 || row >= m_commands.size()) return;
    if (m_editing < 0) m_savedDraft = m_draft;
    m_editing = row;
    m_draft = m_commands[row];
    Q_EMIT changed();
}

void SessionQueue::navigate(int direction)
{
    if (m_commands.isEmpty()) return;
    const int target = m_editing < 0 ? (direction < 0 ? m_commands.size() - 1 : -1) : m_editing + direction;
    if (target >= m_commands.size()) cancelEdit();
    else if (target >= 0) beginEdit(target);
}

void SessionQueue::cancelEdit()
{
    if (m_editing < 0) return;
    m_editing = -1;
    m_draft = m_savedDraft;
    m_savedDraft.clear();
    Q_EMIT changed();
    dispatch();
}

void SessionQueue::remove(int row)
{
    if (row < 0 || row >= m_commands.size()) return;
    m_commands.removeAt(row);
    if (m_editing == row) {
        m_editing = -1;
        m_draft = m_savedDraft;
        m_savedDraft.clear();
    } else if (m_editing > row) {
        --m_editing;
    }
    Q_EMIT changed();
    dispatch();
}
