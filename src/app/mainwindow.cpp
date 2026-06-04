#include "mainwindow.h"

#include "simulationdialog.h"
#include "topologyscene.h"

#include <QAction>
#include <QClipboard>
#include <QCoreApplication>
#include <QComboBox>
#include <QFileDialog>
#include <QFrame>
#include <QFormLayout>
#include <QGraphicsView>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStatusBar>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QTime>
#include <QToolBar>
#include <QVBoxLayout>
#include <QWidget>
#include <QLineEdit>
#include <QRegularExpression>

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
      m_deviceSummary(nullptr),
      m_nameEdit(new QLineEdit(this)),
      m_ipEdit(new QLineEdit(this)),
      m_maskEdit(new QLineEdit(this)),
      m_gatewayEdit(new QLineEdit(this)),
      m_interfaceCombo(new QComboBox(this)),
      m_interfaceIpEdit(new QLineEdit(this)),
      m_interfaceMaskEdit(new QLineEdit(this)),
      m_interfaceTable(nullptr),
      m_linkLeftDeviceCombo(new QComboBox(this)),
      m_linkLeftInterfaceCombo(new QComboBox(this)),
      m_linkRightDeviceCombo(new QComboBox(this)),
      m_linkRightInterfaceCombo(new QComboBox(this)),
      m_testTargetEdit(new QLineEdit(this)),
      m_testTypeCombo(new QComboBox(this)),
      m_testTargetLabel(new QLabel("Target IP:", this)),
      m_routeTable(new QTableWidget(0, 4, this)),
      m_routeCard(nullptr),
      m_undoAction(nullptr),
      m_redoAction(nullptr) {
    // m_pingTargetEdit is the same widget as m_testTargetEdit
    m_pingTargetEdit = m_testTargetEdit;

    setWindowTitle("PacketLab");
    resize(1480, 920);
    applyVisualStyle();

    m_model.seedDemoTopology();

    // ── Toolbar ──────────────────────────────────────────────────────────────
    auto* toolbar = addToolBar("Devices");
    toolbar->setObjectName("topToolbar");
    m_undoAction = toolbar->addAction("Undo", this, &MainWindow::undo);
    m_redoAction = toolbar->addAction("Redo", this, &MainWindow::redo);
    toolbar->addSeparator();
    toolbar->addAction("Open",   this, &MainWindow::loadProject);
    toolbar->addAction("Save",   this, &MainWindow::saveProject);
    toolbar->addAction("Demo",   this, &MainWindow::loadDemoProject);
    toolbar->addAction("✕ Delete", this, &MainWindow::deleteSelection);
    toolbar->addSeparator();
    toolbar->addAction("⊕ PC",     this, &MainWindow::addPc);
    toolbar->addAction("⊕ Switch", this, &MainWindow::addSwitch);
    toolbar->addAction("⊕ Router", this, &MainWindow::addRouter);
    toolbar->addSeparator();
    toolbar->addAction("⊕ Laptop", this, &MainWindow::addLaptop);
    toolbar->addAction("⊕ Server", this, &MainWindow::addServer);
    toolbar->addAction("⊕ Phone",  this, &MainWindow::addIpPhone);
    toolbar->addAction("⊕ Camera", this, &MainWindow::addIpCamera);
    toolbar->addAction("⊕ AP",     this, &MainWindow::addWirelessAp);
    toolbar->addAction("⊕ L3 SW",  this, &MainWindow::addSwitchL3);
    toolbar->addAction("⊕ FW",     this, &MainWindow::addFirewall);
    toolbar->addSeparator();
    toolbar->addAction("▶ Tick", this, &MainWindow::runSimulationTick);
    toolbar->setMovable(false);

    // ── Topology view ─────────────────────────────────────────────────────────
    m_topologyView->setScene(m_topologyScene);
    m_topologyView->setRenderHint(QPainter::Antialiasing, true);
    m_topologyView->setDragMode(QGraphicsView::ScrollHandDrag);
    m_topologyView->setFrameShape(QFrame::NoFrame);
    m_topologyView->setViewportUpdateMode(QGraphicsView::BoundingRectViewportUpdate);

    // ── Inspector card content ────────────────────────────────────────────────
    auto* detailsWidget = new QWidget(this);
    auto* detailsLayout = new QVBoxLayout(detailsWidget);
    auto* detailsForm = new QFormLayout();
    detailsForm->addRow("Name",        m_nameEdit);
    detailsForm->addRow("Device IP",   m_ipEdit);
    detailsForm->addRow("Subnet Mask", m_maskEdit);
    detailsForm->addRow("Gateway",     m_gatewayEdit);
    detailsForm->addRow("Interface",   m_interfaceCombo);
    detailsForm->addRow("IF IP",       m_interfaceIpEdit);
    detailsForm->addRow("IF Mask",     m_interfaceMaskEdit);
    detailsLayout->addLayout(detailsForm);
    auto* applyButton = new QPushButton("Apply Device Config", detailsWidget);
    detailsLayout->addWidget(applyButton);
    m_deviceDetails->setWordWrap(true);
    detailsLayout->addWidget(m_deviceDetails);

    // ── Route card ────────────────────────────────────────────────────────────
    m_routeTable->setHorizontalHeaderLabels({"Destination", "Mask", "Next Hop", "Metric"});
    m_routeTable->horizontalHeader()->setStretchLastSection(false);
    m_routeTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_routeTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_routeTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_routeTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Fixed);
    m_routeTable->setColumnWidth(3, 60);
    m_routeTable->setMaximumHeight(160);
    m_routeTable->setSelectionBehavior(QAbstractItemView::SelectRows);

    auto* routeButtons = new QHBoxLayout();
    auto* addRouteBtn    = new QPushButton("+ Add",    this);
    auto* removeRouteBtn = new QPushButton("− Remove", this);
    auto* applyRoutesBtn = new QPushButton("✓ Apply",  this);
    routeButtons->addWidget(addRouteBtn);
    routeButtons->addWidget(removeRouteBtn);
    routeButtons->addStretch();
    routeButtons->addWidget(applyRoutesBtn);

    auto* routeInner = new QWidget(this);
    auto* routeInnerLayout = new QVBoxLayout(routeInner);
    routeInnerLayout->setContentsMargins(0, 0, 0, 0);
    routeInnerLayout->addWidget(m_routeTable);
    routeInnerLayout->addLayout(routeButtons);

    m_routeCard = buildSidebarCard("Static Routes", routeInner);
    m_routeCard->setVisible(false);

    connect(addRouteBtn,    &QPushButton::clicked, this, &MainWindow::addRouteRow);
    connect(removeRouteBtn, &QPushButton::clicked, this, &MainWindow::removeRouteRow);
    connect(applyRoutesBtn, &QPushButton::clicked, this, &MainWindow::applyRoutes);

    // ── Tests tab ─────────────────────────────────────────────────────────────
    m_testTypeCombo->addItem("Ping",                0);
    m_testTypeCombo->addItem("Traceroute",          1);
    m_testTypeCombo->addItem("ARP Table",           2);
    m_testTypeCombo->addItem("Route Table",         3);
    m_testTypeCombo->addItem("Connectivity Matrix", 4);
    m_testTypeCombo->addItem("Subnet Audit",        5);

    m_testTargetEdit->setPlaceholderText("e.g. 10.0.0.10");

    auto* runTestBtn  = new QPushButton("▶  Run Test", this);
    auto* testForm    = new QFormLayout();
    testForm->addRow("Test:", m_testTypeCombo);
    testForm->addRow(m_testTargetLabel, m_testTargetEdit);
    auto* testsWidget = new QWidget(this);
    auto* testsLayout = new QVBoxLayout(testsWidget);
    testsLayout->addLayout(testForm);
    testsLayout->addWidget(runTestBtn);
    testsLayout->addStretch();

    connect(runTestBtn,      &QPushButton::clicked,                              this, &MainWindow::runSelectedTest);
    connect(m_testTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::onTestTypeChanged);

    // ── Links tab ─────────────────────────────────────────────────────────────
    auto* linksWidget = new QWidget(this);
    auto* linksLayout = new QVBoxLayout(linksWidget);
    auto* linkForm = new QFormLayout();
    linkForm->addRow("Left Device",  m_linkLeftDeviceCombo);
    linkForm->addRow("Left Port",    m_linkLeftInterfaceCombo);
    linkForm->addRow("Right Device", m_linkRightDeviceCombo);
    linkForm->addRow("Right Port",   m_linkRightInterfaceCombo);
    linksLayout->addLayout(linkForm);
    auto* createLinkButton = new QPushButton("Create Link", linksWidget);
    linksLayout->addWidget(createLinkButton);
    linksLayout->addStretch();

    connect(createLinkButton, &QPushButton::clicked, this, &MainWindow::createLink);

    // ── Tab widget combining Tests + Links ────────────────────────────────────
    auto* testTabs = new QTabWidget(this);
    testTabs->addTab(testsWidget, "Tests");
    testTabs->addTab(linksWidget, "Links");

    m_logView->setReadOnly(true);

    // ── Left panel ───────────────────────────────────────────────────────────
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

    // ── Scene panel ──────────────────────────────────────────────────────────
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

    // ── Right panel ──────────────────────────────────────────────────────────
    auto* rightPanel = new QWidget(this);
    rightPanel->setObjectName("sidebarPanel");
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->addWidget(buildSidebarCard("Inspector", detailsWidget));
    rightLayout->addWidget(m_routeCard);
    rightLayout->addWidget(buildSidebarCard("Simulation", testTabs));
    rightLayout->addWidget(buildSidebarCard("Simulation Log", m_logView), 1);

    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->addWidget(leftPanel);
    splitter->addWidget(scenePanel);
    splitter->addWidget(rightPanel);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 1);
    splitter->setStretchFactor(2, 0);
    splitter->setSizes({280, 900, 340});
    setCentralWidget(splitter);
    statusBar()->show();

    // ── Connections ──────────────────────────────────────────────────────────
    connect(m_deviceList,  &QListWidget::currentRowChanged,              this, &MainWindow::syncSelectedDeviceDetails);
    connect(m_topologyScene, &TopologyScene::deviceSelected,             this, &MainWindow::handleSceneDeviceSelected);
    connect(m_topologyScene, &TopologyScene::deviceMoved,                this, &MainWindow::handleSceneDeviceMoved);
    connect(m_topologyScene, &TopologyScene::deviceMoveCommitted,        this, &MainWindow::handleSceneDeviceMoveCommitted);
    connect(m_topologyScene, &TopologyScene::cableRequested,             this, &MainWindow::handleSceneCableRequested);
    connect(m_topologyScene, &TopologyScene::linkSelected,               this, &MainWindow::handleSceneLinkSelected);
    connect(m_topologyScene, &TopologyScene::deviceContextMenuRequested, this, &MainWindow::showDeviceContextMenu);
    connect(m_topologyScene, &TopologyScene::linkContextMenuRequested,   this, &MainWindow::showLinkContextMenu);
    connect(m_topologyScene, &TopologyScene::canvasContextMenuRequested, this, &MainWindow::showCanvasContextMenu);
    connect(applyButton,     &QPushButton::clicked,                      this, &MainWindow::applyDeviceConfiguration);
    connect(m_interfaceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::syncSelectedDeviceDetails);
    connect(m_linkLeftDeviceCombo,  QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::syncLinkCreationControls);
    connect(m_linkRightDeviceCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::syncLinkCreationControls);

    // Initialise test target visibility
    onTestTypeChanged(0);

    rebuildDeviceList();
    rebuildTopology();
    updateStatusBar();
    appendLog("PacketLab initialized.");
    syncSelectedDeviceDetails();
    syncLinkCreationControls();
    pushHistorySnapshot();
    refreshActionStates();
}

// ── Add device slots ─────────────────────────────────────────────────────────

void MainWindow::addPc() {
    m_model.addDevice(DeviceType::Pc, "PC");
    rebuildDeviceList(); rebuildTopology(); pushHistorySnapshot();
    appendLog("Added PC.");
}

void MainWindow::addSwitch() {
    m_model.addDevice(DeviceType::Switch, "SW");
    rebuildDeviceList(); rebuildTopology(); pushHistorySnapshot();
    appendLog("Added Switch.");
}

void MainWindow::addRouter() {
    m_model.addDevice(DeviceType::Router, "R");
    rebuildDeviceList(); rebuildTopology(); pushHistorySnapshot();
    appendLog("Added Router.");
}

void MainWindow::addLaptop() {
    m_model.addDevice(DeviceType::Laptop, "LAP");
    rebuildDeviceList(); rebuildTopology(); pushHistorySnapshot();
    appendLog("Added Laptop.");
}

void MainWindow::addServer() {
    m_model.addDevice(DeviceType::Server, "SRV");
    rebuildDeviceList(); rebuildTopology(); pushHistorySnapshot();
    appendLog("Added Server.");
}

void MainWindow::addIpPhone() {
    m_model.addDevice(DeviceType::IpPhone, "PH");
    rebuildDeviceList(); rebuildTopology(); pushHistorySnapshot();
    appendLog("Added IP Phone.");
}

void MainWindow::addIpCamera() {
    m_model.addDevice(DeviceType::IpCamera, "CAM");
    rebuildDeviceList(); rebuildTopology(); pushHistorySnapshot();
    appendLog("Added IP Camera.");
}

void MainWindow::addWirelessAp() {
    m_model.addDevice(DeviceType::WirelessAp, "AP");
    rebuildDeviceList(); rebuildTopology(); pushHistorySnapshot();
    appendLog("Added Wireless AP.");
}

void MainWindow::addSwitchL3() {
    m_model.addDevice(DeviceType::SwitchL3, "L3");
    rebuildDeviceList(); rebuildTopology(); pushHistorySnapshot();
    appendLog("Added L3 Switch.");
}

void MainWindow::addFirewall() {
    m_model.addDevice(DeviceType::Firewall, "FW");
    rebuildDeviceList(); rebuildTopology(); pushHistorySnapshot();
    appendLog("Added Firewall.");
}

// ── Simulation slots ─────────────────────────────────────────────────────────

void MainWindow::runSimulationTick() {
    appendLog(m_simulator.runTick());
}

void MainWindow::runSelectedTest() {
    const int testType = m_testTypeCombo->currentData().toInt();
    const auto* device = selectedDevice();

    QString title, subtitle, result;

    switch (testType) {
    case 0: // Ping
        if (!device) { appendLog("Test: select a source device first."); return; }
        title    = QString("Ping — %1").arg(device->name());
        subtitle = QString("From: %1 (%2)  →  Target: %3")
                       .arg(device->name(), device->ipAddress(), m_testTargetEdit->text());
        result   = m_simulator.simulatePing(device->id(), m_testTargetEdit->text());
        break;
    case 1: // Traceroute
        if (!device) { appendLog("Test: select a source device first."); return; }
        title    = QString("Traceroute — %1").arg(device->name());
        subtitle = QString("From: %1  →  Target: %2").arg(device->name(), m_testTargetEdit->text());
        result   = m_simulator.simulateTraceroute(device->id(), m_testTargetEdit->text());
        break;
    case 2: // ARP Table
        if (!device) { appendLog("Test: select a device first."); return; }
        title    = QString("ARP Table — %1").arg(device->name());
        subtitle = QString("Interface: %1").arg(device->ipAddress());
        result   = m_simulator.simulateArpTable(device->id());
        break;
    case 3: // Route Table
        if (!device) { appendLog("Test: select a device first."); return; }
        title    = QString("Route Table — %1").arg(device->name());
        subtitle = QString("Device: %1 (%2)").arg(device->name(), device->ipAddress());
        result   = m_simulator.simulateRouteTable(device->id());
        break;
    case 4: // Connectivity Matrix
        title    = "Connectivity Matrix";
        subtitle = QString("Topology: %1 devices, %2 links")
                       .arg(m_model.devices().size()).arg(m_model.links().size());
        result   = m_simulator.simulateConnectivityMatrix();
        break;
    case 5: // Subnet Audit
        title    = "Subnet Audit";
        subtitle = "Full network diagnostic";
        result   = m_simulator.simulateSubnetAudit();
        break;
    default:
        return;
    }

    auto* dlg = new SimulationDialog(title, subtitle, this);
    dlg->runOutput(result);
    dlg->show();
    appendLog(QString("[Test] %1").arg(title));
}

void MainWindow::runPing() {
    const auto* device = selectedDevice();
    if (!device) {
        QMessageBox::information(this, "Ping", "Select a source device first.");
        return;
    }
    const QString target = m_testTargetEdit ? m_testTargetEdit->text() : "";
    auto* dlg = new SimulationDialog(
        QString("Ping — %1").arg(device->name()),
        QString("From: %1 → Target: %2").arg(device->name(), target.isEmpty() ? "?" : target),
        this);
    dlg->runOutput(m_simulator.simulatePing(device->id(), target));
    dlg->show();
}

void MainWindow::onTestTypeChanged(int index) {
    const bool needsTarget = (index == 0 || index == 1);
    m_testTargetLabel->setVisible(needsTarget);
    m_testTargetEdit->setVisible(needsTarget);
}

// ── Route table slots ─────────────────────────────────────────────────────────

void MainWindow::addRouteRow() {
    const int row = m_routeTable->rowCount();
    m_routeTable->insertRow(row);
    m_routeTable->setItem(row, 0, new QTableWidgetItem("0.0.0.0"));
    m_routeTable->setItem(row, 1, new QTableWidgetItem("0.0.0.0"));
    m_routeTable->setItem(row, 2, new QTableWidgetItem(""));
    m_routeTable->setItem(row, 3, new QTableWidgetItem("1"));
}

void MainWindow::removeRouteRow() {
    const int row = m_routeTable->currentRow();
    if (row >= 0) m_routeTable->removeRow(row);
}

void MainWindow::applyRoutes() {
    auto* device = selectedDevice();
    if (!device || !device->isL3Capable()) return;
    device->routingTable().clear();
    for (int r = 0; r < m_routeTable->rowCount(); ++r) {
        RouteEntry entry;
        entry.destination = m_routeTable->item(r, 0) ? m_routeTable->item(r, 0)->text().trimmed() : "";
        entry.mask        = m_routeTable->item(r, 1) ? m_routeTable->item(r, 1)->text().trimmed() : "";
        entry.nextHop     = m_routeTable->item(r, 2) ? m_routeTable->item(r, 2)->text().trimmed() : "";
        entry.metric      = m_routeTable->item(r, 3) ? m_routeTable->item(r, 3)->text().toInt()   : 1;
        if (!entry.destination.isEmpty() && !entry.mask.isEmpty())
            device->routingTable().push_back(entry);
    }
    pushHistorySnapshot();
    appendLog(QString("Applied %1 route(s) to %2.")
                  .arg(device->routingTable().size()).arg(device->name()));
}

void MainWindow::refreshRouteTable(const Device* device) {
    if (!m_routeCard) return;
    if (!device || !device->isL3Capable()) {
        m_routeCard->setVisible(false);
        return;
    }
    m_routeCard->setVisible(true);
    m_routeTable->setRowCount(0);
    for (const auto& route : device->routingTable()) {
        const int r = m_routeTable->rowCount();
        m_routeTable->insertRow(r);
        m_routeTable->setItem(r, 0, new QTableWidgetItem(route.destination));
        m_routeTable->setItem(r, 1, new QTableWidgetItem(route.mask));
        m_routeTable->setItem(r, 2, new QTableWidgetItem(route.nextHop));
        m_routeTable->setItem(r, 3, new QTableWidgetItem(QString::number(route.metric)));
    }
}

// ── Existing slots ────────────────────────────────────────────────────────────

static bool isValidIpv4(const QString& ip) {
    if (ip.isEmpty()) return true;
    static const QRegularExpression re(R"(^(\d{1,3}\.){3}\d{1,3}$)");
    if (!re.match(ip).hasMatch()) return false;
    for (const QString& part : ip.split('.')) {
        bool ok = false;
        const int val = part.toInt(&ok);
        if (!ok || val < 0 || val > 255) return false;
    }
    return true;
}

void MainWindow::applyDeviceConfiguration() {
    auto* device = selectedDevice();
    if (!device) return;

    const QString ip     = m_ipEdit->text().trimmed();
    const QString mask   = m_maskEdit->text().trimmed();
    const QString gw     = m_gatewayEdit->text().trimmed();
    const QString ifIp   = m_interfaceIpEdit->text().trimmed();
    const QString ifMask = m_interfaceMaskEdit->text().trimmed();

    if (!isValidIpv4(ip) || !isValidIpv4(mask) || !isValidIpv4(gw) || !isValidIpv4(ifIp) || !isValidIpv4(ifMask)) {
        QMessageBox::warning(this, "Invalid IP", "One or more IP address fields contain an invalid value.");
        return;
    }

    device->setName(m_nameEdit->text().trimmed().isEmpty() ? device->name() : m_nameEdit->text().trimmed());
    device->setIpAddress(ip);
    device->setSubnetMask(mask);
    device->setDefaultGateway(gw);

    if (auto* iface = device->interfaceAt(m_interfaceCombo->currentIndex())) {
        iface->setIpAddress(ifIp);
        iface->setSubnetMask(ifMask);
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
        this, "Save PacketLab Project", QString(), "PacketLab Project (*.json)");
    if (filePath.isEmpty()) return;
    QString error;
    if (!ProjectSerializer::save(m_model, filePath, &error))
        QMessageBox::critical(this, "Save Failed", error);
    else
        appendLog(QString("Project saved: %1").arg(filePath));
}

void MainWindow::loadProject() {
    const QString filePath = QFileDialog::getOpenFileName(
        this, "Load PacketLab Project", QString(), "PacketLab Project (*.json)");
    if (filePath.isEmpty()) return;
    QString error;
    if (!ProjectSerializer::load(m_model, filePath, &error)) {
        QMessageBox::critical(this, "Load Failed", error);
        return;
    }
    rebuildDeviceList(); rebuildTopology();
    appendLog(QString("Project loaded: %1").arg(filePath));
    syncSelectedDeviceDetails(); pushHistorySnapshot(); refreshActionStates();
}

void MainWindow::loadDemoProject() {
    const QString filePath = QCoreApplication::applicationDirPath() + "/../assets/demo-project.json";
    QString error;
    if (!ProjectSerializer::load(m_model, filePath, &error)) {
        QMessageBox::critical(this, "Demo Load Failed", error);
        return;
    }
    rebuildDeviceList(); rebuildTopology();
    appendLog("Loaded bundled demo topology.");
    syncSelectedDeviceDetails(); pushHistorySnapshot(); refreshActionStates();
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

void MainWindow::handleSceneDeviceMoveCommitted(int deviceId, QPointF position) {
    if (auto* device = m_model.findDevice(deviceId)) {
        device->setPosition(position);
        pushHistorySnapshot();
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
    menu.addAction("Run Ping",      this, &MainWindow::runPing);
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
    menu.addAction("Add PC",         this, &MainWindow::addPc);
    menu.addAction("Add Switch",     this, &MainWindow::addSwitch);
    menu.addAction("Add Router",     this, &MainWindow::addRouter);
    menu.addSeparator();
    menu.addAction("Add Laptop",     this, &MainWindow::addLaptop);
    menu.addAction("Add Server",     this, &MainWindow::addServer);
    menu.addAction("Add IP Phone",   this, &MainWindow::addIpPhone);
    menu.addAction("Add IP Camera",  this, &MainWindow::addIpCamera);
    menu.addAction("Add Wireless AP",this, &MainWindow::addWirelessAp);
    menu.addAction("Add L3 Switch",  this, &MainWindow::addSwitchL3);
    menu.addAction("Add Firewall",   this, &MainWindow::addFirewall);
    menu.exec(screenPos.toPoint());
}

void MainWindow::deleteSelection() {
    if (m_selectedLinkId >= 0) {
        const int linkId = m_selectedLinkId;
        if (m_model.removeLink(linkId)) {
            m_selectedLinkId = -1;
            rebuildTopology(); syncLinkCreationControls();
            pushHistorySnapshot(); refreshActionStates();
            appendLog(QString("Deleted link #%1").arg(linkId));
        }
        return;
    }
    if (auto* device = selectedDevice()) {
        const QString name = device->name();
        const int id = device->id();
        if (m_model.removeDevice(id)) {
            rebuildDeviceList(); rebuildTopology();
            syncSelectedDeviceDetails(); pushHistorySnapshot(); refreshActionStates();
            appendLog(QString("Deleted device %1").arg(name));
        }
    }
}

void MainWindow::undo() {
    if (m_undoStack.size() <= 1) return;
    const QByteArray current = m_undoStack.takeLast();
    m_redoStack.push_back(current);
    restoreSnapshot(m_undoStack.last());
    refreshActionStates();
    appendLog("Undo applied.");
}

void MainWindow::redo() {
    if (m_redoStack.isEmpty()) return;
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

void MainWindow::syncLinkCreationControls() {
    rebuildLinkInterfaceChoices(m_linkLeftDeviceCombo,  m_linkLeftInterfaceCombo);
    rebuildLinkInterfaceChoices(m_linkRightDeviceCombo, m_linkRightInterfaceCombo);
}

// ── Private helpers ───────────────────────────────────────────────────────────

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
    updateStatusBar();
}

void MainWindow::rebuildTopology() {
    m_topologyScene->setSelectedDeviceId(selectedDevice() ? selectedDevice()->id() : -1);
    m_topologyScene->rebuild(m_model);
}

void MainWindow::appendLog(const QString& message) {
    const QString ts = QTime::currentTime().toString("hh:mm:ss");
    m_logView->append(QString("<span style='color:#475569'>[%1]</span> %2").arg(ts, message));
}

void MainWindow::updateStatusBar() {
    const int devices = static_cast<int>(m_model.devices().size());
    const int links   = static_cast<int>(m_model.links().size());
    statusBar()->showMessage(
        QString("  %1 device%2   ·   %3 link%4   ·   Undo depth: %5")
            .arg(devices).arg(devices == 1 ? "" : "s")
            .arg(links).arg(links == 1 ? "" : "s")
            .arg(m_undoStack.size()));
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
        refreshRouteTable(nullptr);
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
    if (m_interfaceCombo->count() > 0)
        m_interfaceCombo->setCurrentIndex(
            previousInterfaceIndex >= 0 && previousInterfaceIndex < m_interfaceCombo->count()
                ? previousInterfaceIndex : 0);

    if (const auto* iface = device->interfaceAt(m_interfaceCombo->currentData().toInt())) {
        m_interfaceIpEdit->setText(iface->ipAddress());
        m_interfaceMaskEdit->setText(iface->subnetMask());
    } else {
        m_interfaceIpEdit->clear();
        m_interfaceMaskEdit->clear();
    }

    const QString text = QString(
        "Name: %1\nType: %2\nPosition: (%3, %4)\n"
        "IP Address: %5\nSubnet Mask: %6\nDefault Gateway: %7\nInterfaces: %8")
        .arg(device->name(), device->typeLabel())
        .arg(device->position().x(), 0, 'f', 0)
        .arg(device->position().y(), 0, 'f', 0)
        .arg(device->ipAddress().isEmpty()      ? "-" : device->ipAddress())
        .arg(device->subnetMask().isEmpty()     ? "-" : device->subnetMask())
        .arg(device->defaultGateway().isEmpty() ? "-" : device->defaultGateway())
        .arg(device->interfaces().size());
    m_deviceDetails->setText(text);

    refreshRouteTable(device);
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
        QMainWindow { background: #f1f5f9; }
        QToolBar#topToolbar {
            background: #ffffff;
            border-bottom: 1px solid #e2e8f0;
            spacing: 4px;
            padding: 8px 16px;
        }
        QToolBar#topToolbar QToolButton {
            background: #f8fafc;
            border: 1px solid #e2e8f0;
            border-radius: 8px;
            padding: 6px 14px;
            color: #334155;
            font-size: 12px;
            font-weight: 600;
        }
        QToolBar#topToolbar QToolButton:hover { background: #e0f2fe; border-color: #bae6fd; color: #0369a1; }
        QToolBar#topToolbar QToolButton:pressed { background: #bae6fd; }
        QToolBar#topToolbar::separator { background: #e2e8f0; width: 1px; margin: 4px 6px; }
        QWidget#sidebarPanel, QWidget#scenePanel { background: transparent; }
        QFrame#sidebarCard { background: #ffffff; border: 1px solid #e2e8f0; border-radius: 14px; }
        QLabel#appTitle { font-size: 20px; font-weight: 800; color: #0f172a; }
        QLabel#panelTitle, QLabel#cardTitle { font-size: 11px; font-weight: 700; color: #64748b; letter-spacing: 0.5px; }
        QLabel#mutedLabel { color: #94a3b8; font-size: 11px; }
        QListWidget { background: #f8fafc; border: 1px solid #e2e8f0; border-radius: 10px; padding: 4px; outline: none; }
        QListWidget::item { border-radius: 8px; padding: 8px 10px; margin: 2px 0; color: #334155; font-size: 12px; }
        QListWidget::item:hover { background: #f0f9ff; }
        QListWidget::item:selected { background: #dbeafe; color: #1e40af; border: 1px solid #93c5fd; }
        QLineEdit, QComboBox { background: #f8fafc; border: 1px solid #e2e8f0; border-radius: 7px; padding: 5px 10px; color: #334155; font-size: 12px; }
        QLineEdit:focus, QComboBox:focus { border-color: #93c5fd; background: #ffffff; }
        QTextEdit { background: #0f172a; border: 1px solid #1e293b; border-radius: 10px; padding: 8px; color: #94a3b8; font-family: monospace; font-size: 11px; }
        QPushButton { background: #2563eb; color: #ffffff; border: none; border-radius: 8px; padding: 8px 16px; font-size: 12px; font-weight: 600; }
        QPushButton:hover { background: #1d4ed8; }
        QPushButton:pressed { background: #1e40af; }
        QFormLayout QLabel { color: #64748b; font-size: 11px; font-weight: 600; }
        QSplitter::handle { background: #e2e8f0; width: 1px; margin: 12px 2px; }
        QScrollBar:vertical { background: transparent; width: 6px; }
        QScrollBar::handle:vertical { background: #cbd5e1; border-radius: 3px; min-height: 24px; }
        QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0; }
        QScrollBar:horizontal { background: transparent; height: 6px; }
        QScrollBar::handle:horizontal { background: #cbd5e1; border-radius: 3px; min-width: 24px; }
        QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0; }
        QStatusBar { background: #ffffff; border-top: 1px solid #e2e8f0; color: #64748b; font-size: 11px; padding: 0 8px; }
        QComboBox::drop-down { border: none; width: 18px; }
        QComboBox QAbstractItemView { background: #ffffff; border: 1px solid #e2e8f0; border-radius: 8px; selection-background-color: #dbeafe; color: #334155; }
        QTableWidget { background: #f8fafc; border: 1px solid #e2e8f0; border-radius: 8px; gridline-color: #e2e8f0; font-size: 11px; color: #334155; }
        QHeaderView::section { background: #f1f5f9; border: none; padding: 4px 8px; font-size: 11px; font-weight: 600; color: #64748b; }
        QTabWidget::pane { border: none; }
        QTabBar::tab { background: #f8fafc; border: 1px solid #e2e8f0; border-radius: 6px; padding: 5px 12px; margin: 2px; font-size: 11px; font-weight: 600; color: #64748b; }
        QTabBar::tab:selected { background: #dbeafe; color: #1e40af; border-color: #93c5fd; }
    )");
}

Device* MainWindow::selectedDevice() {
    const auto* item = m_deviceList->currentItem();
    if (!item) return nullptr;
    return m_model.findDevice(item->data(Qt::UserRole).toInt());
}

const Device* MainWindow::selectedDevice() const {
    const auto* item = m_deviceList->currentItem();
    if (!item) return nullptr;
    return m_model.findDevice(item->data(Qt::UserRole).toInt());
}

void MainWindow::rebuildLinkInterfaceChoices(QComboBox* deviceCombo, QComboBox* interfaceCombo) const {
    const QSignalBlocker deviceBlocker(deviceCombo);
    const QSignalBlocker interfaceBlocker(interfaceCombo);

    const QVariant previousDeviceId       = deviceCombo->currentData();
    const QVariant previousInterfaceIndex = interfaceCombo->currentData();

    if (deviceCombo->count() != static_cast<int>(m_model.devices().size())) {
        deviceCombo->clear();
        for (const auto& device : m_model.devices())
            deviceCombo->addItem(device.name(), device.id());
    }

    if (previousDeviceId.isValid()) {
        const int idx = deviceCombo->findData(previousDeviceId);
        if (idx >= 0) deviceCombo->setCurrentIndex(idx);
    }

    const int deviceId = deviceCombo->currentData().toInt();
    const auto* device = m_model.findDevice(deviceId);
    interfaceCombo->clear();
    if (!device) return;

    for (int i = 0; i < static_cast<int>(device->interfaces().size()); ++i) {
        const auto& iface = device->interfaces()[static_cast<std::size_t>(i)];
        const bool busy = m_model.findLinkForInterface(device->id(), i) != nullptr;
        interfaceCombo->addItem(QString("%1%2").arg(iface.name(), busy ? " (used)" : ""), i);
    }

    if (previousInterfaceIndex.isValid()) {
        const int idx = interfaceCombo->findData(previousInterfaceIndex);
        if (idx >= 0) { interfaceCombo->setCurrentIndex(idx); return; }
    }
    if (interfaceCombo->count() > 0) interfaceCombo->setCurrentIndex(0);
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
        if (showMessages) QMessageBox::warning(this, "Invalid Link", "Choose two different devices.");
        return false;
    }
    if (m_model.findLinkForInterface(leftDeviceId, leftInterfaceIndex) ||
        m_model.findLinkForInterface(rightDeviceId, rightInterfaceIndex)) {
        if (showMessages) QMessageBox::warning(this, "Port Busy", "One of the selected interfaces is already linked.");
        return false;
    }
    m_model.addLink(leftDeviceId, leftInterfaceIndex, rightDeviceId, rightInterfaceIndex);
    m_selectedLinkId = -1;
    rebuildTopology(); syncLinkCreationControls(); pushHistorySnapshot(); refreshActionStates();
    appendLog(QString("Created link: %1:%2 <-> %3:%4")
        .arg(leftDeviceId).arg(leftInterfaceIndex + 1)
        .arg(rightDeviceId).arg(rightInterfaceIndex + 1));
    return true;
}

void MainWindow::pushHistorySnapshot() {
    if (m_restoringHistory) return;
    const QByteArray snapshot = ProjectSerializer::serialize(m_model);
    if (!m_undoStack.isEmpty() && m_undoStack.last() == snapshot) { refreshActionStates(); return; }
    m_undoStack.push_back(snapshot);
    while (m_undoStack.size() > 50) m_undoStack.pop_front();
    m_redoStack.clear();
    refreshActionStates();
}

bool MainWindow::restoreSnapshot(const QByteArray& snapshot) {
    QString error;
    m_restoringHistory = true;
    const bool ok = ProjectSerializer::deserialize(m_model, snapshot, &error);
    m_restoringHistory = false;
    if (!ok) { QMessageBox::critical(this, "Restore Failed", error); return false; }
    m_selectedLinkId = -1;
    rebuildDeviceList(); rebuildTopology(); syncSelectedDeviceDetails(); syncLinkCreationControls();
    return true;
}

void MainWindow::refreshActionStates() {
    if (m_undoAction) m_undoAction->setEnabled(m_undoStack.size() > 1);
    if (m_redoAction) m_redoAction->setEnabled(!m_redoStack.isEmpty());
    updateStatusBar();
}

} // namespace packetlab
