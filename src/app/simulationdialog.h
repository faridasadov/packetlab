#pragma once
#include <QDialog>
#include <QStringList>

class QTextEdit;
class QTimer;
class QLabel;

namespace packetlab {

class SimulationDialog final : public QDialog {
    Q_OBJECT
public:
    explicit SimulationDialog(const QString& title, const QString& subtitle, QWidget* parent = nullptr);
    void runOutput(const QString& text);

private slots:
    void showNextLine();

private:
    QTextEdit*  m_output;
    QLabel*     m_subtitle;
    QTimer*     m_timer;
    QStringList m_lines;
    int         m_lineIndex = 0;
};

} // namespace packetlab
