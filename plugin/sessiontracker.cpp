// SPDX-License-Identifier: MIT
#include "sessiontracker.h"
#include "Emulation.h"
#include "session/Session.h"

SessionTracker::SessionTracker(Konsole::Session *session, QObject *parent)
    : QObject(parent), m_session(session), m_sessionId(session->sessionId())
{
    m_primaryScreen = session->isPrimaryScreen();
    auto *emulation = session->emulation();
    connect(emulation, &Konsole::Emulation::osc777Received, this, &SessionTracker::handleMarker);
    connect(emulation, &Konsole::Emulation::sendData, this, [this](const QByteArray &bytes) {
        // This is observation only; bytes continue along Konsole's existing
        // PTY connection. Conservatively count terminal replies as input too.
        if (!bytes.isEmpty() && m_state != State::Unknown && m_state != State::Closed) {
            setState(State::InputPending);
        }
    });
    connect(emulation, &Konsole::Emulation::primaryScreenInUse, this, [this](bool primary) {
        m_primaryScreen = primary;
        // Exiting an interactive screen doesn't establish shell readiness.
        setState(State::Unknown);
        Q_EMIT changed();
    });
    connect(session, &Konsole::Session::readOnlyChanged, this, &SessionTracker::changed);
    connect(session, &Konsole::Session::started, this, [this] { setState(State::Unknown); });
    connect(session, &Konsole::Session::finished, this, [this] { setState(State::Closed); });
    connect(session, &QObject::destroyed, this, [this] { setState(State::Closed); });
}

void SessionTracker::setState(State state)
{
    if (m_state != state) {
        m_state = state;
        Q_EMIT changed();
    }
}

void SessionTracker::handleMarker(const QStringList &params)
{
    if (m_state == State::Closed || params.size() != 3
        || params[0] != QLatin1String("command-queue") || params[1] != QLatin1String("1")) {
        return;
    }
    if (params[2] == QLatin1String("prompt")) {
        setState(State::PromptReported);
    } else if (params[2] == QLatin1String("running")) {
        setState(State::Running);
    } else if (params[2] == QLatin1String("continuation")) {
        setState(State::Continuation);
    }
}

bool SessionTracker::promptReported() const
{
    return m_state == State::PromptReported && m_primaryScreen
        && m_session && !m_session->isReadOnly();
}

void SessionTracker::invalidatePrompt()
{
    if (m_state != State::Closed) setState(State::InputPending);
}

QString SessionTracker::stateName() const
{
    if (m_state == State::Closed) return QStringLiteral("closed");
    if (m_session && m_session->isReadOnly()) return QStringLiteral("read-only");
    // if (!m_primaryScreen) return QStringLiteral("interactive-screen");
    switch (m_state) {
    case State::Unknown: return QStringLiteral("unknown");
    case State::PromptReported: return QStringLiteral("prompt-reported");
    case State::InputPending: return QStringLiteral("input-pending");
    case State::Running: return QStringLiteral("running");
    case State::Continuation: return QStringLiteral("continuation");
    case State::Closed: break;
    }
    return QStringLiteral("unknown");
}

QString SessionTracker::description() const
{
    const auto state = stateName();
    if (state == QLatin1String("closed")) return tr("Session closed");
    if (state == QLatin1String("read-only")) return tr("Terminal is read-only");
    if (state == QLatin1String("interactive-screen")) return tr("Interactive application screen");
    if (state == QLatin1String("prompt-reported")) return tr("Prompt reported — input buffer not verified");
    if (state == QLatin1String("input-pending")) return tr("Input sent — waiting for a new shell report");
    if (state == QLatin1String("running")) return tr("Command running");
    if (state == QLatin1String("continuation")) return tr("Shell is waiting for continuation input");
    return tr("Readiness unknown — shell reports not observed");
}
