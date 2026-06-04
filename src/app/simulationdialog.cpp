#include "simulationdialog.h"

#include <QClipboard>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QTextEdit>
#include <QTimer>
#include <QVBoxLayout>

namespace packetlab {

SimulationDialog::SimulationDialog(const QString& title, const QString& subtitle, QWidget* parent)
    : QDialog(parent, Qt::Window),
      m_output(new QTextEdit(this)),
      m_subtitle(new QLabel(subtitle, this)),
      m_timer(new QTimer(this)),
      m_lineIndex(0) {
    setWindowTitle(title);
    setAttribute(Qt::WA_DeleteOnClose, true);
    setMinimumSize(680, 480);
    resize(720, 520);

    setStyleSheet(R"(
        QDialog {
            background: #161b22;
        }
        QLabel#subtitleLabel {
            background: #21262d;
            color: #8b949e;
            font-family: "Consolas", "Courier New", monospace;
            font-size: 11px;
            padding: 6px 12px;
            border-bottom: 1px solid #30363d;
        }
        QTextEdit {
            background: #0d1117;
            color: #e6edf3;
            font-family: "Consolas", "Courier New", monospace;
            font-size: 12px;
            border: none;
            padding: 12px;
            selection-background-color: #264f78;
        }
        QPushButton {
            background: #21262d;
            color: #e6edf3;
            border: 1px solid #30363d;
            border-radius: 6px;
            padding: 6px 16px;
            font-size: 12px;
        }
        QPushButton:hover {
            background: #30363d;
        }
    )");

    m_subtitle->setObjectName("subtitleLabel");
    m_output->setReadOnly(true);

    auto* copyBtn  = new QPushButton("Copy",  this);
    auto* closeBtn = new QPushButton("Close", this);
    auto* btnLayout = new QHBoxLayout();
    btnLayout->addStretch();
    btnLayout->addWidget(copyBtn);
    btnLayout->addWidget(closeBtn);

    auto* mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->setSpacing(0);
    mainLayout->addWidget(m_subtitle);
    mainLayout->addWidget(m_output, 1);
    auto* btnWidget = new QWidget(this);
    btnWidget->setStyleSheet("QWidget { background: #161b22; padding: 8px; }");
    btnWidget->setLayout(btnLayout);
    mainLayout->addWidget(btnWidget);

    connect(copyBtn,  &QPushButton::clicked, this, [this]() {
        QGuiApplication::clipboard()->setText(m_output->toPlainText());
    });
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
    connect(m_timer,  &QTimer::timeout,      this, &SimulationDialog::showNextLine);
}

void SimulationDialog::runOutput(const QString& text) {
    m_lines     = text.split('\n');
    m_lineIndex = 0;
    m_output->clear();
    m_timer->start(35);
}

void SimulationDialog::showNextLine() {
    if (m_lineIndex >= m_lines.size()) {
        m_timer->stop();
        return;
    }

    const QString& line = m_lines[m_lineIndex++];

    // Determine color
    QString color;
    if (line.contains("Reply") || line.contains(" OK") || line.contains("✓") || line.contains("●"))
        color = "#4ade80";
    else if (line.contains("FAILED") || line.contains("failed") || line.contains("timeout") || line.contains("✗"))
        color = "#f87171";
    else if (line.contains("* * *") || line.contains("unreachable") || line.contains("○"))
        color = "#fbbf24";
    else if (!line.isEmpty() && (line[0] == QChar(0x2550) || line[0] == QChar(0x2500))) // ═ or ─
        color = "#475569";
    else
        color = "#e6edf3";

    // Escape HTML
    QString escaped = line.toHtmlEscaped();
    m_output->append(QString("<span style='color:%1'>%2</span>").arg(color, escaped));
}

} // namespace packetlab
