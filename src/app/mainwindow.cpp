#include "mainwindow.h"

#include "topologyscene.h"

#include <QAction>
#include <QCoreApplication>
#include <QComboBox>
#include <QFileDialog>
#include <QFrame>
#include <QFormLayout>
#include <QGraphicsView>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSplitter>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>
#include <QLineEdit>
#include <QTextEdit>
#include <QSignalBlocker>

#include "../core/projectserializer.h"

namespace packetlab {

MainWindow::MainWindow()
    : m_simulator(m_model),
      m_selectedLinkId(-1),
      m_restoringHistory(false),
      m_deviceList(new QListWidget(this)),
      m_topologyView(new QGraphicsView(this)),
      m_topologyScene(new TopologyScene(this)),
      m_logView(new QTextEdit(this)),
      m_deviceDetails(new QLabel(this)),
      m_nameEdit(new QLineEdit(this)),
      m_ipEdit(new QLineEdit(this)),
      m_maskEdit(new QLineEdit(this)),
      m_gatewayEdit(new QLineEdit(this)),
      m_interfaceCombo(new QComboBox(this)),
      m_interfaceIpEdit(new QLineEdit(this)),
      m_interfaceMaskEdit(new QLineEdit(this)),
      m_linkLeftDeviceCombo(new QComboBox(this)),
      m_linkLeftInterfaceCombo(new QComboBox(this)),
      m_linkRightDeviceCombo(new QComboBox(this)),
      m_linkRightInterfaceCombo(new QComboBox(this)),
      m_pingTargetEdit(new QLineEdit(this)),
      m_undoAction(nullptr),
      m_redoAction(nullptr) {
    setWindowTitle("PacketLab");
    resize(1480, 920);
    applyVisualStyle();

    m_model.seedDemoTopology();

    auto* toolbar = addToolBar("Devices");
    toolbar->setObjectName("topToolbar");
    m_undoAction = toolbar->addAction("Undo", this, &MainWindow::undo);
    m_redoAction = toolbar->addAction("Redo", this, &MainWindow::redo);
    toolbar->addSeparator();
    toolbar->addAction("Load", this, &MainWindow::loadProject);
    toolbar->addAction("Save", this, &MainWindow::saveProject);
    toolbar->addAction("Demo", this, &MainWindow::loadDemoProject);
    toolbar->addAction("Delete Selected", this, &MainWindow::deleteSelection);
    toolbar->addSeparator();
    toolbar->addAction("Add PC", this, &MainWindow::addPc);
    toolbar->addAction("Add Switch", this, &MainWindow::addSwitch);
    toolbar->addAction("Add Router", this, &MainWindow::addRouter);
    toolbar->addSeparator();
    toolbar->addAction("Run Tick", this, &MainWindow::runSimulationTick);
    toolbar->setMovable(false);

    m_topologyView->setScene(m_topologyScene);
    m_topologyView->setRenderHint(QPainter::Antialiasing, true);
    m_topologyView->setDragMode(QGraphicsView::ScrollHandDrag);
    m_topologyView->setFrameShape(QFrame::NoFrame);
    m_topologyView->setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);

    auto* detailsWidget = new QWidget(this);
    auto* detailsLayout = new QVBoxLayout(detailsWidget);
    auto* detailsForm = new QFormLayout();
    detailsForm->addRow("Name", m_nameEdit);
    detailsForm->addRow("Device IP", m_ipEdit);
    detailsForm->addRow("Subnet Mask", m_maskEdit);
    detailsForm->addRow("Gateway", m_gatewayEdit);
    detailsForm->addRow("Interface", m_interfaceCombo);
    detailsForm->addRow("IF IP", m_interfaceIpEdit);
    detailsForm->addRow("IF Mask", m_interfaceMaskEdit);
    detailsLayout->addLayout(detailsForm);
    auto* applyButton = new QPushButton("Apply Device Config", detailsWidget);
    detailsLayout->addWidget(applyButton);
    m_deviceDetails->setWordWrap(true);
    detailsLayout->addWidget(m_deviceDetails);

    auto* linksWidget = new QWidget(this);
    auto* linksLayout = new QVBoxLayout(linksWidget);
    auto* linkForm = new QFormLayout();
    linkForm->addRow("Left Device", m_linkLeftDeviceCombo);
    linkForm->addRow("Left Port", m_linkLeftInterfaceCombo);
    linkForm->addRow("Right Device", m_linkRightDeviceCombo);
    linkForm->addRow("Right Port", m_linkRightInterfaceCombo);
    linksLayout->addLayout(linkForm);
    auto* createLinkButton = new QPushButton("Create Link", linksWidget);
    linksLayout->addWidget(createLinkButton);
    m_pingTargetEdit->setPlaceholderText("Target IP, e.g. 192.168.10.1");
    linksLayout->addWidget(m_pingTargetEdit);
    auto* pingButton = new QPushButton("Run Ping From Selected Device", linksWidget);
    linksLayout->addWidget(pingButton);

    m_logView->setReadOnly(true);

    auto* leftPanel = new QWidget(this);
    leftPanel->setObjectName("sidebarPanel");
    auto* leftLayout = new QVBoxLayout(leftPanel);
    auto* leftTitle = new QLabel("PacketLab", leftPanel);
    leftTitle->setObjectName("appTitle");
    auto* leftSubtitle = new QLabel("Devices, topology inventory and quick exam-lab actions.", leftPanel);
    leftSubtitle->setWordWrap(true);
    leftSubtitle->setObjectName("mutedLabel");
    leftLayout->addWidget(leftTitle);
    leftLayout->addWidget(leftSubtitle);
    leftLayout->addWidget(buildSidebarCard("Device Inventory", m_deviceList));
    leftLayout->addStretch();

    auto* scenePanel = new QWidget(this);
    scenePanel->setObjectName("scenePanel");
    auto* sceneLayout = new QVBoxLayout(scenePanel);
    auto* sceneTitle = new QLabel("Topology Workspace", scenePanel);
    sceneTitle->setObjectName("panelTitle");
    auto* sceneHint = new QLabel("Select devices, wire ports, tune interface IPs and run quick connectivity checks.", scenePanel);
    sceneHint->setWordWrap(true);
    sceneHint->setObjectName("mutedLabel");
    sceneLayout->addWidget(sceneTitle);
    sceneLayout->addWidget(sceneHint);
    sceneLayout->addWidget(m_topologyView, 1);

    auto* rightPanel = new QWidget(this);
    rightPanel->setObjectName("sidebarPanel");
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->addWidget(buildSidebarCard("Inspector", detailsWidget));
    rightLayout->addWidget(buildSidebarCard("Links & Ping", linksWidget));
    rightLayout->addWidget(buildSidebarCard("Simulation Log", m_logView), 1);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(leftPanel);
    splitter->addWidget(scenePanel);
    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);
    splitter->setSizes({290, 860, 360});
    setCentralWidget(splitter);

    connect(m_deviceList, &QListWidget::currentRowChanged, this, &MainWindow::syncSelectedDeviceDetails);
    connect(m_topologyScene, &TopologyScene::deviceSelected, this, &MainWindow::handleSceneDeviceSelected);
    connect(m_topologyScene, &TopologyScene::deviceMoved, this, &MainWindow::handleSceneDeviceMoved);
    connect(m_topologyScene, &TopologyScene::cableRequested, this, &MainWindow::handleSceneCableRequested);
    connect(m_topologyScene, &TopologyScene::linkSelected, this, &MainWindow::handleSceneLinkSelected);
    connect(m_topologyScene, &TopologyScene::deviceContextMenuRequested, this, &MainWindow::showDeviceContextMenu);
    connect(m_topologyScene, &TopologyScene::linkContextMenuRequested, this, &MainWindow::showLinkContextMenu);
    connect(m_topologyScene, &TopologyScene::canvasContextMenuRequested, this, &MainWindow::showCanvasContextMenu);
    connect(applyButton, &QPushButton::clicked, this, &MainWindow::applyDeviceConfiguration);
    connect(createLinkButton, &QPushButton::clicked, this, &MainWindow::createLink);
    connect(pingButton, &QPushButton::clicked, this, &MainWindow::runPing);
    connect(m_interfaceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::syncSelectedDeviceDetails);
    connect(m_linkLeftDeviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::syncLinkCreationControls);
    connect(m_linkRightDeviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::syncLinkCreationControls);

    rebuildDeviceList();
    rebuildTopology();
    appendLog("PacketLab initialized.");
    syncSelectedDeviceDetails();
    syncLinkCreationControls();
    pushHistorySnapshot();
    refreshActionStates();
}

void MainWindow::addPc() {
    m_model.addDevice(DeviceType::Pc, "PC");
    rebuildDeviceList();
    rebuildTopology();
    pushHistorySnapshot();
    appendLog("Added PC device.");
}

void MainWindow::addSwitch() {
    m_model.addDevice(DeviceType::Switch, "SW");
    rebuildDeviceList();
    rebuildTopology();
    pushHistorySnapshot();
    appendLog("Added switch device.");
}

void MainWindow::addRouter() {
    m_model.addDevice(DeviceType::Router, "R");
    rebuildDeviceList();
    rebuildTopology();
    pushHistorySnapshot();
    appendLog("Added router device.");
}

void MainWindow::runSimulationTick() {
    appendLog(m_simulator.runTick());
}

void MainWindow::applyDeviceConfiguration() {
    auto* device = selectedDevice();
    if (!device) {
        return;
    }

    device->setName(m_nameEdit->text().trimmed().isEmpty() ? device->name() : m_nameEdit->text().trimmed());
    device->setIpAddress(m_ipEdit->text().trimmed());
    device->setSubnetMask(m_maskEdit->text().trimmed());
    device->setDefaultGateway(m_gatewayEdit->text().trimmed());

    if (auto* iface = device->interfaceAt(m_interfaceCombo->currentIndex())) {
        iface->setIpAddress(m_interfaceIpEdit->text().trimmed());
        iface->setSubnetMask(m_interfaceMaskEdit->text().trimmed());
    }

    const int deviceId = device->id();
    rebuildDeviceList();
    rebuildTopology();
    selectDeviceById(deviceId);
    syncLinkCreationControls();
    pushHistorySnapshot();
    appendLog(QString("Updated configuration for %1.").arg(device->name()));
}

void MainWindow::saveProject() {
    const QString filePath = QFileDialog::getSaveFileName(
        this,
        "Save PacketLab Project",
        QString(),
        "PacketLab Project (*.json)");
    if (filePath.isEmpty()) {
        return;
    }

    QString error;
    if (!ProjectSerializer::save(m_model, filePath, &error)) {
        QMessageBox::critical(this, "Save Failed", error);
        return;
    }

    appendLog(QString("Project saved: %1").arg(filePath));
}

void MainWindow::loadProject() {
    const QString filePath = QFileDialog::getOpenFileName(
        this,
        "Load PacketLab Project",
        QString(),
        "PacketLab Project (*.json)");
    if (filePath.isEmpty()) {
        return;
    }

    QString error;
    if (!ProjectSerializer::load(m_model, filePath, &error)) {
        QMessageBox::critical(this, "Load Failed", error);
        return;
    }

    rebuildDeviceList();
    rebuildTopology();
    appendLog(QString("Project loaded: %1").arg(filePath));
    syncSelectedDeviceDetails();
    pushHistorySnapshot();
    refreshActionStates();
}

void MainWindow::loadDemoProject() {
    const QString filePath = QCoreApplication::applicationDirPath() + "/../assets/demo-project.json";
    QString error;
    if (!ProjectSerializer::load(m_model, filePath, &error)) {
        QMessageBox::critical(this, "Demo Load Failed", error);
        return;
    }

    rebuildDeviceList();
    rebuildTopology();
    appendLog("Loaded bundled demo topology.");
    syncSelectedDeviceDetails();
    pushHistorySnapshot();
    refreshActionStates();
}

void MainWindow::handleSceneDeviceSelected(int deviceId) {
    m_selectedLinkId = -1;
    selectDeviceById(deviceId);
}

void MainWindow::handleSceneDeviceMoved(int deviceId, QPointF position) {
    if (auto* device = m_model.findDevice(deviceId)) {
        device->setPosition(position);
        selectDeviceById(deviceId);
        refreshDeviceDetails(device);
    }
}

void MainWindow::handleSceneCableRequested(int leftDeviceId, int leftInterfaceIndex, int rightDeviceId, int rightInterfaceIndex) {
    tryCreateLink(leftDeviceId, leftInterfaceIndex, rightDeviceId, rightInterfaceIndex, true);
}

void MainWindow::handleSceneLinkSelected(int linkId) {
    m_selectedLinkId = linkId;
    m_deviceList->clearSelection();
    m_topologyScene->setSelectedDeviceId(-1);
    m_deviceDetails->setText(QString("Selected link #%1\nPress Delete Selected to remove it.").arg(linkId));
}

void MainWindow::showDeviceContextMenu(int deviceId, QPointF screenPos) {
    selectDeviceById(deviceId);
    QMenu menu(this);
    menu.addAction("Delete Device", this, &MainWindow::deleteSelection);
    menu.addAction("Run Ping", this, &MainWindow::runPing);
    menu.exec(screenPos.toPoint());
}

void MainWindow::showLinkContextMenu(int linkId, QPointF screenPos) {
    handleSceneLinkSelected(linkId);
    QMenu menu(this);
    menu.addAction("Delete Link", this, &MainWindow::deleteSelection);
    menu.exec(screenPos.toPoint());
}

void MainWindow::showCanvasContextMenu(QPointF screenPos) {
    QMenu menu(this);
    menu.addAction("Add PC", this, &MainWindow::addPc);
    menu.addAction("Add Switch", this, &MainWindow::addSwitch);
    menu.addAction("Add Router", this, &MainWindow::addRouter);
    menu.exec(screenPos.toPoint());
}

void MainWindow::deleteSelection() {
    if (m_selectedLinkId >= 0) {
        const int linkId = m_selectedLinkId;
        if (m_model.removeLink(linkId)) {
            m_selectedLinkId = -1;
            rebuildTopology();
            syncLinkCreationControls();
            pushHistorySnapshot();
            refreshActionStates();
            appendLog(QString("Deleted link #%1").arg(linkId));
        }
        return;
    }

    if (auto* device = selectedDevice()) {
        const QString name = device->name();
        const int id = device->id();
        if (m_model.removeDevice(id)) {
            rebuildDeviceList();
            rebuildTopology();
            syncSelectedDeviceDetails();
            pushHistorySnapshot();
            refreshActionStates();
            appendLog(QString("Deleted device %1").arg(name));
        }
    }
}

void MainWindow::undo() {
    if (m_undoStack.size() <= 1) {
        return;
    }

    const QByteArray current = m_undoStack.takeLast();
    m_redoStack.push_back(current);
    restoreSnapshot(m_undoStack.last());
    refreshActionStates();
    appendLog("Undo applied.");
}

void MainWindow::redo() {
    if (m_redoStack.isEmpty()) {
        return;
    }

    const QByteArray next = m_redoStack.takeLast();
    restoreSnapshot(next);
    m_undoStack.push_back(next);
    refreshActionStates();
    appendLog("Redo applied.");
}

void MainWindow::syncSelectedDeviceDetails() {
    m_selectedLinkId = -1;
    m_topologyScene->setSelectedDeviceId(selectedDevice() ? selectedDevice()->id() : -1);
    refreshDeviceDetails(selectedDevice());
}

void MainWindow::createLink() {
    tryCreateLink(
        m_linkLeftDeviceCombo->currentData().toInt(),
        m_linkLeftInterfaceCombo->currentData().toInt(),
        m_linkRightDeviceCombo->currentData().toInt(),
        m_linkRightInterfaceCombo->currentData().toInt(),
        true);
}

void MainWindow::runPing() {
    const auto* device = selectedDevice();
    if (!device) {
        QMessageBox::information(this, "Ping", "Select a source device first.");
        return;
    }

    appendLog(m_simulator.simulatePing(device->id(), m_pingTargetEdit->text()));
}

void MainWindow::syncLinkCreationControls() {
    rebuildLinkInterfaceChoices(m_linkLeftDeviceCombo, m_linkLeftInterfaceCombo);
    rebuildLinkInterfaceChoices(m_linkRightDeviceCombo, m_linkRightInterfaceCombo);
}

void MainWindow::rebuildDeviceList() {
    const int previousRow = m_deviceList->currentRow();
    m_deviceList->clear();
    for (const auto& device : m_model.devices()) {
        auto* item = new QListWidgetItem(
            QString("%1\n%2")
                .arg(device.name(), device.ipAddress().isEmpty() ? device.typeLabel() : device.ipAddress()),
            m_deviceList);
        item->setData(Qt::UserRole, device.id());
        item->setToolTip(QString("%1 | %2 interfaces").arg(device.typeLabel()).arg(device.interfaces().size()));
        item->setSizeHint(QSize(item->sizeHint().width(), 48));
    }

    if (!m_model.devices().empty()) {
        const int nextRow = previousRow >= 0 && previousRow < m_deviceList->count() ? previousRow : 0;
        m_deviceList->setCurrentRow(nextRow);
    }

    syncLinkCreationControls();
}

void MainWindow::rebuildTopology() {
    m_topologyScene->setSelectedDeviceId(selectedDevice() ? selectedDevice()->id() : -1);
    m_topologyScene->rebuild(m_model);
}

void MainWindow::appendLog(const QString& message) {
    m_logView->append(message);
}

void MainWindow::refreshDeviceDetails(const Device* device) {
    if (!device) {
        const QSignalBlocker blocker(m_interfaceCombo);
        m_nameEdit->clear();
        m_ipEdit->clear();
        m_maskEdit->setText("255.255.255.0");
        m_gatewayEdit->clear();
        m_interfaceCombo->clear();
        m_interfaceIpEdit->clear();
        m_interfaceMaskEdit->clear();
        m_deviceDetails->setText("Select a device to inspect its network settings.");
        return;
    }

    m_nameEdit->setText(device->name());
    m_ipEdit->setText(device->ipAddress());
    m_maskEdit->setText(device->subnetMask());
    m_gatewayEdit->setText(device->defaultGateway());

    const QSignalBlocker blocker(m_interfaceCombo);
    const int previousInterfaceIndex = m_interfaceCombo->currentIndex();
    m_interfaceCombo->clear();
    for (int i = 0; i < static_cast<int>(device->interfaces().size()); ++i) {
        const auto& iface = device->interfaces()[static_cast<std::size_t>(i)];
        m_interfaceCombo->addItem(iface.name(), i);
    }
    if (m_interfaceCombo->count() > 0) {
        m_interfaceCombo->setCurrentIndex(previousInterfaceIndex >= 0 && previousInterfaceIndex < m_interfaceCombo->count() ? previousInterfaceIndex : 0);
    }

    if (const auto* iface = device->interfaceAt(m_interfaceCombo->currentData().toInt())) {
        m_interfaceIpEdit->setText(iface->ipAddress());
        m_interfaceMaskEdit->setText(iface->subnetMask());
    } else {
        m_interfaceIpEdit->clear();
        m_interfaceMaskEdit->clear();
    }

    const QString text = QString(
        "Name: %1\n"
        "Type: %2\n"
        "Position: (%3, %4)\n"
        "IP Address: %5\n"
        "Subnet Mask: %6\n"
        "Default Gateway: %7\n"
        "Interfaces: %8")
        .arg(device->name())
        .arg(device->typeLabel())
        .arg(device->position().x(), 0, 'f', 0)
        .arg(device->position().y(), 0, 'f', 0)
        .arg(device->ipAddress().isEmpty() ? "-" : device->ipAddress())
        .arg(device->subnetMask().isEmpty() ? "-" : device->subnetMask())
        .arg(device->defaultGateway().isEmpty() ? "-" : device->defaultGateway())
        .arg(device->interfaces().size());

    m_deviceDetails->setText(text);
}

QWidget* MainWindow::buildSidebarCard(const QString& title, QWidget* content) {
    auto* wrapper = new QFrame(this);
    wrapper->setObjectName("sidebarCard");
    auto* layout = new QVBoxLayout(wrapper);
    layout->setContentsMargins(14, 14, 14, 14);
    layout->setSpacing(10);

    auto* label = new QLabel(title, wrapper);
    label->setObjectName("cardTitle");
    layout->addWidget(label);
    layout->addWidget(content);
    return wrapper;
}

void MainWindow::applyVisualStyle() {
    setStyleSheet(R"(
        QMainWindow {
            background: #eef3f8;
        }
        QToolBar#topToolbar {
            spacing: 8px;
            padding: 10px 16px;
            border: none;
            background: #ffffff;
        }
        QToolBar#topToolbar QToolButton {
            background: #edf4fb;
            border: 1px solid #d6e1ed;
            border-radius: 10px;
            padding: 8px 12px;
            color: #163047;
            font-weight: 600;
        }
        QToolBar#topToolbar QToolButton:hover {
            background: #dfefff;
        }
        QWidget#sidebarPanel, QWidget#scenePanel {
            background: transparent;
        }
        QFrame#sidebarCard {
            background: #ffffff;
            border: 1px solid #d9e3ee;
            border-radius: 16px;
        }
        QLabel#appTitle {
            font-size: 28px;
            font-weight: 700;
            color: #13263c;
        }
        QLabel#panelTitle, QLabel#cardTitle {
            font-size: 16px;
            font-weight: 700;
            color: #17324d;
        }
        QLabel#mutedLabel {
            color: #607287;
        }
        QListWidget, QTextEdit, QLineEdit, QComboBox {
            background: #fbfdff;
            border: 1px solid #d6e1ed;
            border-radius: 10px;
            padding: 8px;
            color: #12283e;
        }
        QListWidget::item {
            border-radius: 10px;
            padding: 8px;
            margin: 4px 0;
            background: #f4f8fc;
        }
        QListWidget::item:selected {
            background: #dbeafe;
            color: #0f2740;
            border: 1px solid #93c5fd;
        }
        QPushButton {
            background: #163d67;
            color: white;
            border: none;
            border-radius: 10px;
            padding: 10px 14px;
            font-weight: 600;
        }
        QPushButton:hover {
            background: #1f4e81;
        }
        QFormLayout QLabel {
            color: #42576d;
            font-weight: 600;
        }
        QSplitter::handle {
            background: transparent;
            width: 6px;
        }
    )");
}

Device* MainWindow::selectedDevice() {
    const int row = m_deviceList->currentRow();
    if (row < 0 || row >= static_cast<int>(m_model.devices().size())) {
        return nullptr;
    }

    return m_model.findDevice(m_model.devices()[static_cast<std::size_t>(row)].id());
}

const Device* MainWindow::selectedDevice() const {
    const int row = m_deviceList->currentRow();
    if (row < 0 || row >= static_cast<int>(m_model.devices().size())) {
        return nullptr;
    }

    return &m_model.devices()[static_cast<std::size_t>(row)];
}

void MainWindow::rebuildLinkInterfaceChoices(QComboBox* deviceCombo, QComboBox* interfaceCombo) const {
    const QSignalBlocker deviceBlocker(deviceCombo);
    const QSignalBlocker interfaceBlocker(interfaceCombo);

    const QVariant previousDeviceId = deviceCombo->currentData();
    const QVariant previousInterfaceIndex = interfaceCombo->currentData();

    if (deviceCombo->count() != static_cast<int>(m_model.devices().size())) {
        deviceCombo->clear();
        for (const auto& device : m_model.devices()) {
            deviceCombo->addItem(device.name(), device.id());
        }
    }

    if (previousDeviceId.isValid()) {
        const int previousIndex = deviceCombo->findData(previousDeviceId);
        if (previousIndex >= 0) {
            deviceCombo->setCurrentIndex(previousIndex);
        }
    }

    const int deviceId = deviceCombo->currentData().toInt();
    const auto* device = m_model.findDevice(deviceId);
    interfaceCombo->clear();
    if (!device) {
        return;
    }

    for (int i = 0; i < static_cast<int>(device->interfaces().size()); ++i) {
        const auto& iface = device->interfaces()[static_cast<std::size_t>(i)];
        const bool busy = m_model.findLinkForInterface(device->id(), i) != nullptr;
        interfaceCombo->addItem(QString("%1%2").arg(iface.name(), busy ? " (used)" : ""), i);
    }

    if (previousInterfaceIndex.isValid()) {
        const int previousIndex = interfaceCombo->findData(previousInterfaceIndex);
        if (previousIndex >= 0) {
            interfaceCombo->setCurrentIndex(previousIndex);
            return;
        }
    }

    if (interfaceCombo->count() > 0) {
        interfaceCombo->setCurrentIndex(0);
    }
}

void MainWindow::selectDeviceById(int deviceId) {
    for (int row = 0; row < static_cast<int>(m_model.devices().size()); ++row) {
        if (m_model.devices()[static_cast<std::size_t>(row)].id() == deviceId) {
            m_deviceList->setCurrentRow(row);
            return;
        }
    }
}

bool MainWindow::tryCreateLink(int leftDeviceId, int leftInterfaceIndex, int rightDeviceId, int rightInterfaceIndex, bool showMessages) {
    if (!leftDeviceId || !rightDeviceId || leftDeviceId == rightDeviceId) {
        if (showMessages) {
            QMessageBox::warning(this, "Invalid Link", "Choose two different devices.");
        }
        return false;
    }

    if (m_model.findLinkForInterface(leftDeviceId, leftInterfaceIndex) ||
        m_model.findLinkForInterface(rightDeviceId, rightInterfaceIndex)) {
        if (showMessages) {
            QMessageBox::warning(this, "Port Busy", "One of the selected interfaces is already linked.");
        }
        return false;
    }

    m_model.addLink(leftDeviceId, leftInterfaceIndex, rightDeviceId, rightInterfaceIndex);
    m_selectedLinkId = -1;
    rebuildTopology();
    syncLinkCreationControls();
    pushHistorySnapshot();
    refreshActionStates();
    appendLog(QString("Created link: %1:%2 <-> %3:%4")
        .arg(leftDeviceId)
        .arg(leftInterfaceIndex + 1)
        .arg(rightDeviceId)
        .arg(rightInterfaceIndex + 1));
    return true;
}

void MainWindow::pushHistorySnapshot() {
    if (m_restoringHistory) {
        return;
    }

    const QByteArray snapshot = ProjectSerializer::serialize(m_model);
    if (!m_undoStack.isEmpty() && m_undoStack.last() == snapshot) {
        refreshActionStates();
        return;
    }

    m_undoStack.push_back(snapshot);
    while (m_undoStack.size() > 50) {
        m_undoStack.pop_front();
    }
    m_redoStack.clear();
    refreshActionStates();
}

bool MainWindow::restoreSnapshot(const QByteArray& snapshot) {
    QString error;
    m_restoringHistory = true;
    const bool ok = ProjectSerializer::deserialize(m_model, snapshot, &error);
    m_restoringHistory = false;
    if (!ok) {
        QMessageBox::critical(this, "Restore Failed", error);
        return false;
    }

    m_selectedLinkId = -1;
    rebuildDeviceList();
    rebuildTopology();
    syncSelectedDeviceDetails();
    syncLinkCreationControls();
    return true;
}

void MainWindow::refreshActionStates() {
    if (m_undoAction) {
        m_undoAction->setEnabled(m_undoStack.size() > 1);
    }
    if (m_redoAction) {
        m_redoAction->setEnabled(!m_redoStack.isEmpty());
    }
}

}  // namespace packetlab
