#pragma once

#include <QString>

namespace packetlab {

enum class PortMode {
    Routed,
    Access,
    Trunk
};

class NetworkInterface {
public:
    NetworkInterface(QString name, QString ipAddress = "", QString subnetMask = "255.255.255.0");

    const QString& name() const;
    void setName(QString value);
    const QString& ipAddress() const;
    void setIpAddress(QString value);
    const QString& subnetMask() const;
    void setSubnetMask(QString value);
    PortMode portMode() const;
    void setPortMode(PortMode value);
    int accessVlan() const;
    void setAccessVlan(int value);
    int nativeVlan() const;
    void setNativeVlan(int value);
    const QString& allowedVlans() const;
    void setAllowedVlans(QString value);
    const QString& inboundAcl() const;
    void setInboundAcl(QString value);

private:
    QString m_name;
    QString m_ipAddress;
    QString m_subnetMask;
    PortMode m_portMode;
    int m_accessVlan;
    int m_nativeVlan;
    QString m_allowedVlans;
    QString m_inboundAcl;
};

}  // namespace packetlab
