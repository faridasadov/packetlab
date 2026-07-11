#pragma once
#include <QPointF>
#include <QString>
#include <vector>
#include "networkinterface.h"

namespace packetlab {

enum class DeviceType {
    Pc, Laptop, Server, IpPhone, IpCamera, WirelessAp,
    Switch, SwitchL3, Router, Firewall
};

struct RouteEntry {
    QString destination;
    QString mask;
    QString nextHop;
    int metric = 1;
};

struct VlanEntry {
    int id = 1;
    QString name = "default";
};

struct DhcpPool {
    QString name;
    QString network;
    QString mask;
    QString defaultRouter;
    QString dnsServer;
};

struct AclRule {
    QString action;
    QString protocol;
    QString source;
    QString destination;
};

struct AccessList {
    QString name;
    std::vector<AclRule> rules;
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
    const std::vector<RouteEntry>& routingTable() const;
    std::vector<RouteEntry>& routingTable();
    const std::vector<VlanEntry>& vlanDatabase() const;
    std::vector<VlanEntry>& vlanDatabase();
    const std::vector<DhcpPool>& dhcpPools() const;
    std::vector<DhcpPool>& dhcpPools();
    const std::vector<AccessList>& accessLists() const;
    std::vector<AccessList>& accessLists();
    bool isSwitchLike() const;
    bool isL3Capable() const;
    QString typeLabel() const;
    QString typeId() const;
private:
    int m_id;
    DeviceType m_type;
    QString m_name;
    QPointF m_position;
    QString m_ipAddress;
    QString m_subnetMask;
    QString m_defaultGateway;
    std::vector<NetworkInterface> m_interfaces;
    std::vector<RouteEntry> m_routingTable;
    std::vector<VlanEntry> m_vlanDatabase;
    std::vector<DhcpPool> m_dhcpPools;
    std::vector<AccessList> m_accessLists;
};

} // namespace packetlab
