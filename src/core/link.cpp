#include "link.h"

namespace packetlab {

Link::Link(int id, int leftDeviceId, int leftInterfaceIndex, int rightDeviceId, int rightInterfaceIndex)
    : m_id(id),
      m_leftDeviceId(leftDeviceId),
      m_leftInterfaceIndex(leftInterfaceIndex),
      m_rightDeviceId(rightDeviceId),
      m_rightInterfaceIndex(rightInterfaceIndex) {}

int Link::id() const {
    return m_id;
}

int Link::leftDeviceId() const {
    return m_leftDeviceId;
}

int Link::leftInterfaceIndex() const {
    return m_leftInterfaceIndex;
}

int Link::rightDeviceId() const {
    return m_rightDeviceId;
}

int Link::rightInterfaceIndex() const {
    return m_rightInterfaceIndex;
}

}  // namespace packetlab
