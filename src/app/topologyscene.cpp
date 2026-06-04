#include "topologyscene.h"

#include <algorithm>
#include <cmath>
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

// ─── Device node ────────────────────────────────────────────────────────────

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
        shadow->setBlurRadius(24.0);
        shadow->setOffset(0.0, 6.0);
        shadow->setColor(QColor(15, 23, 42, 45));
        setGraphicsEffect(shadow);
    }

    QRectF boundingRect() const override {
        return QRectF(-86.0, -52.0, 172.0, 104.0);
    }

    int type() const override { return Type; }
    int deviceId() const { return m_device.id(); }

    QPointF interfaceAnchorScenePoint(int index) const {
        const auto anchors = interfaceAnchorLocalPoints();
        if (index < 0 || index >= static_cast<int>(anchors.size()))
            return scenePos();
        return mapToScene(anchors[static_cast<std::size_t>(index)]);
    }

    int interfaceAtScenePoint(const QPointF& scenePoint) const {
        const QPointF localPoint = mapFromScene(scenePoint);
        const auto anchors = interfaceAnchorLocalPoints();
        for (int i = 0; i < static_cast<int>(anchors.size()); ++i) {
            if (QLineF(localPoint, anchors[static_cast<std::size_t>(i)]).length() <= 9.0)
                return i;
        }
        return -1;
    }

    void setHighlighted(bool highlighted) {
        if (m_selected == highlighted) return;
        m_selected = highlighted;
        update();
    }

    void addEdgeRefresh(std::function<void()> refresh) {
        m_edgeRefreshers.push_back(std::move(refresh));
    }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override {
        painter->setRenderHint(QPainter::Antialiasing, true);

        QColor fill, header, accent;
        switch (m_device.type()) {
        case DeviceType::Pc:
            fill = QColor("#f0fdf4"); header = QColor("#bbf7d0"); accent = QColor("#16a34a");
            break;
        case DeviceType::Laptop:
            fill = QColor("#f0fdf4"); header = QColor("#bbf7d0"); accent = QColor("#16a34a");
            break;
        case DeviceType::Server:
            fill = QColor("#f5f3ff"); header = QColor("#ddd6fe"); accent = QColor("#7c3aed");
            break;
        case DeviceType::IpPhone:
            fill = QColor("#ecfeff"); header = QColor("#a5f3fc"); accent = QColor("#0891b2");
            break;
        case DeviceType::IpCamera:
            fill = QColor("#fff1f2"); header = QColor("#fecdd3"); accent = QColor("#e11d48");
            break;
        case DeviceType::WirelessAp:
            fill = QColor("#f0f9ff"); header = QColor("#bae6fd"); accent = QColor("#0284c7");
            break;
        case DeviceType::Switch:
            fill = QColor("#eff6ff"); header = QColor("#bfdbfe"); accent = QColor("#2563eb");
            break;
        case DeviceType::SwitchL3:
            fill = QColor("#eef2ff"); header = QColor("#c7d2fe"); accent = QColor("#4338ca");
            break;
        case DeviceType::Router:
            fill = QColor("#fffbeb"); header = QColor("#fde68a"); accent = QColor("#d97706");
            break;
        case DeviceType::Firewall:
            fill = QColor("#fff7ed"); header = QColor("#fed7aa"); accent = QColor("#ea580c");
            break;
        }

        const QRectF r = boundingRect();
        constexpr qreal radius = 16.0;
        constexpr qreal headerH = 34.0;

        // Selection glow
        if (m_selected) {
            QColor glow(accent); glow.setAlpha(45);
            painter->setPen(Qt::NoPen);
            painter->setBrush(glow);
            painter->drawRoundedRect(r.adjusted(-6, -6, 6, 6), radius + 6, radius + 6);
        }

        // Card body
        painter->setPen(m_selected ? QPen(accent, 2.2) : QPen(QColor("#cbd5e1"), 1.5));
        painter->setBrush(fill);
        painter->drawRoundedRect(r, radius, radius);

        // Header band
        QPainterPath hp;
        hp.moveTo(r.left() + radius, r.top());
        hp.lineTo(r.right() - radius, r.top());
        hp.arcTo(QRectF(r.right() - 2*radius, r.top(), 2*radius, 2*radius), 90, -90);
        hp.lineTo(r.right(), r.top() + headerH);
        hp.lineTo(r.left(), r.top() + headerH);
        hp.arcTo(QRectF(r.left(), r.top(), 2*radius, 2*radius), 180, -90);
        hp.closeSubpath();
        painter->setPen(Qt::NoPen);
        painter->setBrush(header);
        painter->drawPath(hp);

        // Separator line
        painter->setPen(QPen(accent.lighter(165), 0.8));
        painter->drawLine(QLineF(r.left()+14, r.top()+headerH, r.right()-14, r.top()+headerH));

        // Device icon
        drawDeviceIcon(painter, QRectF(r.left()+10, r.top()+7, 20, 20), accent.darker(115));

        // Type label
        painter->setPen(accent.darker(160));
        QFont tf; tf.setFamily("Sans Serif"); tf.setPixelSize(9); tf.setBold(true);
        painter->setFont(tf);
        painter->drawText(QRectF(r.left()+36, r.top()+8, 88, 18),
                          Qt::AlignVCenter, m_device.typeLabel().toUpper());

        // Port count
        painter->setPen(QColor("#64748b"));
        QFont bf; bf.setFamily("Sans Serif"); bf.setPixelSize(8);
        painter->setFont(bf);
        painter->drawText(QRectF(r.right()-46, r.top()+8, 34, 18),
                          Qt::AlignRight|Qt::AlignVCenter,
                          QString("%1 pts").arg(m_device.interfaces().size()));

        // Device name
        painter->setPen(QColor("#0f172a"));
        QFont nf; nf.setFamily("Sans Serif"); nf.setPixelSize(13); nf.setBold(true);
        painter->setFont(nf);
        painter->drawText(QRectF(r.left()+12, r.top()+headerH+6, r.width()-24, 20),
                          Qt::AlignVCenter, m_device.name());

        // IP address
        const bool hasIp = !m_device.ipAddress().isEmpty();
        painter->setPen(hasIp ? QColor("#475569") : QColor("#94a3b8"));
        QFont ipf; ipf.setFamily("monospace"); ipf.setPixelSize(9);
        painter->setFont(ipf);
        painter->drawText(QRectF(r.left()+12, r.top()+headerH+28, r.width()-24, 16),
                          Qt::AlignVCenter,
                          hasIp ? m_device.ipAddress() : "– no IP –");

        // Interface port dots
        const auto anchors = interfaceAnchorLocalPoints();
        for (int i = 0; i < static_cast<int>(anchors.size()); ++i) {
            const bool hovered = (i == m_hoveredInterfaceIndex);
            const bool active = m_device.isL3Capable()
                             || m_device.type() == DeviceType::Switch
                             || m_device.type() == DeviceType::SwitchL3
                             || i < 2;
            const QColor dotBg = hovered ? accent.lighter(120)
                               : (active ? accent.lighter(180) : QColor("#e2e8f0"));
            const QColor dotBorder = hovered ? accent
                                   : (active ? accent.lighter(120) : QColor("#cbd5e1"));
            painter->setPen(QPen(dotBorder, hovered ? 1.6 : 1.0));
            painter->setBrush(dotBg);
            const qreal dr = hovered ? 7.0 : 5.0;
            painter->drawEllipse(anchors[static_cast<std::size_t>(i)], dr, dr);
        }
    }

protected:
    QVariant itemChange(GraphicsItemChange change, const QVariant& value) override {
        if (change == ItemPositionHasChanged && m_onMoved)
            m_onMoved(m_device.id(), value.toPointF());
        if (change == ItemPositionHasChanged)
            for (const auto& refresh : m_edgeRefreshers) refresh();
        return QGraphicsObject::itemChange(change, value);
    }

    void mousePressEvent(QGraphicsSceneMouseEvent* event) override {
        if (m_onSelected) m_onSelected(m_device.id());
        QGraphicsObject::mousePressEvent(event);
    }

    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override {
        if (m_onReleased) m_onReleased(m_device.id(), pos());
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
    void drawDeviceIcon(QPainter* painter, const QRectF& rect, const QColor& color) const {
        painter->save();
        painter->setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        painter->setBrush(Qt::NoBrush);
        switch (m_device.type()) {
        case DeviceType::Pc: {
            const qreal sw = rect.width() * 0.88;
            const qreal sh = rect.height() * 0.62;
            const QRectF screen(rect.center().x() - sw/2, rect.top(), sw, sh);
            painter->drawRoundedRect(screen, 1.5, 1.5);
            const qreal cx = rect.center().x();
            painter->drawLine(QPointF(cx, screen.bottom()), QPointF(cx, rect.bottom()));
            painter->drawLine(QPointF(cx - sw*0.35, rect.bottom()), QPointF(cx + sw*0.35, rect.bottom()));
            break;
        }
        case DeviceType::Laptop: {
            // Screen (top 60%)
            const qreal sw = rect.width() * 0.88;
            const qreal sh = rect.height() * 0.60;
            const QRectF screen(rect.center().x() - sw/2, rect.top(), sw, sh);
            painter->drawRoundedRect(screen, 1.5, 1.5);
            // Base (wider than screen, bottom 40%)
            const qreal bw = rect.width();
            const qreal bh = rect.height() * 0.28;
            const QRectF base(rect.center().x() - bw/2, screen.bottom(), bw, bh);
            painter->drawRoundedRect(base, 1.0, 1.0);
            break;
        }
        case DeviceType::Server: {
            // Tall rect with horizontal drive slots
            const QRectF body(rect.left(), rect.top(), rect.width(), rect.height());
            painter->drawRoundedRect(body, 1.5, 1.5);
            const qreal y1 = rect.top() + rect.height() * 0.33;
            const qreal y2 = rect.top() + rect.height() * 0.60;
            painter->drawLine(QPointF(rect.left()+2, y1), QPointF(rect.right()-2, y1));
            painter->drawLine(QPointF(rect.left()+2, y2), QPointF(rect.right()-2, y2));
            // Power LED
            painter->setPen(Qt::NoPen);
            painter->setBrush(color);
            painter->drawEllipse(QPointF(rect.right()-3.5, rect.top()+3.5), 2.0, 2.0);
            break;
        }
        case DeviceType::IpPhone: {
            // Rounded rect body + handset hint
            const QRectF body(rect.left(), rect.top() + rect.height()*0.1,
                              rect.width()*0.75, rect.height()*0.85);
            painter->drawRoundedRect(body, 2.5, 2.5);
            // Handset / earpiece line at top-right
            painter->drawLine(QPointF(body.right()+1, rect.top()+2),
                              QPointF(rect.right(), rect.top()+2));
            painter->drawLine(QPointF(rect.right(), rect.top()+2),
                              QPointF(rect.right(), rect.center().y()));
            break;
        }
        case DeviceType::IpCamera: {
            // Rectangle body
            const QRectF body(rect.left(), rect.top() + rect.height()*0.2,
                              rect.width()*0.65, rect.height()*0.6);
            painter->drawRoundedRect(body, 2.0, 2.0);
            // Lens circle on front face
            const QPointF lensCenter(body.right() + (rect.right() - body.right())*0.5,
                                     body.center().y());
            painter->drawEllipse(lensCenter, rect.width()*0.16, rect.width()*0.16);
            break;
        }
        case DeviceType::WirelessAp: {
            // Small rect body (lower third)
            const QRectF body(rect.left() + rect.width()*0.15, rect.top() + rect.height()*0.6,
                              rect.width()*0.7, rect.height()*0.35);
            painter->drawRoundedRect(body, 2.0, 2.0);
            // Three concentric wifi arcs above
            const QPointF apex(rect.center().x(), rect.top() + rect.height()*0.58);
            for (int arc = 1; arc <= 3; ++arc) {
                const qreal r2 = arc * rect.width() * 0.14;
                painter->drawArc(QRectF(apex.x()-r2, apex.y()-r2, 2*r2, 2*r2),
                                 30*16, 120*16);
            }
            break;
        }
        case DeviceType::Switch: {
            const QRectF box(rect.left(), rect.top() + rect.height()*0.22,
                             rect.width(), rect.height() * 0.56);
            painter->drawRoundedRect(box, 1.5, 1.5);
            painter->setPen(Qt::NoPen);
            painter->setBrush(color.lighter(155));
            for (int i = 1; i <= 5; ++i)
                painter->drawEllipse(QPointF(box.left() + i * box.width()/6.0, box.center().y()), 1.2, 1.2);
            break;
        }
        case DeviceType::SwitchL3: {
            const QRectF box(rect.left(), rect.top() + rect.height()*0.22,
                             rect.width(), rect.height() * 0.56);
            painter->drawRoundedRect(box, 1.5, 1.5);
            painter->setPen(Qt::NoPen);
            painter->setBrush(color.lighter(155));
            for (int i = 1; i <= 5; ++i)
                painter->drawEllipse(QPointF(box.left() + i * box.width()/6.0, box.center().y()), 1.2, 1.2);
            // L3 text label
            painter->setPen(QPen(color, 1.0));
            QFont f; f.setPixelSize(6); f.setBold(true);
            painter->setFont(f);
            painter->drawText(QRectF(rect.right()-8, rect.top(), 8, 10), Qt::AlignCenter, "L3");
            break;
        }
        case DeviceType::Router: {
            const qreal r = qMin(rect.width(), rect.height()) / 2.2;
            const QPointF c = rect.center();
            painter->drawEllipse(c, r, r);
            painter->setPen(QPen(color.lighter(135), 1.1, Qt::SolidLine, Qt::RoundCap));
            painter->drawLine(QPointF(c.x() - r*0.55, c.y()), QPointF(c.x() + r*0.55, c.y()));
            painter->drawLine(QPointF(c.x(), c.y() - r*0.55), QPointF(c.x(), c.y() + r*0.55));
            break;
        }
        case DeviceType::Firewall: {
            // Shield outline
            const qreal cx = rect.center().x();
            const qreal top = rect.top();
            const qreal bottom = rect.bottom();
            const qreal left = rect.left() + rect.width()*0.1;
            const qreal right = rect.right() - rect.width()*0.1;
            const qreal mid = rect.top() + rect.height()*0.55;
            QPainterPath shield;
            shield.moveTo(cx, top);
            shield.lineTo(right, top + rect.height()*0.18);
            shield.lineTo(right, mid);
            shield.quadTo(right, bottom, cx, bottom);
            shield.quadTo(left, bottom, left, mid);
            shield.lineTo(left, top + rect.height()*0.18);
            shield.closeSubpath();
            painter->drawPath(shield);
            break;
        }
        }
        painter->restore();
    }

    std::vector<QPointF> interfaceAnchorLocalPoints() const {
        std::vector<QPointF> anchors;
        const int count = static_cast<int>(m_device.interfaces().size());
        if (count <= 0) return anchors;

        const QRectF rect = boundingRect().adjusted(18.0, 18.0, -18.0, -12.0);
        if (count == 1) {
            anchors.emplace_back(0.0, rect.bottom());
            return anchors;
        }
        if (count <= 4) {
            for (int i = 0; i < count; ++i) {
                const qreal t = static_cast<qreal>(i) / static_cast<qreal>(count - 1);
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

// ─── Link edge ───────────────────────────────────────────────────────────────

class LinkEdgeItem final : public QGraphicsItem {
public:
    LinkEdgeItem(const Link& link, DeviceNodeItem* left, DeviceNodeItem* right)
        : m_link(link), m_left(left), m_right(right), m_selected(false), m_hovered(false) {
        setZValue(1.0);
        setAcceptHoverEvents(true);
    }

    int linkId() const { return m_link.id(); }

    void setSelectedState(bool selected) {
        if (m_selected == selected) return;
        m_selected = selected;
        update();
    }

    void refreshGeometry() {
        prepareGeometryChange();
        update();
    }

    QRectF boundingRect() const override {
        if (!m_left || !m_right) return {};
        const QPointF lp = m_left->interfaceAnchorScenePoint(m_link.leftInterfaceIndex());
        const QPointF rp = m_right->interfaceAnchorScenePoint(m_link.rightInterfaceIndex());
        const qreal dist = QLineF(lp, rp).length();
        const qreal bow = qBound(30.0, dist * 0.28, 90.0);
        return QRectF(lp, rp).normalized().adjusted(-50.0, -50.0, 50.0, bow + 40.0);
    }

    void paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) override {
        if (!m_left || !m_right) return;
        painter->setRenderHint(QPainter::Antialiasing, true);

        const QPointF p1 = mapFromScene(m_left->interfaceAnchorScenePoint(m_link.leftInterfaceIndex()));
        const QPointF p2 = mapFromScene(m_right->interfaceAnchorScenePoint(m_link.rightInterfaceIndex()));

        const qreal dist = QLineF(p1, p2).length();
        const qreal bow = qBound(30.0, dist * 0.28, 90.0);
        const QPointF c1(p1.x(), p1.y() + bow);
        const QPointF c2(p2.x(), p2.y() + bow);

        QPainterPath path;
        path.moveTo(p1);
        path.cubicTo(c1, c2, p2);

        const QColor lineColor = m_selected ? QColor("#3b82f6")
                               : (m_hovered ? QColor("#60a5fa") : QColor("#94a3b8"));
        const qreal lineWidth  = m_selected ? 3.5 : (m_hovered ? 2.8 : 2.0);

        painter->setPen(QPen(QColor(0, 0, 0, 18), lineWidth + 3, Qt::SolidLine, Qt::RoundCap));
        painter->drawPath(path);
        painter->setPen(QPen(lineColor, lineWidth, Qt::SolidLine, Qt::RoundCap));
        painter->drawPath(path);

        painter->setPen(Qt::NoPen);
        painter->setBrush(lineColor);
        painter->drawEllipse(p1, 3.5, 3.5);
        painter->drawEllipse(p2, 3.5, 3.5);

        painter->setPen(m_selected ? QColor("#1d4ed8") : QColor("#94a3b8"));
        QFont lf; lf.setFamily("Sans Serif"); lf.setPixelSize(8);
        painter->setFont(lf);
        painter->drawText(QRectF(p1.x()-22, p1.y()+9, 44, 12), Qt::AlignCenter,
                          QString("p%1").arg(m_link.leftInterfaceIndex() + 1));
        painter->drawText(QRectF(p2.x()-22, p2.y()+9, 44, 12), Qt::AlignCenter,
                          QString("p%1").arg(m_link.rightInterfaceIndex() + 1));
    }

protected:
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override {
        QGraphicsItem::mousePressEvent(event);
        event->accept();
    }

    void hoverEnterEvent(QGraphicsSceneHoverEvent* event) override {
        m_hovered = true; update();
        QGraphicsItem::hoverEnterEvent(event);
    }

    void hoverLeaveEvent(QGraphicsSceneHoverEvent* event) override {
        m_hovered = false; update();
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

// ─── TopologyScene ───────────────────────────────────────────────────────────

TopologyScene::TopologyScene(QObject* parent)
    : QGraphicsScene(parent),
      m_selectedDeviceId(-1),
      m_selectedLinkId(-1),
      m_temporaryCable(nullptr),
      m_dragDeviceId(-1),
      m_dragInterfaceIndex(-1) {
    setSceneRect(0.0, 0.0, 2400.0, 1600.0);
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
        auto* left  = deviceItems.value(link.leftDeviceId(),  nullptr);
        auto* right = deviceItems.value(link.rightDeviceId(), nullptr);
        if (!left || !right) continue;
        auto* edge = new LinkEdgeItem(link, left, right);
        left->addEdgeRefresh([edge]()  { edge->refreshGeometry(); });
        right->addEdgeRefresh([edge]() { edge->refreshGeometry(); });
        addItem(edge);
    }
}

void TopologyScene::setSelectedDeviceId(int deviceId) {
    if (m_selectedDeviceId == deviceId) return;
    m_selectedDeviceId = deviceId;
    m_selectedLinkId = -1;

    for (QGraphicsItem* item : items()) {
        if (auto* node = dynamic_cast<DeviceNodeItem*>(item))
            node->setHighlighted(node->deviceId() == m_selectedDeviceId);
        else if (auto* edge = dynamic_cast<LinkEdgeItem*>(item))
            edge->setSelectedState(false);
    }
}

void TopologyScene::drawBackground(QPainter* painter, const QRectF& rect) {
    painter->fillRect(rect, QColor("#f8fafc"));

    constexpr qreal step = 24.0;
    constexpr int   majorEvery = 4;

    painter->setPen(Qt::NoPen);

    const qreal startX = std::floor(rect.left() / step) * step;
    const qreal startY = std::floor(rect.top()  / step) * step;

    for (qreal x = startX; x <= rect.right() + step; x += step) {
        for (qreal y = startY; y <= rect.bottom() + step; y += step) {
            const int ix = static_cast<int>(std::round(x / step));
            const int iy = static_cast<int>(std::round(y / step));
            const bool major = (ix % majorEvery == 0) && (iy % majorEvery == 0);
            painter->setBrush(major ? QColor("#94a3b8") : QColor("#dde3ed"));
            const qreal r = major ? 2.0 : 1.2;
            painter->drawEllipse(QPointF(x, y), r, r);
        }
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
                if (auto* node = dynamic_cast<DeviceNodeItem*>(other))
                    node->setHighlighted(false);
                else if (auto* edgeItem = dynamic_cast<LinkEdgeItem*>(other))
                    edgeItem->setSelectedState(edgeItem->linkId() == m_selectedLinkId);
            }
            emit linkSelected(m_selectedLinkId);
            event->accept();
            return;
        }

        auto* node = dynamic_cast<DeviceNodeItem*>(item);
        if (!node) continue;

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
        for (QGraphicsItem* item : items(event->scenePos())) {
            auto* node = dynamic_cast<DeviceNodeItem*>(item);
            if (!node) continue;
            const int targetIndex = node->interfaceAtScenePoint(event->scenePos());
            if (targetIndex >= 0 &&
                !(node->deviceId() == m_dragDeviceId && targetIndex == m_dragInterfaceIndex)) {
                emit cableRequested(m_dragDeviceId, m_dragInterfaceIndex,
                                    node->deviceId(), targetIndex);
                break;
            }
        }
        clearInteractionArtifacts();
        event->accept();
        return;
    }
    QGraphicsScene::mouseReleaseEvent(event);
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
