#pragma once

#include <QPointF>
#include <QString>
#include <vector>

#include "networkinterface.h"

namespace packetlab {

enum class DeviceType {
    Pc,
    Switch,
    Router
};

class Device {
public:
    Device(int id, DeviceType type, QString name);

    int id() const;
    DeviceType type() const;
    const QString& name() const;
    void setName(QString value);
    QPointF position() const;
    void setPosition(QPointF value);
    const QString& ipAddress() const;
    void setIpAddress(QString value);
    const QString& subnetMask() const;
    void setSubnetMask(QString value);
    const QString& defaultGateway() const;
    void setDefaultGateway(QString value);
    const std::vector<NetworkInterface>& interfaces() const;
    std::vector<NetworkInterface>& interfaces();
    NetworkInterface* interfaceAt(int index);
    const NetworkInterface* interfaceAt(int index) const;

    QString typeLabel() const;

private:
    int m_id;
    DeviceType m_type;
    QString m_name;
    QPointF m_position;
    QString m_ipAddress;
    QString m_subnetMask;
    QString m_defaultGateway;
    std::vector<NetworkInterface> m_interfaces;
};

}  // namespace packetlab
