#include "simulator.h"

#include <algorithm>
#include <QList>
#include <QSet>
#include <QHash>
#include <QRegularExpression>
#include <QStringList>

namespace packetlab {

namespace {

bool deviceOwnsIp(const Device& device, const QString& ip) {
    if (device.ipAddress() == ip) {
        return true;
    }
    for (const auto& iface : device.interfaces()) {
        if (iface.ipAddress() == ip) {
            return true;
        }
    }
    return false;
}

}

Simulator::Simulator(NetworkModel& model) : m_model(model), m_tick(0) {}

QString Simulator::runTick() {
    ++m_tick;
    return QString("Simulation tick %1, active devices: %2")
        .arg(m_tick)
        .arg(m_model.devices().size());
}

// ── ping ─────────────────────────────────────────────────────────────────────

QString Simulator::simulatePing(int sourceDeviceId, const QString& targetIp) const {
    const Device* source = m_model.findDevice(sourceDeviceId);
    if (!source)
        return "Ping failed: source device not found.";
    const QString target = targetIp.trimmed();
    if (target.isEmpty())
        return "Ping failed: target IP is empty.";

    const TraceResult result = traceRoute(*source, target);
    QStringList lines;
    lines << QString("Pinging %1 from %2:").arg(target, source->name());
    lines << "";
    if (result.success) {
        for (int i = 1; i <= 4; ++i)
            lines << QString("Reply from %1: bytes=32 time=1ms TTL=64").arg(target);
        lines << "";
        lines << "Ping statistics for " + target + ":";
        lines << "    Packets: Sent = 4, Received = 4, Lost = 0 (0% loss)";
        lines << "Approximate round trip times in milli-seconds:";
        lines << "    Minimum = 1ms, Maximum = 1ms, Average = 1ms";
    } else {
        for (int i = 1; i <= 4; ++i)
            lines << QString("Request timeout for icmp_seq %1").arg(i);
        lines << "";
        lines << "Ping statistics for " + target + ":";
        lines << "    Packets: Sent = 4, Received = 0, Lost = 4 (100% loss)";
        lines << "";
        lines << "FAILED: " + result.error;
    }
    return lines.join('\n');
}

// ── traceroute ───────────────────────────────────────────────────────────────

QString Simulator::simulateTraceroute(int sourceDeviceId, const QString& targetIp) const {
    const Device* source = m_model.findDevice(sourceDeviceId);
    if (!source)
        return "Traceroute failed: source device not found.";
    const QString target = targetIp.trimmed();
    if (target.isEmpty())
        return "Traceroute failed: target IP is empty.";

    const TraceResult result = traceRoute(*source, target);
    QStringList lines;
    lines << QString("Tracing route to %1 from %2:").arg(target, source->name());
    lines << "Over a maximum of 16 hops:";
    lines << "";

    if (result.success) {
        for (int i = 0; i < result.hops.size(); ++i) {
            const QString& hop = result.hops[i];
            // Try to find device name for this IP
            QString label = hop;
            for (const auto& dev : m_model.devices()) {
                if (dev.ipAddress() == hop) {
                    label = QString("%1 (%2)").arg(dev.name(), hop);
                    break;
                }
            }
            lines << QString("  %1  <1ms  <1ms  <1ms  %2").arg(i + 1).arg(label);
        }
        lines << "";
        lines << "Trace complete.";
    } else {
        lines << "  1  *  *  *  Request timed out.";
        lines << "";
        lines << "Trace failed: " + result.error;
    }
    return lines.join('\n');
}

// ── ARP table ────────────────────────────────────────────────────────────────

QString Simulator::simulateArpTable(int deviceId) const {
    const Device* dev = m_model.findDevice(deviceId);
    if (!dev)
        return "ARP: device not found.";

    QStringList lines;
    lines << QString("ARP cache for %1 (%2):").arg(dev->name(), dev->ipAddress());
    lines << "";
    lines << QString("%1 %2 %3").arg(QString("Internet Address"), -18).arg(QString("Physical Address"), -20).arg(QString("Type"), -10);
    lines << QString(50, '-');

    bool any = false;
    for (const auto& other : m_model.devices()) {
        if (other.id() == dev->id()) continue;
        if (other.ipAddress().isEmpty()) continue;
        if (!sameSubnet(dev->ipAddress(), other.ipAddress(), dev->subnetMask())) continue;
        if (!areConnected(dev->id(), other.id())) continue;

        // Fake MAC: AA-BB-CC-00-00-XX where XX = device id in hex
        const QString mac = QString("AA-BB-CC-00-00-%1").arg(other.id(), 2, 16, QChar('0')).toUpper();
        lines << QString("%1 %2 %3").arg(other.ipAddress(), -18).arg(mac, -20).arg(QString("dynamic"), -10);
        any = true;
    }

    if (!any)
        lines << "(no entries)";

    return lines.join('\n');
}

// ── route table ──────────────────────────────────────────────────────────────

QString Simulator::simulateRouteTable(int deviceId) const {
    const Device* dev = m_model.findDevice(deviceId);
    if (!dev)
        return "Route table: device not found.";
    if (!dev->isL3Capable())
        return QString("Route table: %1 is not an L3 device (Router, L3 Switch, or Firewall).").arg(dev->name());

    QStringList lines;
    lines << QString("Routing table for %1:").arg(dev->name());
    lines << "";
    auto fmtRow = [](const QString& code, const QString& dest, const QString& mask,
                     const QString& nexthop, const QString& metric) -> QString {
        return QString("%1 %2 %3 %4 %5")
            .arg(code,    -5)
            .arg(dest,   -18)
            .arg(mask,   -18)
            .arg(nexthop,-18)
            .arg(metric);
    };
    lines << fmtRow("Code", "Destination", "Mask", "Next Hop", "Metric");
    lines << QString(75, '-');

    // Connected routes from interfaces
    for (const auto& iface : dev->interfaces()) {
        if (iface.ipAddress().isEmpty() || iface.subnetMask().isEmpty()) continue;
        bool ok1, ok2;
        const quint32 ip   = ipv4ToInt(iface.ipAddress(), &ok1);
        const quint32 mask = ipv4ToInt(iface.subnetMask(), &ok2);
        if (!ok1 || !ok2) continue;
        const quint32 net = ip & mask;
        const QString netStr = QString("%1.%2.%3.%4")
            .arg((net >> 24) & 0xFF).arg((net >> 16) & 0xFF)
            .arg((net >>  8) & 0xFF).arg(net & 0xFF);
        lines << fmtRow("C", netStr, iface.subnetMask(), "directly connected", "0");
    }

    // Static routes
    for (const auto& route : dev->routingTable()) {
        lines << fmtRow("S", route.destination, route.mask, route.nextHop,
                        QString::number(route.metric));
    }

    if (lines.size() <= 3)
        lines << "(no routes)";

    return lines.join('\n');
}

// ── connectivity matrix ───────────────────────────────────────────────────────

QString Simulator::simulateConnectivityMatrix() const {
    // collect devices with IPs
    QList<const Device*> devs;
    for (const auto& d : m_model.devices()) {
        if (!d.ipAddress().isEmpty())
            devs.append(&d);
    }

    if (devs.isEmpty())
        return "Connectivity Matrix: no devices with IP addresses.";

    QStringList lines;
    lines << "Connectivity Matrix (● = reachable, ○ = unreachable):";
    lines << "";

    // Header row: short names
    const int col = 8;
    QString header = QString("%1").arg("", col);
    for (const auto* d : devs)
        header += QString("%1").arg(d->name().left(col - 1), -(col - 1)).left(col - 1) + " ";
    lines << header;
    lines << QString(header.length(), '-');

    for (const auto* src : devs) {
        QString row = QString("%1").arg(src->name().left(col - 1), -(col - 1)).left(col - 1) + " ";
        for (const auto* dst : devs) {
            QString cell;
            if (src->id() == dst->id()) {
                cell = "─";
            } else {
                const TraceResult r = traceRoute(*src, dst->ipAddress());
                cell = r.success ? "●" : "○";
            }
            row += QString("%1").arg(cell, -(col - 1)).left(col - 1) + " ";
        }
        lines << row;
    }

    return lines.join('\n');
}

// ── subnet audit ─────────────────────────────────────────────────────────────

QString Simulator::simulateSubnetAudit() const {
    QStringList lines;
    lines << "═══════════════════════════════════════";
    lines << "         SUBNET AUDIT REPORT           ";
    lines << "═══════════════════════════════════════";
    lines << "";

    const auto& devices = m_model.devices();

    // 1. Duplicate IPs
    QHash<QString, QStringList> ipMap;
    for (const auto& d : devices) {
        if (!d.ipAddress().isEmpty())
            ipMap[d.ipAddress()].append(d.name());
    }
    bool dupFound = false;
    for (auto it = ipMap.begin(); it != ipMap.end(); ++it) {
        if (it.value().size() > 1) {
            lines << QString("[✗] Duplicate IP %1: %2").arg(it.key(), it.value().join(", "));
            dupFound = true;
        }
    }
    if (!dupFound)
        lines << "[✓] No duplicate IP addresses.";

    // 2. Isolated devices (no links)
    bool isoFound = false;
    for (const auto& d : devices) {
        bool linked = false;
        for (const auto& link : m_model.links()) {
            if (link.leftDeviceId() == d.id() || link.rightDeviceId() == d.id()) {
                linked = true;
                break;
            }
        }
        if (!linked) {
            lines << QString("[✗] Isolated device (no links): %1").arg(d.name());
            isoFound = true;
        }
    }
    if (!isoFound)
        lines << "[✓] All devices have at least one link.";

    // 3. Invalid gateways
    bool gwFound = false;
    for (const auto& d : devices) {
        if (d.defaultGateway().isEmpty()) continue;
        bool gwExists = false;
        for (const auto& other : devices) {
            if (other.ipAddress() == d.defaultGateway()) { gwExists = true; break; }
            for (const auto& iface : other.interfaces()) {
                if (iface.ipAddress() == d.defaultGateway()) { gwExists = true; break; }
            }
            if (gwExists) break;
        }
        if (!gwExists) {
            lines << QString("[✗] %1 has gateway %2 which is not found in topology.")
                       .arg(d.name(), d.defaultGateway());
            gwFound = true;
        }
    }
    if (!gwFound)
        lines << "[✓] All configured gateways exist in topology.";

    // 4. End devices with no IP
    bool noIpFound = false;
    for (const auto& d : devices) {
        if (!d.isL3Capable() && d.type() != DeviceType::Switch && d.type() != DeviceType::SwitchL3) {
            if (d.ipAddress().isEmpty()) {
                lines << QString("[✗] End device with no IP: %1").arg(d.name());
                noIpFound = true;
            }
        }
    }
    if (!noIpFound)
        lines << "[✓] All end devices have IP addresses assigned.";

    lines << "";
    lines << "─────────────────────────────────────";
    const int total = lines.filter("[✗]").size();
    lines << (total == 0
        ? "Audit complete — no issues found."
        : QString("Audit complete — %1 issue(s) found.").arg(total));

    return lines.join('\n');
}

// ── private helpers ───────────────────────────────────────────────────────────

Simulator::TraceResult Simulator::traceRoute(const Device& source, const QString& targetIp) const {
    TraceResult result;

    // Find target device
    const Device* targetDev = nullptr;
    for (const auto& d : m_model.devices()) {
        if (deviceOwnsIp(d, targetIp)) { targetDev = &d; break; }
    }
    if (!targetDev) {
        result.error = QString("No device owns IP %1.").arg(targetIp);
        return result;
    }

    QSet<int> visited;
    const Device* current = &source;
    int iterations = 0;

    while (iterations++ < 16) {
        if (current->ipAddress() == targetIp) {
            result.hops.append(current->ipAddress());
            result.success = true;
            return result;
        }

        // Direct reach?
        if (deviceCanDirectlyReach(*current, targetIp) && areConnected(current->id(), targetDev->id())) {
            const int inboundInterface = connectedInterfaceIndex(current->id(), targetDev->id());
            if (!aclAllowsTraffic(*targetDev, inboundInterface, source.ipAddress(), targetIp)) {
                result.error = QString("ACL on %1 denied traffic to %2.").arg(targetDev->name(), targetIp);
                return result;
            }
            if (!result.hops.isEmpty() && result.hops.last() != current->ipAddress())
                result.hops.append(current->ipAddress());
            else if (result.hops.isEmpty())
                result.hops.append(current->ipAddress());
            result.hops.append(targetIp);
            result.success = true;
            return result;
        }

        const Device* next = resolveNextHop(*current, targetIp);
        if (!next) {
            result.error = QString("No route from %1 to %2.").arg(current->name(), targetIp);
            return result;
        }
        if (!areConnected(current->id(), next->id())) {
            result.error = QString("%1 and %2 are not connected.").arg(current->name(), next->name());
            return result;
        }
        const int inboundInterface = connectedInterfaceIndex(current->id(), next->id());
        if (!aclAllowsTraffic(*next, inboundInterface, source.ipAddress(), targetIp)) {
            result.error = QString("ACL on %1 denied traffic to %2.").arg(next->name(), targetIp);
            return result;
        }
        if (visited.contains(next->id())) {
            result.error = "Routing loop detected.";
            return result;
        }

        result.hops.append(current->ipAddress().isEmpty() ? current->name() : current->ipAddress());
        visited.insert(current->id());
        current = next;
    }

    result.error = "TTL exceeded (max 16 hops).";
    return result;
}

const Device* Simulator::resolveNextHop(const Device& current, const QString& targetIp) const {
    if (current.isL3Capable()) {
        // LPM over routing table
        bool okTarget;
        const quint32 targetInt = ipv4ToInt(targetIp, &okTarget);
        if (!okTarget) return nullptr;

        quint32 bestMask = 0;
        QString bestNextHopIp;
        for (const auto& route : current.routingTable()) {
            bool okMask, okDest;
            const quint32 mask = ipv4ToInt(route.mask, &okMask);
            const quint32 dest = ipv4ToInt(route.destination, &okDest);
            if (!okMask || !okDest) continue;
            if ((targetInt & mask) == (dest & mask)) {
                if (mask > bestMask) {
                    bestMask = mask;
                    bestNextHopIp = route.nextHop;
                }
            }
        }
        if (!bestNextHopIp.isEmpty()) {
            for (const auto& d : m_model.devices()) {
                if (deviceOwnsIp(d, bestNextHopIp)) return &d;
            }
        }
    }

    // Fallback: default gateway
    if (!current.defaultGateway().isEmpty()) {
        for (const auto& d : m_model.devices()) {
            if (deviceOwnsIp(d, current.defaultGateway())) return &d;
        }
    }
    return nullptr;
}

bool Simulator::deviceCanDirectlyReach(const Device& device, const QString& targetIp) const {
    // Check main IP/mask
    if (!device.ipAddress().isEmpty() && !device.subnetMask().isEmpty()) {
        if (sameSubnet(device.ipAddress(), targetIp, device.subnetMask()))
            return true;
    }
    // Check each interface
    for (const auto& iface : device.interfaces()) {
        if (!iface.ipAddress().isEmpty() && !iface.subnetMask().isEmpty()) {
            if (sameSubnet(iface.ipAddress(), targetIp, iface.subnetMask()))
                return true;
        }
    }
    return false;
}

bool Simulator::areConnected(int deviceIdA, int deviceIdB) const {
    QSet<int> visited;
    QList<int> queue;
    queue.push_back(deviceIdA);
    visited.insert(deviceIdA);
    while (!queue.isEmpty()) {
        const int cur = queue.takeFirst();
        for (const auto& link : m_model.links()) {
            int neighbor = -1;
            if (link.leftDeviceId() == cur)  neighbor = link.rightDeviceId();
            else if (link.rightDeviceId() == cur) neighbor = link.leftDeviceId();
            if (neighbor == deviceIdB) return true;
            if (neighbor >= 0 && !visited.contains(neighbor)) {
                visited.insert(neighbor);
                queue.push_back(neighbor);
            }
        }
    }
    return false;
}

int Simulator::connectedInterfaceIndex(int fromDeviceId, int toDeviceId) const {
    for (const auto& link : m_model.links()) {
        if (link.leftDeviceId() == fromDeviceId && link.rightDeviceId() == toDeviceId) {
            return link.rightInterfaceIndex();
        }
        if (link.rightDeviceId() == fromDeviceId && link.leftDeviceId() == toDeviceId) {
            return link.leftInterfaceIndex();
        }
    }
    return -1;
}

bool Simulator::aclAllowsTraffic(const Device& destinationDevice, int inboundInterfaceIndex, const QString& sourceIp, const QString& targetIp) const {
    if (inboundInterfaceIndex < 0) {
        return true;
    }
    const auto* iface = destinationDevice.interfaceAt(inboundInterfaceIndex);
    if (!iface || iface->inboundAcl().isEmpty()) {
        return true;
    }
    for (const auto& acl : destinationDevice.accessLists()) {
        if (acl.name.compare(iface->inboundAcl(), Qt::CaseInsensitive) != 0) {
            continue;
        }
        for (const auto& rule : acl.rules) {
            if (rule.protocol != "ip") {
                continue;
            }
            if (aclAddressMatches(rule.source, sourceIp) && aclAddressMatches(rule.destination, targetIp)) {
                return rule.action == "permit";
            }
        }
        return false;
    }
    return true;
}

bool Simulator::aclAddressMatches(const QString& spec, const QString& ip) {
    const QString normalized = spec.trimmed().toLower();
    if (normalized == "any") {
        return true;
    }
    if (normalized.startsWith("host ")) {
        return normalized.mid(5).trimmed() == ip.toLower();
    }
    const QStringList parts = normalized.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
    if (parts.size() == 2) {
        bool okNet = false;
        bool okWild = false;
        bool okIp = false;
        const quint32 net = ipv4ToInt(parts[0], &okNet);
        const quint32 wildcard = ipv4ToInt(parts[1], &okWild);
        const quint32 target = ipv4ToInt(ip, &okIp);
        if (okNet && okWild && okIp) {
            const quint32 mask = ~wildcard;
            return (target & mask) == (net & mask);
        }
    }
    return normalized == ip.toLower();
}

quint32 Simulator::ipv4ToInt(const QString& value, bool* ok) {
    const QStringList parts = value.split('.');
    if (parts.size() != 4) { if (ok) *ok = false; return 0; }
    quint32 result = 0;
    for (const auto& part : parts) {
        bool localOk = false;
        const int octet = part.toInt(&localOk);
        if (!localOk || octet < 0 || octet > 255) { if (ok) *ok = false; return 0; }
        result = (result << 8U) | static_cast<quint32>(octet);
    }
    if (ok) *ok = true;
    return result;
}

bool Simulator::sameSubnet(const QString& ipA, const QString& ipB, const QString& mask) {
    bool okA, okB, okM;
    const quint32 a  = ipv4ToInt(ipA,  &okA);
    const quint32 b  = ipv4ToInt(ipB,  &okB);
    const quint32 m  = ipv4ToInt(mask, &okM);
    if (!okA || !okB || !okM) return false;
    return (a & m) == (b & m);
}

} // namespace packetlab
