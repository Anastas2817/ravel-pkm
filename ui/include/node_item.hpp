#ifndef RAVEL_UI_INCLUDE_NODE_ITEM_HPP_
#define RAVEL_UI_INCLUDE_NODE_ITEM_HPP_

#include <QGraphicsLineItem>
#include <QGraphicsObject>
#include <QString>
#include <cstdint>
#include <string>

namespace ravel::ui {

class NodeItem : public QGraphicsObject {
  Q_OBJECT

 public:
  NodeItem(std::uint64_t id, QString title, QString type, double size, const QRectF& rect,
           QString color, std::string shape, QString frame_color, QGraphicsItem* parent = nullptr);

  std::uint64_t id() const {
    return id_;
  }
  QString title() const {
    return title_;
  }
  QString GetType() const {
    return type_;
  }
  QRectF boundingRect() const override;
  void SetRect(const QRectF& r, double size);
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

  void Change(const QString& title, const QString& type, const QString& color,
              const std::string& shape, const QString& frame_color);

  // Border point of this node's rect facing `other_center` — where a
  // connection arriving from that direction should attach, so the line ends
  // at the border instead of being hidden under the node.
  [[nodiscard]] QPointF AnchorTowards(const QPointF& other_center) const;

  void SetShadowSize(quint8 overflow) {
    prepareGeometryChange();
    shadow_ = overflow;
    update();
  }
  [[nodiscard]] quint8 shadow() const {
    return shadow_;
  }

 signals:
  void Moved(std::uint64_t id, const QPointF& new_pos);
  void MoveFinished(std::uint64_t id, const QPointF& pos);
  void RelationFinished(std::uint64_t from, std::uint64_t to, QString from_title, QString to_title);

 protected:
  QVariant itemChange(GraphicsItemChange change, const QVariant& value) override;
  void mouseReleaseEvent(QGraphicsSceneMouseEvent* e) override;
  void mousePressEvent(QGraphicsSceneMouseEvent* e) override;
  void mouseMoveEvent(QGraphicsSceneMouseEvent* e) override;

 private:
  std::uint64_t id_;
  QString title_;
  double size_;

  QString type_;
  QString color_;
  std::string shape_;
  QString frame_color_;

  QPointF drag_start_;
  QRectF rect_;
  QGraphicsLineItem* rel_ = nullptr;

  quint8 shadow_ = 0;
};

}  // namespace ravel::ui

#endif  // RAVEL_UI_INCLUDE_NODE_ITEM_HPP_
