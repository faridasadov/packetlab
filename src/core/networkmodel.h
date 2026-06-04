#pragma once

#include <vector>

#include "device.h"
#include "link.h"

namespace packetlab {

class NetworkModel {
public:
    NetworkModel();

    const std::vector<Device>& devices() const;
    const std::vector<Link>& links() const;
    Device& addDevice(DeviceType type, const QString& baseName);
    Link& addLink(int leftDeviceId, int leftInterfaceIndex, int rightDeviceId, int rightInterfaceIndex);
    Device* findDevice(int id);
    const Device* findDevice(int id) const;
    Link* findLink(int id);
    const Link* findLink(int id) const;
    const Link* findLinkForInterface(int deviceId, int interfaceIndex) const;
    bool removeDevice(int id);
    bool removeLink(int id);
    void clear();
    void seedDemoTopology();

private:
    QPointF nextPosition(DeviceType type) const;

    int m_nextId;
    int m_nextLinkId;
    std::vector<Device> m_devices;
    std::vector<Link> m_links;
};

}  // namespace packetlab
