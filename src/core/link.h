#pragma once

namespace packetlab {

class Link {
public:
    Link(int id, int leftDeviceId, int leftInterfaceIndex, int rightDeviceId, int rightInterfaceIndex);

    int id() const;
    int leftDeviceId() const;
    int leftInterfaceIndex() const;
    int rightDeviceId() const;
    int rightInterfaceIndex() const;

private:
    int m_id;
    int m_leftDeviceId;
    int m_leftInterfaceIndex;
    int m_rightDeviceId;
    int m_rightInterfaceIndex;
};

}  // namespace packetlab
