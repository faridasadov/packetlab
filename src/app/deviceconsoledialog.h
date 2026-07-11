#pragma once

#include <QDialog>

#include "../core/networkmodel.h"
#include "../core/simulator.h"

class QTextEdit;
class QTabWidget;

namespace packetlab {

class ConsoleLineEdit;

class DeviceConsoleDialog final : public QDialog {
    Q_OBJECT
public:
    DeviceConsoleDialog(NetworkModel& model, Simulator& simulator, int deviceId, QWidget* parent = nullptr);

signals:
    void modelMutated(int deviceId);

private slots:
    void executeCliCommand();

private:
    void appendCliLine(const QString& text, const QString& color = "#e6edf3");
    QString commandPrompt() const;
    QString commandOutput(const QString& command);
    void refreshPanels();
    QStringList completionCandidates() const;
    QString webMarkup() const;
    Device* device();
    const Device* device() const;
    NetworkInterface* selectedInterface();

    NetworkModel& m_model;
    Simulator& m_simulator;
    int m_deviceId;
    bool m_inConfigMode = false;
    QString m_selectedInterfaceName;
    int m_selectedVlanId = -1;
    QString m_selectedDhcpPoolName;
    QTextEdit* m_cliOutput;
    ConsoleLineEdit* m_cliInput;
    QTextEdit* m_webView;
    QTextEdit* m_summaryView;
};

} // namespace packetlab
