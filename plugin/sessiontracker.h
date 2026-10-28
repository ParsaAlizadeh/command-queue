// SPDX-License-Identifier: MIT
#pragma once

#include <QObject>
#include <QPointer>

namespace Konsole { class Session; }

// One observer per shell session, shared by all its views. A reported prompt
// is a readiness candidate, NOT proof that the shell's editing buffer is empty.
// This observer never sends input or decides to dispatch a command.
class SessionTracker final : public QObject
{
    Q_OBJECT
public:
    enum class State { Unknown, PromptReported, InputPending, Running, Continuation, Closed };
    Q_ENUM(State)

    explicit SessionTracker(Konsole::Session *session, QObject *parent = nullptr);
    int sessionId() const { return m_sessionId; }
    QString stateName() const;
    QString description() const;
    bool promptReported() const;
    void invalidatePrompt();

Q_SIGNALS:
    void changed();

private:
    void setState(State state);
    void handleMarker(const QStringList &params);
    QPointer<Konsole::Session> m_session;
    int m_sessionId;
    State m_state = State::Unknown;
    bool m_primaryScreen = true;
};
