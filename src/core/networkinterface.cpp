#include "networkinterface.h"

#include <utility>

namespace packetlab {

NetworkInterface::NetworkInterface(QString name, QString ipAddress, QString subnetMask)
    : m_name(std::move(name)),
      m_ipAddress(std::move(ipAddress)),
      m_subnetMask(std::move(subnetMask)),
      m_portMode(PortMode::Routed),
      m_accessVlan(1),
      m_nativeVlan(1),
      m_allowedVlans("1"),
      m_inboundAcl("") {}

const QString& NetworkInterface::name() const {
    return m_name;
}

void NetworkInterface::setName(QString value) {
    m_name = std::move(value);
}

const QString& NetworkInterface::ipAddress() const {
    return m_ipAddress;
}

void NetworkInterface::setIpAddress(QString value) {
    m_ipAddress = std::move(value);
}

const QString& NetworkInterface::subnetMask() const {
    return m_subnetMask;
}

void NetworkInterface::setSubnetMask(QString value) {
    m_subnetMask = std::move(value);
}

PortMode NetworkInterface::portMode() const {
    return m_portMode;
}

void NetworkInterface::setPortMode(PortMode value) {
    m_portMode = value;
}

int NetworkInterface::accessVlan() const {
    return m_accessVlan;
}

void NetworkInterface::setAccessVlan(int value) {
    m_accessVlan = value;
}

int NetworkInterface::nativeVlan() const {
    return m_nativeVlan;
}

void NetworkInterface::setNativeVlan(int value) {
    m_nativeVlan = value;
}

const QString& NetworkInterface::allowedVlans() const {
    return m_allowedVlans;
}

void NetworkInterface::setAllowedVlans(QString value) {
    m_allowedVlans = std::move(value);
}

const QString& NetworkInterface::inboundAcl() const {
    return m_inboundAcl;
}

void NetworkInterface::setInboundAcl(QString value) {
    m_inboundAcl = std::move(value);
}

}  // namespace packetlab
