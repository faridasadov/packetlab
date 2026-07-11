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
    case DeviceType::Laptop:
        m_interfaces.emplace_back("eth0");
        m_interfaces.emplace_back("wlan0");
        break;
    case DeviceType::Server:
        m_interfaces.emplace_back("eth0");
        m_interfaces.emplace_back("eth1");
        break;
    case DeviceType::IpPhone:
        m_interfaces.emplace_back("eth0");
        break;
    case DeviceType::IpCamera:
        m_interfaces.emplace_back("eth0");
        break;
    case DeviceType::WirelessAp:
        m_interfaces.emplace_back("eth0");
        m_interfaces.emplace_back("wlan0");
        break;
    case DeviceType::Switch:
    case DeviceType::SwitchL3:
        m_vlanDatabase.push_back({1, "default"});
        for (int i = 0; i < 8; ++i) {
            m_interfaces.emplace_back(QString("fa0/%1").arg(i + 1));
            m_interfaces.back().setPortMode(PortMode::Access);
            m_interfaces.back().setAccessVlan(1);
            m_interfaces.back().setNativeVlan(1);
            m_interfaces.back().setAllowedVlans("1");
        }
        break;
    case DeviceType::Router:
        m_interfaces.emplace_back("g0/0");
        m_interfaces.emplace_back("g0/1");
        m_interfaces.emplace_back("g0/2");
        m_interfaces.emplace_back("g0/3");
        break;
    case DeviceType::Firewall:
        m_interfaces.emplace_back("eth0");
        m_interfaces.emplace_back("eth1");
        m_interfaces.emplace_back("eth2");
        break;
    }
}

int Device::id() const { return m_id; }
DeviceType Device::type() const { return m_type; }
const QString& Device::name() const { return m_name; }
void Device::setName(QString value) { m_name = std::move(value); }
QPointF Device::position() const { return m_position; }
void Device::setPosition(QPointF value) { m_position = value; }
const QString& Device::ipAddress() const { return m_ipAddress; }
void Device::setIpAddress(QString value) { m_ipAddress = std::move(value); }
const QString& Device::subnetMask() const { return m_subnetMask; }
void Device::setSubnetMask(QString value) { m_subnetMask = std::move(value); }
const QString& Device::defaultGateway() const { return m_defaultGateway; }
void Device::setDefaultGateway(QString value) { m_defaultGateway = std::move(value); }

const std::vector<NetworkInterface>& Device::interfaces() const { return m_interfaces; }
std::vector<NetworkInterface>& Device::interfaces() { return m_interfaces; }

NetworkInterface* Device::interfaceAt(int index) {
    if (index < 0 || index >= static_cast<int>(m_interfaces.size())) return nullptr;
    return &m_interfaces[static_cast<std::size_t>(index)];
}

const NetworkInterface* Device::interfaceAt(int index) const {
    if (index < 0 || index >= static_cast<int>(m_interfaces.size())) return nullptr;
    return &m_interfaces[static_cast<std::size_t>(index)];
}

const std::vector<RouteEntry>& Device::routingTable() const { return m_routingTable; }
std::vector<RouteEntry>& Device::routingTable() { return m_routingTable; }
const std::vector<VlanEntry>& Device::vlanDatabase() const { return m_vlanDatabase; }
std::vector<VlanEntry>& Device::vlanDatabase() { return m_vlanDatabase; }
const std::vector<DhcpPool>& Device::dhcpPools() const { return m_dhcpPools; }
std::vector<DhcpPool>& Device::dhcpPools() { return m_dhcpPools; }
const std::vector<AccessList>& Device::accessLists() const { return m_accessLists; }
std::vector<AccessList>& Device::accessLists() { return m_accessLists; }

bool Device::isSwitchLike() const {
    return m_type == DeviceType::Switch || m_type == DeviceType::SwitchL3;
}

bool Device::isL3Capable() const {
    return m_type == DeviceType::Router
        || m_type == DeviceType::SwitchL3
        || m_type == DeviceType::Firewall;
}

QString Device::typeLabel() const {
    switch (m_type) {
    case DeviceType::Pc:        return "PC";
    case DeviceType::Laptop:    return "Laptop";
    case DeviceType::Server:    return "Server";
    case DeviceType::IpPhone:   return "IP Phone";
    case DeviceType::IpCamera:  return "IP Camera";
    case DeviceType::WirelessAp:return "Wireless AP";
    case DeviceType::Switch:    return "Switch";
    case DeviceType::SwitchL3:  return "L3 Switch";
    case DeviceType::Router:    return "Router";
    case DeviceType::Firewall:  return "Firewall";
    }
    return "Unknown";
}

QString Device::typeId() const {
    switch (m_type) {
    case DeviceType::Pc:        return "pc";
    case DeviceType::Laptop:    return "laptop";
    case DeviceType::Server:    return "server";
    case DeviceType::IpPhone:   return "ipphone";
    case DeviceType::IpCamera:  return "ipcamera";
    case DeviceType::WirelessAp:return "wirelessap";
    case DeviceType::Switch:    return "switch";
    case DeviceType::SwitchL3:  return "switchl3";
    case DeviceType::Router:    return "router";
    case DeviceType::Firewall:  return "firewall";
    }
    return "pc";
}

} // namespace packetlab
