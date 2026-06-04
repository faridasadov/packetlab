#pragma once

#include <QByteArray>
#include <QList>
#include <QMainWindow>

#include "../core/networkmodel.h"
#include "../core/simulator.h"

class QListWidget;
class QGraphicsView;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QTableWidget;
class QTextEdit;
class QWidget;
class QListWidgetItem;
class QAction;

namespace packetlab {

class TopologyScene;

class MainWindow final : public QMainWindow {
    Q_OBJECT

public:
    MainWindow();

private slots:
    void addPc();
    void addSwitch();
    void addRouter();
    void runSimulationTick();
    void saveProject();
    void loadProject();
    void syncSelectedDeviceDetails();
    void applyDeviceConfiguration();
    void createLink();
    void runPing();
    void syncLinkCreationControls();
    void loadDemoProject();
    void handleSceneDeviceSelected(int deviceId);
    void handleSceneDeviceMoved(int deviceId, QPointF position);
    void handleSceneDeviceMoveCommitted(int deviceId, QPointF position);
    void handleSceneCableRequested(int leftDeviceId, int leftInterfaceIndex, int rightDeviceId, int rightInterfaceIndex);
    void handleSceneLinkSelected(int linkId);
    void showDeviceContextMenu(int deviceId, QPointF screenPos);
    void showLinkContextMenu(int linkId, QPointF screenPos);
    void showCanvasContextMenu(QPointF screenPos);
    void deleteSelection();
    void undo();
    void redo();

private:
    void rebuildDeviceList();
    void rebuildTopology();
    void appendLog(const QString& message);
    void refreshDeviceDetails(const Device* device);
    QWidget* buildSidebarCard(const QString& title, QWidget* content);
    void applyVisualStyle();
    Device* selectedDevice();
    const Device* selectedDevice() const;
    void rebuildLinkInterfaceChoices(QComboBox* deviceCombo, QComboBox* interfaceCombo) const;
    void selectDeviceById(int deviceId);
    bool tryCreateLink(int leftDeviceId, int leftInterfaceIndex, int rightDeviceId, int rightInterfaceIndex, bool showMessages);
    void pushHistorySnapshot();
    bool restoreSnapshot(const QByteArray& snapshot);
    void refreshActionStates();

    NetworkModel m_model;
    Simulator m_simulator;
    int m_selectedLinkId;
    bool m_restoringHistory;
    QList<QByteArray> m_undoStack;
    QList<QByteArray> m_redoStack;
    QListWidget* m_deviceList;
    QGraphicsView* m_topologyView;
    TopologyScene* m_topologyScene;
    QTextEdit* m_logView;
    QLabel* m_deviceDetails;
    QLabel* m_deviceSummary;
    QLineEdit* m_nameEdit;
    QLineEdit* m_ipEdit;
    QLineEdit* m_maskEdit;
    QLineEdit* m_gatewayEdit;
    QComboBox* m_interfaceCombo;
    QLineEdit* m_interfaceIpEdit;
    QLineEdit* m_interfaceMaskEdit;
    QTableWidget* m_interfaceTable;
    QComboBox* m_linkLeftDeviceCombo;
    QComboBox* m_linkLeftInterfaceCombo;
    QComboBox* m_linkRightDeviceCombo;
    QComboBox* m_linkRightInterfaceCombo;
    QLineEdit* m_pingTargetEdit;
    QAction* m_undoAction;
    QAction* m_redoAction;
};

}  // namespace packetlab
