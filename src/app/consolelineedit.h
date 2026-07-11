#pragma once

#include <QLineEdit>
#include <QStringList>

class QCompleter;

namespace packetlab {

class ConsoleLineEdit final : public QLineEdit {
    Q_OBJECT
public:
    explicit ConsoleLineEdit(QWidget* parent = nullptr);

    void setCompletionCandidates(const QStringList& values);
    void addHistoryEntry(const QString& command);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    void stepHistory(int delta);
    void applyCompletion();

    QCompleter* m_completer;
    QStringList m_history;
    int m_historyIndex;
};

} // namespace packetlab
