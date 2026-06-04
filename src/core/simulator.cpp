#include "simulator.h"

#include <QStringList>

namespace packetlab {

Simulator::Simulator(NetworkModel& model) : m_model(model), m_tick(0) {}

QString Simulator::runTick() {
    ++m_tick;
    return QString("Simulation tick %1, active devices: %2")
        .arg(m_tick)
        .arg(m_model.devices().size());
}

QString Simulator::simulatePing(int sourceDeviceId, const QString& targetIpAddress) const {
    const Device* source = m_model.findDevice(sourceDeviceId);
    if (!source) {
        return "Ping failed: source device was not found.";
    }

    if (targetIpAddress.trimmed().isEmpty()) {
        return "Ping failed: target IP is empty.";
    }

    return resolvePingHop(*source, targetIpAddress.trimmed());
}

QString Simulator::resolvePingHop(const Device& source, const QString& targetIpAddress) const {
    for (const auto& target : m_model.devices()) {
        if (target.ipAddress() == targetIpAddress) {
            if (sameSubnet(source.ipAddress(), target.ipAddress(), source.subnetMask())) {
                return QString("Ping success: %1 reached %2 on the same subnet.")
                    .arg(source.name(), target.name());
            }

            if (!source.defaultGateway().isEmpty()) {
                for (const auto& router : m_model.devices()) {
                    if (router.ipAddress() == source.defaultGateway() && target.defaultGateway() == router.ipAddress()) {
                        return QString("Ping success: %1 reached %2 through router %3.")
                            .arg(source.name(), target.name(), router.name());
                    }
                }
            }

            return QString("Ping failed: %1 can see target %2 but no route is configured.")
                .arg(source.name(), target.name());
        }
    }

    return QString("Ping failed: no device owns %1.").arg(targetIpAddress);
}

quint32 Simulator::ipv4ToInt(const QString& value, bool* ok) {
    const QStringList parts = value.split('.');
    if (parts.size() != 4) {
        if (ok) *ok = false;
        return 0;
    }

    quint32 result = 0;
    for (const auto& part : parts) {
        bool localOk = false;
        const int octet = part.toInt(&localOk);
        if (!localOk || octet < 0 || octet > 255) {
            if (ok) *ok = false;
            return 0;
        }
        result = (result << 8U) | static_cast<quint32>(octet);
    }

    if (ok) *ok = true;
    return result;
}

bool Simulator::sameSubnet(const QString& ipA, const QString& ipB, const QString& mask) {
    bool okA = false;
    bool okB = false;
    bool okMask = false;
    const quint32 left = ipv4ToInt(ipA, &okA);
    const quint32 right = ipv4ToInt(ipB, &okB);
    const quint32 subnetMask = ipv4ToInt(mask, &okMask);

    if (!okA || !okB || !okMask) {
        return false;
    }

    return (left & subnetMask) == (right & subnetMask);
}

}  // namespace packetlab
