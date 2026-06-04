#pragma once

#include <QString>

#include "networkmodel.h"

namespace packetlab {

class Simulator {
public:
    explicit Simulator(NetworkModel& model);

    QString runTick();
    QString simulatePing(int sourceDeviceId, const QString& targetIpAddress) const;

private:
    QString resolvePingHop(const Device& source, const QString& targetIpAddress) const;
    static quint32 ipv4ToInt(const QString& value, bool* ok);
    static bool sameSubnet(const QString& ipA, const QString& ipB, const QString& mask);

    NetworkModel& m_model;
    int m_tick;
};

}  // namespace packetlab
