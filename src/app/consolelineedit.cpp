#include "consolelineedit.h"

#include <QCompleter>
#include <QKeyEvent>
#include <QStringListModel>

namespace packetlab {

ConsoleLineEdit::ConsoleLineEdit(QWidget* parent)
    : QLineEdit(parent),
      m_completer(new QCompleter(this)),
      m_historyIndex(-1) {
    auto* model = new QStringListModel(this);
    m_completer->setModel(model);
    m_completer->setCaseSensitivity(Qt::CaseInsensitive);
    m_completer->setFilterMode(Qt::MatchStartsWith);
    setCompleter(m_completer);
}

void ConsoleLineEdit::setCompletionCandidates(const QStringList& values) {
    if (auto* model = qobject_cast<QStringListModel*>(m_completer->model())) {
        model->setStringList(values);
    }
}

void ConsoleLineEdit::addHistoryEntry(const QString& command) {
    if (command.isEmpty()) {
        return;
    }
    if (m_history.isEmpty() || m_history.back() != command) {
        m_history.push_back(command);
    }
    m_historyIndex = -1;
}

void ConsoleLineEdit::keyPressEvent(QKeyEvent* event) {
    switch (event->key()) {
    case Qt::Key_Up:
        stepHistory(-1);
        return;
    case Qt::Key_Down:
        stepHistory(1);
        return;
    case Qt::Key_Tab:
        applyCompletion();
        return;
    default:
        break;
    }
    QLineEdit::keyPressEvent(event);
}

void ConsoleLineEdit::stepHistory(int delta) {
    if (m_history.isEmpty()) {
        return;
    }
    if (m_historyIndex < 0) {
        m_historyIndex = m_history.size();
    }
    m_historyIndex = qBound(0, m_historyIndex + delta, m_history.size());
    if (m_historyIndex >= 0 && m_historyIndex < m_history.size()) {
        setText(m_history.at(m_historyIndex));
    } else if (m_historyIndex == m_history.size()) {
        clear();
    }
}

void ConsoleLineEdit::applyCompletion() {
    const QString current = text().trimmed();
    if (current.isEmpty()) {
        return;
    }
    m_completer->setCompletionPrefix(current);
    const QString completion = m_completer->currentCompletion();
    if (!completion.isEmpty()) {
        setText(completion);
        setCursorPosition(completion.size());
    }
}

} // namespace packetlab
