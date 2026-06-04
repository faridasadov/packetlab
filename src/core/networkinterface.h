#pragma once

#include <QString>

namespace packetlab {

class NetworkInterface {
public:
    NetworkInterface(QString name, QString ipAddress = "", QString subnetMask = "255.255.255.0");

    const QString& name() const;
    void setName(QString value);
    const QString& ipAddress() const;
    void setIpAddress(QString value);
    const QString& subnetMask() const;
    void setSubnetMask(QString value);

private:
    QString m_name;
    QString m_ipAddress;
    QString m_subnetMask;
};

}  // namespace packetlab
