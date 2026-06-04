#include "networkinterface.h"

#include <utility>

namespace packetlab {

NetworkInterface::NetworkInterface(QString name, QString ipAddress, QString subnetMask)
    : m_name(std::move(name)),
      m_ipAddress(std::move(ipAddress)),
      m_subnetMask(std::move(subnetMask)) {}

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

}  // namespace packetlab
