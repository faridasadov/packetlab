#include "networkmodel.h"

#include <algorithm>

namespace packetlab {

NetworkModel::NetworkModel() : m_nextId(1), m_nextLinkId(1) {}

const std::vector<Device>& NetworkModel::devices() const {
    return m_devices;
}

const std::vector<Link>& NetworkModel::links() const {
    return m_links;
}

Device& NetworkModel::addDevice(DeviceType type, const QString& baseName) {
    const QString name = QString("%1-%2").arg(baseName).arg(m_nextId);
    m_devices.emplace_back(m_nextId++, type, name);
    m_devices.back().setPosition(nextPosition(type));
    return m_devices.back();
}

Link& NetworkModel::addLink(int leftDeviceId, int leftInterfaceIndex, int rightDeviceId, int rightInterfaceIndex) {
    m_links.emplace_back(m_nextLinkId++, leftDeviceId, leftInterfaceIndex, rightDeviceId, rightInterfaceIndex);
    return m_links.back();
}

Device* NetworkModel::findDevice(int id) {
    for (auto& device : m_devices) {
        if (device.id() == id) {
            return &device;
        }
    }

    return nullptr;
}

const Device* NetworkModel::findDevice(int id) const {
    for (const auto& device : m_devices) {
        if (device.id() == id) {
            return &device;
        }
    }

    return nullptr;
}

Link* NetworkModel::findLink(int id) {
    for (auto& link : m_links) {
        if (link.id() == id) {
            return &link;
        }
    }
    return nullptr;
}

const Link* NetworkModel::findLink(int id) const {
    for (const auto& link : m_links) {
        if (link.id() == id) {
            return &link;
        }
    }
    return nullptr;
}

const Link* NetworkModel::findLinkForInterface(int deviceId, int interfaceIndex) const {
    for (const auto& link : m_links) {
        if (link.leftDeviceId() == deviceId && link.leftInterfaceIndex() == interfaceIndex) {
            return &link;
        }
        if (link.rightDeviceId() == deviceId && link.rightInterfaceIndex() == interfaceIndex) {
            return &link;
        }
    }

    return nullptr;
}

bool NetworkModel::removeDevice(int id) {
    const auto beforeDevices = m_devices.size();
    m_devices.erase(
        std::remove_if(m_devices.begin(), m_devices.end(), [id](const Device& device) { return device.id() == id; }),
        m_devices.end());
    if (m_devices.size() == beforeDevices) {
        return false;
    }

    m_links.erase(
        std::remove_if(m_links.begin(), m_links.end(), [id](const Link& link) {
            return link.leftDeviceId() == id || link.rightDeviceId() == id;
        }),
        m_links.end());
    return true;
}

bool NetworkModel::removeLink(int id) {
    const auto before = m_links.size();
    m_links.erase(
        std::remove_if(m_links.begin(), m_links.end(), [id](const Link& link) { return link.id() == id; }),
        m_links.end());
    return m_links.size() != before;
}

void NetworkModel::clear() {
    m_nextId = 1;
    m_nextLinkId = 1;
    m_devices.clear();
    m_links.clear();
}

void NetworkModel::seedDemoTopology() {
    if (!m_devices.empty()) {
        return;
    }

    auto& pc = addDevice(DeviceType::Pc, "PC");
    auto& sw = addDevice(DeviceType::Switch, "SW");
    auto& router = addDevice(DeviceType::Router, "R");

    addLink(pc.id(), 0, sw.id(), 0);
    addLink(sw.id(), 1, router.id(), 0);
}

QPointF NetworkModel::nextPosition(DeviceType type) const {
    const qreal x = 120.0 + (m_devices.size() % 4) * 180.0;
    const qreal y = 100.0 + (m_devices.size() / 4) * 160.0;

    switch (type) {
    case DeviceType::Pc:
        return QPointF(x, y);
    case DeviceType::Laptop:
        return QPointF(x, y);
    case DeviceType::Server:
        return QPointF(x, y - 20.0);
    case DeviceType::IpPhone:
        return QPointF(x, y);
    case DeviceType::IpCamera:
        return QPointF(x, y);
    case DeviceType::WirelessAp:
        return QPointF(x, y - 10.0);
    case DeviceType::Switch:
        return QPointF(x, y + 20.0);
    case DeviceType::SwitchL3:
        return QPointF(x, y + 20.0);
    case DeviceType::Router:
        return QPointF(x, y - 20.0);
    case DeviceType::Firewall:
        return QPointF(x, y - 20.0);
    default:
        return QPointF(x, y);
    }
}

}  // namespace packetlab
