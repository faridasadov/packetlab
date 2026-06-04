#pragma once

#include <QGraphicsScene>

#include "../core/networkmodel.h"

class QGraphicsLineItem;
class QGraphicsSceneMouseEvent;
class QGraphicsSceneContextMenuEvent;

namespace packetlab {

class TopologyScene final : public QGraphicsScene {
    Q_OBJECT

public:
    explicit TopologyScene(QObject* parent = nullptr);

    void rebuild(const NetworkModel& model);
    void setSelectedDeviceId(int deviceId);

signals:
    void deviceSelected(int deviceId);
    void deviceMoved(int deviceId, QPointF position);
    void deviceMoveCommitted(int deviceId, QPointF position);
    void cableRequested(int leftDeviceId, int leftInterfaceIndex, int rightDeviceId, int rightInterfaceIndex);
    void linkSelected(int linkId);
    void deviceContextMenuRequested(int deviceId, QPointF screenPos);
    void linkContextMenuRequested(int linkId, QPointF screenPos);
    void canvasContextMenuRequested(QPointF screenPos);

private:
    void drawBackground(QPainter* painter, const QRectF& rect) override;
    void contextMenuEvent(QGraphicsSceneContextMenuEvent* event) override;
    void mousePressEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseMoveEvent(QGraphicsSceneMouseEvent* event) override;
    void mouseReleaseEvent(QGraphicsSceneMouseEvent* event) override;
    void addDeviceItem(const Device& device);
    void addLinkItem(const NetworkModel& model, const Link& link);
    void clearInteractionArtifacts();

    int m_selectedDeviceId;
    int m_selectedLinkId;
    QGraphicsLineItem* m_temporaryCable;
    int m_dragDeviceId;
    int m_dragInterfaceIndex;
};

}  // namespace packetlab
