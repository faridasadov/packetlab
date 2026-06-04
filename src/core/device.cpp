#include "device.h"

#include <utility>

namespace packetlab {

Device::Device(int id, DeviceType type, QString name)
    : m_id(id),
      m_type(type),
      m_name(std::move(name)),
      m_position(0.0, 0.0),
      m_ipAddress(""),
      m_subnetMask("255.255.255.0"),
      m_defaultGateway("") {
    switch (m_type) {
    case DeviceType::Pc:
        m_interfaces.emplace_back("eth0");
        break;
    case DeviceType::Switch:
        for (int i = 0; i < 8; ++i) {
            m_interfaces.emplace_back(QString("fa0/%1").arg(i + 1));
        }
        break;
    case DeviceType::Router:
        m_interfaces.emplace_back("g0/0");
        m_interfaces.emplace_back("g0/1");
        break;
    }
}

int Device::id() const {
    return m_id;
}

DeviceType Device::type() const {
    return m_type;
}

const QString& Device::name() const {
    return m_name;
}

void Device::setName(QString value) {
    m_name = std::move(value);
}

QPointF Device::position() const {
    return m_position;
}

void Device::setPosition(QPointF value) {
    m_position = value;
}

const QString& Device::ipAddress() const {
    return m_ipAddress;
}

void Device::setIpAddress(QString value) {
    m_ipAddress = std::move(value);
}

const QString& Device::subnetMask() const {
    return m_subnetMask;
}

void Device::setSubnetMask(QString value) {
    m_subnetMask = std::move(value);
}

const QString& Device::defaultGateway() const {
    return m_defaultGateway;
}

void Device::setDefaultGateway(QString value) {
    m_defaultGateway = std::move(value);
}

const std::vector<NetworkInterface>& Device::interfaces() const {
    return m_interfaces;
}

std::vector<NetworkInterface>& Device::interfaces() {
    return m_interfaces;
}

NetworkInterface* Device::interfaceAt(int index) {
    if (index < 0 || index >= static_cast<int>(m_interfaces.size())) {
        return nullptr;
    }

    return &m_interfaces[static_cast<std::size_t>(index)];
}

const NetworkInterface* Device::interfaceAt(int index) const {
    if (index < 0 || index >= static_cast<int>(m_interfaces.size())) {
        return nullptr;
    }

    return &m_interfaces[static_cast<std::size_t>(index)];
}

QString Device::typeLabel() const {
    switch (m_type) {
    case DeviceType::Pc:
        return "PC";
    case DeviceType::Switch:
        return "Switch";
    case DeviceType::Router:
        return "Router";
    }

    return "Unknown";
}

}  // namespace packetlab
