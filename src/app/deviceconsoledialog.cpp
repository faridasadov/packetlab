#include "deviceconsoledialog.h"
#include "consolelineedit.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QStringList>
#include <QTabWidget>
#include <QTextEdit>
#include <QVBoxLayout>

namespace packetlab {

namespace {

QString portModeLabel(PortMode mode);

QString interfacesSummary(const Device& device) {
    QStringList lines;
    for (const auto& iface : device.interfaces()) {
        lines << QString("%1\t%2\t%3\t%4")
                     .arg(iface.name(),
                          iface.ipAddress().isEmpty() ? "unassigned" : iface.ipAddress(),
                          iface.subnetMask().isEmpty() ? "-" : iface.subnetMask(),
                          portModeLabel(iface.portMode()));
    }
    return lines.join('\n');
}

QString portModeLabel(PortMode mode) {
    switch (mode) {
    case PortMode::Routed: return "routed";
    case PortMode::Access: return "access";
    case PortMode::Trunk:  return "trunk";
    }
    return "routed";
}

AccessList* findAccessList(Device& device, const QString& name) {
    for (auto& acl : device.accessLists()) {
        if (acl.name.compare(name, Qt::CaseInsensitive) == 0) {
            return &acl;
        }
    }
    return nullptr;
}

}

DeviceConsoleDialog::DeviceConsoleDialog(NetworkModel& model, Simulator& simulator, int deviceId, QWidget* parent)
    : QDialog(parent, Qt::Window),
      m_model(model),
      m_simulator(simulator),
      m_deviceId(deviceId),
      m_cliOutput(new QTextEdit(this)),
      m_cliInput(new ConsoleLineEdit(this)),
      m_webView(new QTextEdit(this)),
      m_summaryView(new QTextEdit(this)) {
    const auto* dev = device();
    setAttribute(Qt::WA_DeleteOnClose, true);
    setMinimumSize(860, 620);
    resize(940, 680);
    setWindowTitle(dev ? QString("%1 Console").arg(dev->name()) : "Device Console");

    setStyleSheet(R"(
        QDialog { background: #0f172a; }
        QTabBar::tab {
            background: #172033;
            color: #cbd5e1;
            border: 1px solid #334155;
            border-radius: 6px;
            padding: 7px 14px;
            margin-right: 4px;
        }
        QTabBar::tab:selected { background: #2563eb; color: #eff6ff; border-color: #2563eb; }
        QTextEdit {
            background: #020617;
            color: #e2e8f0;
            border: 1px solid #1e293b;
            border-radius: 10px;
            padding: 10px;
            font-family: "Consolas", "Courier New", monospace;
            font-size: 12px;
        }
        QLineEdit {
            background: #020617;
            color: #e2e8f0;
            border: 1px solid #334155;
            border-radius: 8px;
            padding: 8px 10px;
            font-family: "Consolas", "Courier New", monospace;
            font-size: 12px;
        }
        QLabel {
            color: #94a3b8;
            font-size: 11px;
        }
        QPushButton {
            background: #1d4ed8;
            color: #eff6ff;
            border: none;
            border-radius: 8px;
            padding: 8px 14px;
            font-weight: 600;
        }
        QPushButton:hover { background: #2563eb; }
    )");

    auto* tabs = new QTabWidget(this);

    m_cliOutput->setReadOnly(true);
    m_webView->setReadOnly(true);
    m_summaryView->setReadOnly(true);
    m_webView->setAcceptRichText(true);

    auto* cliTab = new QWidget(this);
    auto* cliLayout = new QVBoxLayout(cliTab);
    auto* cliHint = new QLabel(
        "Supported commands: help, show ip interface brief, show ip route, show vlan brief, show interfaces trunk, show ip dhcp pool, show running-config, ping <ip>, clear", cliTab);
    auto* cliBar = new QHBoxLayout();
    auto* runBtn = new QPushButton("Run", cliTab);
    m_cliInput->setPlaceholderText(dev ? QString("%1>").arg(dev->name()) : "device>");
    cliBar->addWidget(m_cliInput, 1);
    cliBar->addWidget(runBtn);
    cliLayout->addWidget(cliHint);
    cliLayout->addWidget(m_cliOutput, 1);
    cliLayout->addLayout(cliBar);

    auto* webTab = new QWidget(this);
    auto* webLayout = new QVBoxLayout(webTab);
    webLayout->addWidget(m_webView, 1);

    auto* summaryTab = new QWidget(this);
    auto* summaryLayout = new QVBoxLayout(summaryTab);
    summaryLayout->addWidget(m_summaryView, 1);

    tabs->addTab(cliTab, "CLI");
    tabs->addTab(webTab, "Web UI");
    tabs->addTab(summaryTab, "Summary");

    auto* outer = new QVBoxLayout(this);
    outer->addWidget(tabs);

    connect(runBtn, &QPushButton::clicked, this, &DeviceConsoleDialog::executeCliCommand);
    connect(m_cliInput, &QLineEdit::returnPressed, this, &DeviceConsoleDialog::executeCliCommand);
    m_cliInput->setCompletionCandidates(completionCandidates());

    if (dev) {
        appendCliLine("PacketLab IOS-like console ready.", "#60a5fa");
        appendCliLine("Type 'help' to see supported commands.", "#94a3b8");
        refreshPanels();
    } else {
        appendCliLine("Device not found.", "#f87171");
        m_webView->setHtml("<h3>Device not found</h3>");
        m_summaryView->setPlainText("Device not found.");
    }
}

void DeviceConsoleDialog::executeCliCommand() {
    const QString command = m_cliInput->text().trimmed();
    if (command.isEmpty()) {
        return;
    }

    appendCliLine(commandPrompt() + command, "#93c5fd");
    const QString output = commandOutput(command);
    m_cliInput->addHistoryEntry(command);
    if (!output.isEmpty()) {
        for (const QString& line : output.split('\n')) {
            appendCliLine(line, output.contains("Unknown command") ? "#f87171" : "#e2e8f0");
        }
    }
    m_cliInput->clear();
}

void DeviceConsoleDialog::appendCliLine(const QString& text, const QString& color) {
    m_cliOutput->append(QString("<span style='color:%1'>%2</span>").arg(color, text.toHtmlEscaped()));
}

QString DeviceConsoleDialog::commandPrompt() const {
    const auto* dev = device();
    if (!dev) {
        return "device# ";
    }
    if (!m_inConfigMode) {
        return QString("%1# ").arg(dev->name());
    }
    if (!m_selectedDhcpPoolName.isEmpty()) {
        return QString("%1(dhcp-config)# ").arg(dev->name());
    }
    if (m_selectedVlanId >= 0) {
        return QString("%1(config-vlan-%2)# ").arg(dev->name()).arg(m_selectedVlanId);
    }
    if (m_selectedInterfaceName.isEmpty()) {
        return QString("%1(config)# ").arg(dev->name());
    }
    return QString("%1(config-if-%2)# ").arg(dev->name(), m_selectedInterfaceName);
}

QString DeviceConsoleDialog::commandOutput(const QString& command) {
    auto* dev = device();
    if (!dev) {
        return "Device not found.";
    }

    const QString normalized = command.trimmed().toLower();
    if (normalized == "help" || normalized == "?") {
        return "help\nshow ip interface brief\nshow ip route\nshow vlan brief\nshow interfaces trunk\nshow ip dhcp pool\nshow running-config\nping <ip>\nconf t\nhostname <name>\ninterface <name>\nvlan <id>\nip dhcp pool <name>\nip address <ip> <mask>\nip route <dest> <mask> <next-hop>\nnetwork <ip> <mask>\ndefault-router <ip>\ndns-server <ip>\ndefault-gateway <ip>\nexit\nend\nclear";
    }
    if (normalized == "clear") {
        m_cliOutput->clear();
        return QString();
    }
    if (normalized == "conf t" || normalized == "configure terminal") {
        m_inConfigMode = true;
        m_selectedInterfaceName.clear();
        m_selectedVlanId = -1;
        m_selectedDhcpPoolName.clear();
        return "Enter configuration commands, one per line. End with CNTL/Z.";
    }
    if (normalized == "end") {
        m_inConfigMode = false;
        m_selectedInterfaceName.clear();
        m_selectedVlanId = -1;
        m_selectedDhcpPoolName.clear();
        return "Leaving configuration mode.";
    }
    if (normalized == "exit") {
        if (!m_selectedInterfaceName.isEmpty()) {
            m_selectedInterfaceName.clear();
            return "Exit from interface configuration mode.";
        }
        if (m_selectedVlanId >= 0) {
            m_selectedVlanId = -1;
            return "Exit from VLAN configuration mode.";
        }
        if (!m_selectedDhcpPoolName.isEmpty()) {
            m_selectedDhcpPoolName.clear();
            return "Exit from DHCP pool configuration mode.";
        }
        if (m_inConfigMode) {
            m_inConfigMode = false;
            return "Exit from configuration mode.";
        }
        close();
        return "Connection closed by foreign host.";
    }
    if (normalized == "show ip interface brief") {
        QStringList lines;
        lines << "Interface\tIP-Address\tSubnet Mask\tStatus";
        lines << "------------------------------------------------------------";
        for (const auto& iface : dev->interfaces()) {
            lines << QString("%1\t%2\t%3\tup")
                         .arg(iface.name(),
                              iface.ipAddress().isEmpty() ? "unassigned" : iface.ipAddress(),
                              iface.subnetMask().isEmpty() ? "-" : iface.subnetMask());
        }
        return lines.join('\n');
    }
    if (normalized == "show ip route") {
        return m_simulator.simulateRouteTable(dev->id());
    }
    if (normalized == "show vlan brief") {
        if (!dev->isSwitchLike()) {
            return "VLAN table is available only on switch-class devices.";
        }
        QStringList lines;
        lines << "VLAN Name                             Status    Ports";
        for (const auto& vlan : dev->vlanDatabase()) {
            QStringList ports;
            for (const auto& iface : dev->interfaces()) {
                if (iface.portMode() == PortMode::Access && iface.accessVlan() == vlan.id) {
                    ports << iface.name();
                }
            }
            lines << QString("%1    %2    active    %3")
                         .arg(vlan.id, -4)
                         .arg(vlan.name, -30)
                         .arg(ports.join(", "));
        }
        return lines.join('\n');
    }
    if (normalized == "show interfaces trunk") {
        if (!dev->isSwitchLike()) {
            return "Trunk table is available only on switch-class devices.";
        }
        QStringList lines;
        lines << "Port        Mode     Native VLAN  Allowed VLANs";
        lines << "-----------------------------------------------";
        for (const auto& iface : dev->interfaces()) {
            if (iface.portMode() != PortMode::Trunk) {
                continue;
            }
            lines << QString("%1    trunk    %2           %3")
                         .arg(iface.name(), -10)
                         .arg(iface.nativeVlan(), -4)
                         .arg(iface.allowedVlans());
        }
        if (lines.size() == 2) {
            lines << "(no trunk ports)";
        }
        return lines.join('\n');
    }
    if (normalized == "show ip dhcp pool") {
        QStringList lines;
        lines << "Pool Name        Network            Mask               Default Router     DNS";
        lines << "--------------------------------------------------------------------------";
        for (const auto& pool : dev->dhcpPools()) {
            lines << QString("%1 %2 %3 %4 %5")
                         .arg(pool.name, -16)
                         .arg(pool.network.isEmpty() ? "-" : pool.network, -18)
                         .arg(pool.mask.isEmpty() ? "-" : pool.mask, -18)
                         .arg(pool.defaultRouter.isEmpty() ? "-" : pool.defaultRouter, -18)
                         .arg(pool.dnsServer.isEmpty() ? "-" : pool.dnsServer);
        }
        if (lines.size() == 2) {
            lines << "(no DHCP pools)";
        }
        return lines.join('\n');
    }
    if (normalized == "show access-lists") {
        QStringList lines;
        for (const auto& acl : dev->accessLists()) {
            lines << QString("Extended IP access list %1").arg(acl.name);
            for (const auto& rule : acl.rules) {
                lines << QString("    %1 %2 %3 %4")
                             .arg(rule.action, rule.protocol, rule.source, rule.destination);
            }
        }
        if (lines.isEmpty()) {
            lines << "(no access lists)";
        }
        return lines.join('\n');
    }
    if (normalized == "show running-config") {
        QStringList lines;
        lines << "hostname " + dev->name();
        if (dev->isSwitchLike()) {
            for (const auto& vlan : dev->vlanDatabase()) {
                lines << QString("vlan %1").arg(vlan.id);
                lines << QString(" name %1").arg(vlan.name);
                lines << "!";
            }
        }
        lines << "!";
        for (const auto& iface : dev->interfaces()) {
            lines << "interface " + iface.name();
            if (dev->isSwitchLike()) {
                lines << " switchport";
                lines << QString(" switchport mode %1").arg(portModeLabel(iface.portMode()));
                if (iface.portMode() == PortMode::Access) {
                    lines << QString(" switchport access vlan %1").arg(iface.accessVlan());
                } else if (iface.portMode() == PortMode::Trunk) {
                    lines << QString(" switchport trunk native vlan %1").arg(iface.nativeVlan());
                    lines << QString(" switchport trunk allowed vlan %1").arg(iface.allowedVlans());
                }
            }
            if (!iface.ipAddress().isEmpty()) {
                lines << QString(" ip address %1 %2").arg(iface.ipAddress(), iface.subnetMask());
            }
            lines << " no shutdown";
            lines << "!";
        }
        if (!dev->routingTable().empty()) {
            for (const auto& route : dev->routingTable()) {
                lines << QString("ip route %1 %2 %3").arg(route.destination, route.mask, route.nextHop);
            }
        }
        for (const auto& pool : dev->dhcpPools()) {
            lines << QString("ip dhcp pool %1").arg(pool.name);
            if (!pool.network.isEmpty() && !pool.mask.isEmpty())
                lines << QString(" network %1 %2").arg(pool.network, pool.mask);
            if (!pool.defaultRouter.isEmpty())
                lines << QString(" default-router %1").arg(pool.defaultRouter);
            if (!pool.dnsServer.isEmpty())
                lines << QString(" dns-server %1").arg(pool.dnsServer);
            lines << "!";
        }
        for (const auto& acl : dev->accessLists()) {
            for (const auto& rule : acl.rules) {
                lines << QString("access-list %1 %2 %3 %4 %5")
                             .arg(acl.name, rule.action, rule.protocol, rule.source, rule.destination);
            }
        }
        return lines.join('\n');
    }
    if (normalized.startsWith("ping ")) {
        return m_simulator.simulatePing(dev->id(), command.mid(5).trimmed());
    }
    if (m_inConfigMode) {
        if (normalized.startsWith("hostname ")) {
            const QString newName = command.mid(command.indexOf(' ') + 1).trimmed();
            if (newName.isEmpty()) {
                return "Hostname cannot be empty.";
            }
            dev->setName(newName);
            refreshPanels();
            emit modelMutated(m_deviceId);
            return "Hostname updated.";
        }
        if (normalized.startsWith("interface ")) {
            const QString interfaceName = command.mid(command.indexOf(' ') + 1).trimmed();
            for (const auto& iface : dev->interfaces()) {
                if (iface.name().compare(interfaceName, Qt::CaseInsensitive) == 0) {
                    m_selectedInterfaceName = iface.name();
                    m_selectedDhcpPoolName.clear();
                    m_selectedVlanId = -1;
                    return QString("Configuring interface %1").arg(iface.name());
                }
            }
            return QString("Interface %1 not found.").arg(interfaceName);
        }
        if (normalized.startsWith("vlan ")) {
            if (!dev->isSwitchLike()) {
                return "VLAN configuration is only supported on switch-class devices.";
            }
            bool parsed = false;
            const int vlanId = command.mid(command.indexOf(' ') + 1).trimmed().toInt(&parsed);
            if (!parsed || vlanId <= 0) {
                return "Usage: vlan <id>";
            }
            bool exists = false;
            for (const auto& vlan : dev->vlanDatabase()) {
                if (vlan.id == vlanId) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                dev->vlanDatabase().push_back({vlanId, QString("VLAN%1").arg(vlanId)});
            }
            m_selectedVlanId = vlanId;
            m_selectedInterfaceName.clear();
            refreshPanels();
            emit modelMutated(m_deviceId);
            return QString("VLAN %1 ready for configuration.").arg(vlanId);
        }
        if (normalized.startsWith("ip dhcp pool ")) {
            if (!dev->isL3Capable()) {
                return "DHCP pool configuration is only supported on L3-capable devices.";
            }
            const QString poolName = command.mid(QString("ip dhcp pool ").size()).trimmed();
            if (poolName.isEmpty()) {
                return "Usage: ip dhcp pool <name>";
            }
            bool exists = false;
            for (const auto& pool : dev->dhcpPools()) {
                if (pool.name.compare(poolName, Qt::CaseInsensitive) == 0) {
                    exists = true;
                    break;
                }
            }
            if (!exists) {
                dev->dhcpPools().push_back({poolName, "", "", "", ""});
            }
            m_selectedDhcpPoolName = poolName;
            m_selectedInterfaceName.clear();
            m_selectedVlanId = -1;
            refreshPanels();
            emit modelMutated(m_deviceId);
            return QString("DHCP pool %1 ready for configuration.").arg(poolName);
        }
        if (normalized.startsWith("access-list ")) {
            const QStringList parts = command.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (parts.size() < 6) {
                return "Usage: access-list <name> permit|deny ip <source> <destination>";
            }
            const QString aclName = parts[1];
            const QString action = parts[2].toLower();
            const QString protocol = parts[3].toLower();
            if (action != "permit" && action != "deny") {
                return "ACL action must be permit or deny.";
            }
            if (protocol != "ip") {
                return "Only 'ip' protocol is supported right now.";
            }
            QString source;
            QString destination;
            if (parts[4].toLower() == "any") {
                source = "any";
                destination = parts.mid(5).join(' ');
            } else if (parts[4].toLower() == "host" && parts.size() >= 8) {
                source = QString("host %1").arg(parts[5]);
                destination = parts.mid(6).join(' ');
            } else {
                if (parts.size() < 7) {
                    return "Source must be 'any', 'host <ip>', or '<network> <wildcard>'.";
                }
                source = QString("%1 %2").arg(parts[4], parts[5]);
                destination = parts.mid(6).join(' ');
            }
            auto* acl = findAccessList(*dev, aclName);
            if (!acl) {
                dev->accessLists().push_back({aclName, {}});
                acl = &dev->accessLists().back();
            }
            acl->rules.push_back({action, protocol, source.trimmed(), destination.trimmed()});
            refreshPanels();
            emit modelMutated(m_deviceId);
            return QString("ACL rule added to %1.").arg(aclName);
        }
        if (normalized.startsWith("name ") && m_selectedVlanId >= 0) {
            const QString vlanName = command.mid(command.indexOf(' ') + 1).trimmed();
            for (auto& vlan : dev->vlanDatabase()) {
                if (vlan.id == m_selectedVlanId) {
                    vlan.name = vlanName;
                    refreshPanels();
                    emit modelMutated(m_deviceId);
                    return QString("VLAN %1 renamed.").arg(m_selectedVlanId);
                }
            }
            return "Selected VLAN not found.";
        }
        if (normalized.startsWith("network ") && !m_selectedDhcpPoolName.isEmpty()) {
            const QStringList parts = command.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (parts.size() < 3) {
                return "Usage: network <ip> <mask>";
            }
            for (auto& pool : dev->dhcpPools()) {
                if (pool.name.compare(m_selectedDhcpPoolName, Qt::CaseInsensitive) == 0) {
                    pool.network = parts[1];
                    pool.mask = parts[2];
                    refreshPanels();
                    emit modelMutated(m_deviceId);
                    return "DHCP network updated.";
                }
            }
            return "Selected DHCP pool not found.";
        }
        if (normalized.startsWith("default-router ") && !m_selectedDhcpPoolName.isEmpty()) {
            const QString value = command.mid(QString("default-router ").size()).trimmed();
            for (auto& pool : dev->dhcpPools()) {
                if (pool.name.compare(m_selectedDhcpPoolName, Qt::CaseInsensitive) == 0) {
                    pool.defaultRouter = value;
                    refreshPanels();
                    emit modelMutated(m_deviceId);
                    return "DHCP default-router updated.";
                }
            }
            return "Selected DHCP pool not found.";
        }
        if (normalized.startsWith("dns-server ") && !m_selectedDhcpPoolName.isEmpty()) {
            const QString value = command.mid(QString("dns-server ").size()).trimmed();
            for (auto& pool : dev->dhcpPools()) {
                if (pool.name.compare(m_selectedDhcpPoolName, Qt::CaseInsensitive) == 0) {
                    pool.dnsServer = value;
                    refreshPanels();
                    emit modelMutated(m_deviceId);
                    return "DHCP DNS server updated.";
                }
            }
            return "Selected DHCP pool not found.";
        }
        if (normalized.startsWith("default-gateway ")) {
            dev->setDefaultGateway(command.mid(command.indexOf(' ') + 1).trimmed());
            refreshPanels();
            emit modelMutated(m_deviceId);
            return "Default gateway updated.";
        }
        if (normalized.startsWith("ip route ")) {
            const QStringList parts = command.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (parts.size() < 5) {
                return "Usage: ip route <destination> <mask> <next-hop>";
            }
            dev->routingTable().push_back({parts[2], parts[3], parts[4], 1});
            refreshPanels();
            emit modelMutated(m_deviceId);
            return "Static route added.";
        }
        if (normalized.startsWith("ip address ")) {
            auto* iface = selectedInterface();
            if (!iface) {
                return "Select an interface first: interface <name>";
            }
            const QStringList parts = command.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (parts.size() < 4) {
                return "Usage: ip address <ip> <mask>";
            }
            iface->setIpAddress(parts[2]);
            iface->setSubnetMask(parts[3]);
            if (dev->ipAddress().isEmpty()) {
                dev->setIpAddress(parts[2]);
                dev->setSubnetMask(parts[3]);
            }
            refreshPanels();
            emit modelMutated(m_deviceId);
            return QString("Assigned %1 %2 to %3").arg(parts[2], parts[3], iface->name());
        }
        if (normalized == "switchport") {
            if (!selectedInterface()) {
                return "Select an interface first: interface <name>";
            }
            return "Switchport context acknowledged.";
        }
        if (normalized.startsWith("switchport mode ")) {
            auto* iface = selectedInterface();
            if (!iface) {
                return "Select an interface first: interface <name>";
            }
            const QString mode = normalized.mid(QString("switchport mode ").size());
            if (mode == "access") {
                iface->setPortMode(PortMode::Access);
            } else if (mode == "trunk") {
                iface->setPortMode(PortMode::Trunk);
            } else {
                return "Supported modes: access, trunk";
            }
            refreshPanels();
            emit modelMutated(m_deviceId);
            return QString("Port mode set to %1.").arg(mode);
        }
        if (normalized.startsWith("switchport access vlan ")) {
            auto* iface = selectedInterface();
            if (!iface) {
                return "Select an interface first: interface <name>";
            }
            bool parsed = false;
            const int vlanId = command.mid(QString("switchport access vlan ").size()).trimmed().toInt(&parsed);
            if (!parsed || vlanId <= 0) {
                return "Usage: switchport access vlan <id>";
            }
            iface->setPortMode(PortMode::Access);
            iface->setAccessVlan(vlanId);
            bool exists = false;
            for (const auto& vlan : dev->vlanDatabase()) {
                if (vlan.id == vlanId) {
                    exists = true;
                    break;
                }
            }
            if (!exists && dev->isSwitchLike()) {
                dev->vlanDatabase().push_back({vlanId, QString("VLAN%1").arg(vlanId)});
            }
            refreshPanels();
            emit modelMutated(m_deviceId);
            return QString("Access VLAN set to %1.").arg(vlanId);
        }
        if (normalized.startsWith("switchport trunk native vlan ")) {
            auto* iface = selectedInterface();
            if (!iface) {
                return "Select an interface first: interface <name>";
            }
            bool parsed = false;
            const int vlanId = command.mid(QString("switchport trunk native vlan ").size()).trimmed().toInt(&parsed);
            if (!parsed || vlanId <= 0) {
                return "Usage: switchport trunk native vlan <id>";
            }
            iface->setPortMode(PortMode::Trunk);
            iface->setNativeVlan(vlanId);
            refreshPanels();
            emit modelMutated(m_deviceId);
            return QString("Trunk native VLAN set to %1.").arg(vlanId);
        }
        if (normalized.startsWith("switchport trunk allowed vlan ")) {
            auto* iface = selectedInterface();
            if (!iface) {
                return "Select an interface first: interface <name>";
            }
            const QString value = command.mid(QString("switchport trunk allowed vlan ").size()).trimmed();
            if (value.isEmpty()) {
                return "Usage: switchport trunk allowed vlan <list>";
            }
            iface->setPortMode(PortMode::Trunk);
            iface->setAllowedVlans(value);
            refreshPanels();
            emit modelMutated(m_deviceId);
            return "Trunk allowed VLAN list updated.";
        }
        if (normalized.startsWith("ip access-group ")) {
            auto* iface = selectedInterface();
            if (!iface) {
                return "Select an interface first: interface <name>";
            }
            const QStringList parts = command.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (parts.size() < 3) {
                return "Usage: ip access-group <acl> in";
            }
            if (parts.size() < 4 || parts[3].toLower() != "in") {
                return "Only inbound ACL direction is supported right now.";
            }
            if (!findAccessList(*dev, parts[2])) {
                return QString("ACL %1 not found.").arg(parts[2]);
            }
            iface->setInboundAcl(parts[2]);
            refreshPanels();
            emit modelMutated(m_deviceId);
            return QString("Inbound ACL %1 applied on %2.").arg(parts[2], iface->name());
        }
        if (normalized == "no shutdown") {
            return "Interface enabled.";
        }
    }
    return QString("Unknown command: %1").arg(command);
}

void DeviceConsoleDialog::refreshPanels() {
    const auto* dev = device();
    if (!dev) {
        return;
    }
    setWindowTitle(QString("%1 Console").arg(dev->name()));
    m_cliInput->setPlaceholderText(QString("%1>").arg(dev->name()));
    m_cliInput->setCompletionCandidates(completionCandidates());
    m_webView->setHtml(webMarkup());
    m_summaryView->setPlainText(
        QString("Device: %1\nType: %2\nConfig Mode: %3\nSelected Interface: %4\nSelected VLAN: %5\nSelected DHCP Pool: %6\nDHCP Pools: %7\n\nInterfaces\n%8")
            .arg(dev->name(),
                 dev->typeLabel(),
                 m_inConfigMode ? "yes" : "no",
                 m_selectedInterfaceName.isEmpty() ? "-" : m_selectedInterfaceName,
                 m_selectedVlanId < 0 ? "-" : QString::number(m_selectedVlanId),
                 m_selectedDhcpPoolName.isEmpty() ? "-" : m_selectedDhcpPoolName,
                 QString::number(dev->dhcpPools().size()),
                 interfacesSummary(*dev)));
}

QStringList DeviceConsoleDialog::completionCandidates() const {
    QStringList items{
        "help",
        "show ip interface brief",
        "show ip route",
        "show vlan brief",
        "show interfaces trunk",
        "show ip dhcp pool",
        "show access-lists",
        "show running-config",
        "ping ",
        "conf t",
        "configure terminal",
        "hostname ",
        "interface ",
        "vlan ",
        "ip dhcp pool ",
        "access-list ",
        "default-gateway ",
        "ip route ",
        "exit",
        "end",
        "clear"
    };
    if (m_inConfigMode) {
        items << "ip address " << "no shutdown";
        items << "switchport mode access" << "switchport mode trunk";
        items << "switchport access vlan " << "switchport trunk native vlan " << "switchport trunk allowed vlan ";
        items << "ip access-group ";
        items << "network " << "default-router " << "dns-server ";
    }
    const auto* dev = device();
    if (dev) {
        for (const auto& iface : dev->interfaces()) {
            items << QString("interface %1").arg(iface.name());
        }
        for (const auto& acl : dev->accessLists()) {
            items << QString("ip access-group %1 in").arg(acl.name);
        }
    }
    return items;
}

QString DeviceConsoleDialog::webMarkup() const {
    const auto* dev = device();
    if (!dev) {
        return "<h3>Device not found</h3>";
    }

    QString rows;
    for (const auto& iface : dev->interfaces()) {
        rows += QString("<tr><td>%1</td><td>%2</td><td>%3</td><td>up</td></tr>")
                    .arg(iface.name().toHtmlEscaped(),
                         (iface.ipAddress().isEmpty() ? "-" : iface.ipAddress()).toHtmlEscaped(),
                         (iface.subnetMask().isEmpty() ? "-" : iface.subnetMask()).toHtmlEscaped());
    }

    return QString(R"(
        <html>
        <body style="background:#f8fafc;font-family:Segoe UI,Arial,sans-serif;color:#0f172a;">
            <div style="max-width:860px;margin:0 auto;">
                <div style="background:linear-gradient(135deg,#1d4ed8,#0f172a);padding:18px 22px;border-radius:16px;color:white;">
                    <div style="font-size:12px;opacity:.8;">PacketLab Embedded Management</div>
                    <div style="font-size:28px;font-weight:700;">%1</div>
                    <div style="font-size:14px;opacity:.92;">%2</div>
                </div>
                <div style="display:flex;gap:16px;margin-top:16px;">
                    <div style="flex:1;background:white;border:1px solid #dbe4f0;border-radius:14px;padding:16px;">
                        <h3 style="margin-top:0;">Management</h3>
                        <p><b>IP:</b> %3</p>
                        <p><b>Gateway:</b> %4</p>
                        <p><b>Interfaces:</b> %5</p>
                    </div>
                    <div style="flex:1;background:white;border:1px solid #dbe4f0;border-radius:14px;padding:16px;">
                        <h3 style="margin-top:0;">Status</h3>
                        <p>PacketLab simulates a lightweight device web panel for topology inspection.</p>
                        <p>CLI tab remains the primary configuration surface.</p>
                    </div>
                </div>
                <div style="background:white;border:1px solid #dbe4f0;border-radius:14px;padding:16px;margin-top:16px;">
                    <h3 style="margin-top:0;">Interface Table</h3>
                    <table style="width:100%%;border-collapse:collapse;">
                        <thead>
                            <tr style="background:#eff6ff;">
                                <th style="text-align:left;padding:8px;border-bottom:1px solid #dbe4f0;">Interface</th>
                                <th style="text-align:left;padding:8px;border-bottom:1px solid #dbe4f0;">IP Address</th>
                                <th style="text-align:left;padding:8px;border-bottom:1px solid #dbe4f0;">Mask</th>
                                <th style="text-align:left;padding:8px;border-bottom:1px solid #dbe4f0;">Status</th>
                            </tr>
                        </thead>
                        <tbody>%6</tbody>
                    </table>
                </div>
            </div>
        </body>
        </html>)")
        .arg(dev->name().toHtmlEscaped(),
             dev->typeLabel().toHtmlEscaped(),
             (dev->ipAddress().isEmpty() ? "-" : dev->ipAddress()).toHtmlEscaped(),
             (dev->defaultGateway().isEmpty() ? "-" : dev->defaultGateway()).toHtmlEscaped(),
             QString::number(dev->interfaces().size()),
             rows);
}

const Device* DeviceConsoleDialog::device() const {
    return m_model.findDevice(m_deviceId);
}

Device* DeviceConsoleDialog::device() {
    return m_model.findDevice(m_deviceId);
}

NetworkInterface* DeviceConsoleDialog::selectedInterface() {
    auto* dev = device();
    if (!dev || m_selectedInterfaceName.isEmpty()) {
        return nullptr;
    }
    for (auto& iface : dev->interfaces()) {
        if (iface.name().compare(m_selectedInterfaceName, Qt::CaseInsensitive) == 0) {
            return &iface;
        }
    }
    return nullptr;
}

} // namespace packetlab
