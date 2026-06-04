#include "topologyscene.h"

#include <algorithm>
#include <functional>
#include <vector>

#include <QBrush>
#include <QFont>
#include <QHash>
#include <QGraphicsDropShadowEffect>
#include <QGraphicsItemGroup>
#include <QGraphicsLineItem>
#include <QGraphicsSceneContextMenuEvent>
#include <QGraphicsSceneHoverEvent>
#include <QGraphicsSceneMouseEvent>
#include <QPainterPath>
#include <QPainter>
#include <QPen>
#include <QStyleOptionGraphicsItem>

namespace packetlab {

namespace {

class DeviceNodeItem final : public QGraphicsObject {
public:
    enum { Type = UserType + 1 };

    DeviceNodeItem(
        const Device& device,
        bool selected,
        std::function<void(int)> onSelected,
        std::function<void(int, QPointF)> onMoved,
        std::function<void(int, QPointF)> onReleased)
        : m_device(device),
          m_selected(selected),
          m_onSelected(std::move(onSelected)),
          m_onMoved(std::move(onMoved)),
          m_onReleased(std::move(onReleased)),
          m_hoveredInterfaceIndex(-1) {
        setFlags(ItemIsMovable | ItemIsSelectable | ItemSendsGeometryChanges);
        setCacheMode(DeviceCoordinateCache);
        setAcceptHoverEvents(true);
        setPos(device.position());
        auto* shadow = new QGraphicsDropShadowEffect();
        shadow->setBlurRadius(20.0);
        shadow->setOffset(0.0, 8.0);
        shadow->setColor(QColor(17, 24, 39, 35));
        setGraphicsEffect(shadow);
    }

    QRectF boundingRect() const override {
        return QRectF(-76.0, -44.0, 152.0, 88.0);
    }

    int type() const override {
        return Type;
    }

    int deviceId() const {
        return m_device.id();
    }

    QPointF interfaceAnchorScenePoint(int index) const {
        const auto anchors = interfaceAnchorLocalPoints();
        if (index < 0 || index >= static_cast<int>(anchors.size())) {
            return scenePos();
        }
        return mapToScene(anchors[static_cast<std::size_t>(index)]);
    }

    int interfaceAtScenePoint(const QPointF& scenePoint) const {
        const QPointF localPoint = mapFromScene(scenePoint);
        const auto anchors = interfaceAnchorLocalPoints();
        for (int i = 0; i < static_cast<int>(anchors.size()); ++i) {
            if (QLineF(localPoint, anchors[static_cast<std::size_t>(i)]).length() <= 8.0) {
                return i;
            }
        }
        return -1;
    }

    void setHighlighted(bool highlighted) {
        if (m_selected == highlighted) {
            return;
        }
        m_selected = highlighted;
        update();
    }

    void addEdgeRefresh(std::function<void()> refresh) {
        m_edgeRefreshers.push_back(std::move(refresh));
    }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override {
        QColor fill("#ffffff");
        QColor accent("#3c78d8");
        if (m_device.type() == DeviceType::Switch) {
            fill = QColor("#e8f3ff");
            accent = QColor("#1570ef");
        } else if (m_device.type() == DeviceType::Router) {
            fill = QColor("#fff1de");
            accent = QColor("#f08c2e");
        } else {
            fill = QColor("#eaf7ee");
            accent = QColor("#23915b");
        }

        painter->setRenderHint(QPainter::Antialiasing, true);
        painter->setPen(QPen(m_selected ? QColor("#111827") : QColor("#c8d5e3"), m_selected ? 2.8 : 1.5));
        painter->setBrush(fill);
        painter->drawRoundedRect(boundingRect(), 16.0, 16.0);

        painter->setPen(Qt::NoPen);
        painter->setBrush(accent);
        painter->drawRoundedRect(QRectF(-76.0, -44.0, 152.0, 11.0), 16.0, 16.0);
        painter->drawRect(QRectF(-76.0, -38.0, 152.0, 6.0));

        painter->setPen(accent.darker(130));
        QFont typeFont("Sans Serif", 8, QFont::Bold);
        painter->setFont(typeFont);
        painter->drawText(QRectF(-62.0, -26.0, 100.0, 16.0), m_device.typeLabel().toUpper());

        painter->setPen(QColor("#142033"));
        QFont titleFont("Sans Serif", 11, QFont::Bold);
        painter->setFont(titleFont);
        painter->drawText(QRectF(-62.0, -8.0, 110.0, 20.0), m_device.name());

        painter->setPen(QColor("#516174"));
        QFont bodyFont("Sans Serif", 8);
        painter->setFont(bodyFont);
        const QString ipText = m_device.ipAddress().isEmpty() ? "No IP assigned" : m_device.ipAddress();
        painter->drawText(QRectF(-62.0, 16.0, 120.0, 16.0), ipText);

        painter->setPen(QColor("#6a7b8d"));
        painter->drawText(QRectF(10.0, -26.0, 54.0, 16.0), Qt::AlignRight, QString("%1 ports").arg(m_device.interfaces().size()));

        const auto anchors = interfaceAnchorLocalPoints();
        for (int i = 0; i < static_cast<int>(anchors.size()); ++i) {
            const bool active = i < 2 || m_device.type() == DeviceType::Switch;
            const bool hovered = i == m_hoveredInterfaceIndex;
            painter->setPen(QPen(hovered ? QColor("#111827") : (active ? accent.darker(110) : QColor("#9fb2c7")), hovered ? 1.8 : 1.0));
            painter->setBrush(hovered ? QColor("#dbeafe") : (active ? QColor("#ffffff") : QColor("#eef3f8")));
            painter->drawEllipse(anchors[static_cast<std::size_t>(i)], hovered ? 6.0 : 5.0, hovered ? 6.0 : 5.0);
        }
    }

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override {
        if (change == ItemPositionHasChanged && m_onMoved) {
            m_onMoved(m_device.id(), value.toPointF());
        }
        if (change == ItemPositionHasChanged) {
            for (const auto& refresh : m_edgeRefreshers) {
                refresh();
            }
        }
        return QGraphicsObject::itemChange(change, value);
    }

    void mousePressEvent(QGraphicsSceneMouseEvent* event) override {
        if (m_onSelected) {
            m_onSelected(m_device.id());
        }
        QGraphicsObject::mousePressEvent(event);
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override {
        if (m_onReleased) {
            m_onReleased(m_device.id(), pos());
        }
        QGraphicsObject::mouseReleaseEvent(event);
    }

    void hoverMoveEvent(QGraphicsSceneHoverEvent* event) override {
        const int hovered = interfaceAtScenePoint(event->scenePos());
        if (hovered != m_hoveredInterfaceIndex) {
            m_hoveredInterfaceIndex = hovered;
            update();
        }
        QGraphicsObject::hoverMoveEvent(event);
    }

    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override {
        if (m_hoveredInterfaceIndex != -1) {
            m_hoveredInterfaceIndex = -1;
            update();
        }
        QGraphicsObject::hoverLeaveEvent(event);
    }

private:
    std::vector<QPointF> interfaceAnchorLocalPoints() const {
        std::vector<QPointF> anchors;
        const int count = static_cast<int>(m_device.interfaces().size());
        if (count <= 0) {
            return anchors;
        }

        const QRectF rect = boundingRect().adjusted(18.0, 18.0, -18.0, -12.0);
        if (count == 1) {
            anchors.emplace_back(0.0, rect.bottom());
            return anchors;
        }

        if (count <= 4) {
            for (int i = 0; i < count; ++i) {
                const qreal t = count == 1 ? 0.5 : static_cast<qreal>(i) / static_cast<qreal>(count - 1);
                anchors.emplace_back(rect.left() + t * rect.width(), rect.bottom());
            }
            return anchors;
        }

        const int topRow = (count + 1) / 2;
        const int bottomRow = count - topRow;
        for (int i = 0; i < topRow; ++i) {
            const qreal t = topRow == 1 ? 0.5 : static_cast<qreal>(i) / static_cast<qreal>(topRow - 1);
            anchors.emplace_back(rect.left() + t * rect.width(), rect.bottom() - 8.0);
        }
        for (int i = 0; i < bottomRow; ++i) {
            const qreal t = bottomRow == 1 ? 0.5 : static_cast<qreal>(i) / static_cast<qreal>(bottomRow - 1);
            anchors.emplace_back(rect.left() + t * rect.width(), rect.bottom() + 8.0);
        }
        return anchors;
    }

    Device m_device;
    bool m_selected;
    std::function<void(int)> m_onSelected;
    std::function<void(int, QPointF)> m_onMoved;
    std::function<void(int, QPointF)> m_onReleased;
    std::vector<std::function<void()>> m_edgeRefreshers;
    int m_hoveredInterfaceIndex;
};

class LinkEdgeItem final : public QGraphicsItem {
public:
    LinkEdgeItem(const Link& link, DeviceNodeItem* left, DeviceNodeItem* right)
        : m_link(link), m_left(left), m_right(right), m_selected(false), m_hovered(false) {
        setZValue(1.0);
        setAcceptHoverEvents(true);
    }

    int linkId() const {
        return m_link.id();
    }

    void setSelectedState(bool selected) {
        if (m_selected == selected) {
            return;
        }
        m_selected = selected;
        update();
    }

    void refreshGeometry() {
        prepareGeometryChange();
        update();
    }

    QRectF boundingRect() const override {
        if (!m_left || !m_right) {
            return {};
        }
        const QPointF leftPoint = m_left->interfaceAnchorScenePoint(m_link.leftInterfaceIndex());
        const QPointF rightPoint = m_right->interfaceAnchorScenePoint(m_link.rightInterfaceIndex());
        return QRectF(leftPoint, rightPoint).normalized().adjusted(-48.0, -32.0, 48.0, 32.0);
    }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override {
        if (!m_left || !m_right) {
            return;
        }

        const QPointF leftPoint = mapFromScene(m_left->interfaceAnchorScenePoint(m_link.leftInterfaceIndex()));
        const QPointF rightPoint = mapFromScene(m_right->interfaceAnchorScenePoint(m_link.rightInterfaceIndex()));
        const QPointF mid = (leftPoint + rightPoint) / 2.0;

        painter->setRenderHint(QPainter::Antialiasing, true);
        const QColor lineColor = m_selected ? QColor("#2563eb") : (m_hovered ? QColor("#4f7fb6") : QColor("#7890aa"));
        painter->setPen(QPen(lineColor, m_selected ? 4.0 : 3.0, Qt::SolidLine, Qt::RoundCap));
        painter->drawLine(leftPoint, rightPoint);

        painter->setPen(Qt::NoPen);
        painter->setBrush(lineColor);
        painter->drawEllipse(mid, 3.5, 3.5);

        painter->setPen(m_selected ? QColor("#1d4ed8") : QColor("#60758c"));
        painter->setFont(QFont("Sans Serif", 7, QFont::Medium));
        const QString labelText = QString("p%1 <-> p%2")
            .arg(m_link.leftInterfaceIndex() + 1)
            .arg(m_link.rightInterfaceIndex() + 1);
        painter->drawText(QRectF(mid.x() - 42.0, mid.y() - 22.0, 84.0, 14.0), Qt::AlignCenter, labelText);
    }

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override {
        QGraphicsItem::mousePressEvent(event);
        event->accept();
    }

    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override {
        m_hovered = true;
        update();
        QGraphicsItem::hoverEnterEvent(event);
    }

    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override {
        m_hovered = false;
        update();
        QGraphicsItem::hoverLeaveEvent(event);
    }

private:
    Link m_link;
    DeviceNodeItem* m_left;
    DeviceNodeItem* m_right;
    bool m_selected;
    bool m_hovered;
};

}  // namespace

TopologyScene::TopologyScene(QObject* parent)
    : QGraphicsScene(parent),
      m_selectedDeviceId(-1),
      m_selectedLinkId(-1),
      m_temporaryCable(nullptr),
      m_dragDeviceId(-1),
      m_dragInterfaceIndex(-1) {
    setSceneRect(0.0, 0.0, 1400.0, 900.0);
}

void TopologyScene::rebuild(const NetworkModel& model) {
    clear();
    m_temporaryCable = nullptr;
    m_dragDeviceId = -1;
    m_dragInterfaceIndex = -1;

    QHash<int, DeviceNodeItem*> deviceItems;
    for (const auto& device : model.devices()) {
        auto* item = new DeviceNodeItem(
            device,
            device.id() == m_selectedDeviceId,
            [this](int deviceId) { emit deviceSelected(deviceId); },
            [this](int deviceId, const QPointF& position) { emit deviceMoved(deviceId, position); },
            [this](int deviceId, const QPointF& position) { emit deviceMoveCommitted(deviceId, position); });
        addItem(item);
        item->setZValue(2.0);
        deviceItems.insert(device.id(), item);
    }

    for (const auto& link : model.links()) {
        auto* left = deviceItems.value(link.leftDeviceId(), nullptr);
        auto* right = deviceItems.value(link.rightDeviceId(), nullptr);
        if (!left || !right) {
            continue;
        }
        auto* edge = new LinkEdgeItem(link, left, right);
        left->addEdgeRefresh([edge]() { edge->refreshGeometry(); });
        right->addEdgeRefresh([edge]() { edge->refreshGeometry(); });
        addItem(edge);
    }
}

void TopologyScene::setSelectedDeviceId(int deviceId) {
    if (m_selectedDeviceId == deviceId) {
        return;
    }

    m_selectedDeviceId = deviceId;
    m_selectedLinkId = -1;

    const auto sceneItems = items();
    for (QGraphicsItem* item : sceneItems) {
        if (auto* node = dynamic_cast<DeviceNodeItem*>(item)) {
            node->setHighlighted(node->deviceId() == m_selectedDeviceId);
        } else if (auto* edge = dynamic_cast<LinkEdgeItem*>(item)) {
            edge->setSelectedState(false);
        }
    }
}

void TopologyScene::drawBackground(QPainter* painter, const QRectF& rect) {
    painter->fillRect(rect, QColor("#f3f7fb"));

    const qreal grid = 32.0;
    QPen minorPen(QColor("#e5edf5"));
    minorPen.setWidthF(1.0);
    painter->setPen(minorPen);

    const qreal left = std::floor(rect.left() / grid) * grid;
    const qreal top = std::floor(rect.top() / grid) * grid;

    for (qreal x = left; x < rect.right(); x += grid) {
        painter->drawLine(QLineF(x, rect.top(), x, rect.bottom()));
    }
    for (qreal y = top; y < rect.bottom(); y += grid) {
        painter->drawLine(QLineF(rect.left(), y, rect.right(), y));
    }
}

void TopologyScene::contextMenuEvent(QGraphicsSceneContextMenuEvent* event) {
    const auto sceneItems = items(event->scenePos());
    for (QGraphicsItem* item : sceneItems) {
        if (auto* edge = dynamic_cast<LinkEdgeItem*>(item)) {
            emit linkContextMenuRequested(edge->linkId(), event->screenPos());
            event->accept();
            return;
        }
        if (auto* node = dynamic_cast<DeviceNodeItem*>(item)) {
            emit deviceContextMenuRequested(node->deviceId(), event->screenPos());
            event->accept();
            return;
        }
    }

    emit canvasContextMenuRequested(event->screenPos());
    event->accept();
}

void TopologyScene::mousePressEvent(QGraphicsSceneMouseEvent* event) {
    const auto sceneItems = items(event->scenePos());
    for (QGraphicsItem* item : sceneItems) {
        if (auto* edge = dynamic_cast<LinkEdgeItem*>(item)) {
            m_selectedLinkId = edge->linkId();
            m_selectedDeviceId = -1;
            for (QGraphicsItem* other : items()) {
                if (auto* node = dynamic_cast<DeviceNodeItem*>(other)) {
                    node->setHighlighted(false);
                } else if (auto* edgeItem = dynamic_cast<LinkEdgeItem*>(other)) {
                    edgeItem->setSelectedState(edgeItem->linkId() == m_selectedLinkId);
                }
            }
            emit linkSelected(m_selectedLinkId);
            event->accept();
            return;
        }

        auto* node = dynamic_cast<DeviceNodeItem*>(item);
        if (!node) {
            continue;
        }

        const int interfaceIndex = node->interfaceAtScenePoint(event->scenePos());
        if (interfaceIndex >= 0) {
            m_dragDeviceId = node->deviceId();
            m_dragInterfaceIndex = interfaceIndex;
            const QPointF start = node->interfaceAnchorScenePoint(interfaceIndex);
            m_temporaryCable = addLine(
                QLineF(start, event->scenePos()),
                QPen(QColor("#3b82f6"), 2.0, Qt::DashLine, Qt::RoundCap));
            m_temporaryCable->setZValue(4.0);
            event->accept();
            return;
        }
    }

    QGraphicsScene::mousePressEvent(event);
}

void TopologyScene::mouseMoveEvent(QGraphicsSceneMouseEvent* event) {
    if (m_temporaryCable) {
        QLineF line = m_temporaryCable->line();
        line.setP2(event->scenePos());
        m_temporaryCable->setLine(line);
        event->accept();
        return;
    }

    QGraphicsScene::mouseMoveEvent(event);
}

void TopologyScene::mouseReleaseEvent(QGraphicsSceneMouseEvent* event) {
    if (m_temporaryCable) {
        const auto sceneItems = items(event->scenePos());
        for (QGraphicsItem* item : sceneItems) {
            auto* node = dynamic_cast<DeviceNodeItem*>(item);
            if (!node) {
                continue;
            }

            const int targetInterfaceIndex = node->interfaceAtScenePoint(event->scenePos());
            if (targetInterfaceIndex >= 0 &&
                !(node->deviceId() == m_dragDeviceId && targetInterfaceIndex == m_dragInterfaceIndex)) {
                emit cableRequested(m_dragDeviceId, m_dragInterfaceIndex, node->deviceId(), targetInterfaceIndex);
                break;
            }
        }

        clearInteractionArtifacts();
        event->accept();
        return;
    }

    QGraphicsScene::mouseReleaseEvent(event);
}

void TopologyScene::addDeviceItem(const Device& device) {
    Q_UNUSED(device);
}

void TopologyScene::addLinkItem(const NetworkModel& model, const Link& link) {
    Q_UNUSED(model);
    Q_UNUSED(link);
}

void TopologyScene::clearInteractionArtifacts() {
    if (m_temporaryCable) {
        removeItem(m_temporaryCable);
        delete m_temporaryCable;
        m_temporaryCable = nullptr;
    }

    m_dragDeviceId = -1;
    m_dragInterfaceIndex = -1;
}

}  // namespace packetlab
