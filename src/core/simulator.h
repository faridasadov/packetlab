#pragma once
#include <QString>
#include <QStringList>
#include "networkmodel.h"

namespace packetlab {

class Simulator {
public:
    explicit Simulator(NetworkModel& model);
    QString runTick();
    QString simulatePing(int sourceDeviceId, const QString& targetIp) const;
    QString simulateTraceroute(int sourceDeviceId, const QString& targetIp) const;
    QString simulateArpTable(int deviceId) const;
    QString simulateRouteTable(int deviceId) const;
    QString simulateConnectivityMatrix() const;
    QString simulateSubnetAudit() const;

private:
    struct TraceResult {
        bool success = false;
        QStringList hops;
        QString error;
    };
    TraceResult traceRoute(const Device& source, const QString& targetIp) const;
    const Device* resolveNextHop(const Device& current, const QString& targetIp) const;
    bool deviceCanDirectlyReach(const Device& device, const QString& targetIp) const;
    bool areConnected(int deviceIdA, int deviceIdB) const;
    static quint32 ipv4ToInt(const QString& value, bool* ok);
    static bool sameSubnet(const QString& ipA, const QString& ipB, const QString& mask);
    NetworkModel& m_model;
    int m_tick;
};

} // namespace packetlab
