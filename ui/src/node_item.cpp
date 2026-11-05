#ifdef _MSC_VER
// Qt: шум code analysis (PREfast, C26xxx) из чужих заголовков — см. main.cpp.
#pragma warning(disable : 26495 26813)
#endif

#include "node_item.hpp"

#include <QGraphicsScene>
#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <algorithm>
#include <cmath>
#include <limits>

namespace ravel::ui {

NodeItem::NodeItem(std::uint64_t id, QString title, QString type, double size, const QRectF& rect,
                   QString color, std::string shape, QString frame_color, QGraphicsItem* parent)
    : QGraphicsObject(parent),
      id_(id),
      title_(title),
      type_(type),
      size_(size),
      rect_(rect),
      color_(color),
      shape_(shape),
      frame_color_(frame_color) {
  this->setFlag(QGraphicsItem::ItemIsMovable);
  this->setFlag(QGraphicsItem::ItemSendsGeometryChanges);
  setFlag(QGraphicsItem::ItemIsSelectable);
  this->setAcceptHoverEvents(true);
}

void NodeItem::SetRect(const QRectF& r, double size) {
  if (r == rect_) {
    return;
  }
  prepareGeometryChange();
  rect_ = r;
  size_ = size;
  update();
}

QRectF NodeItem::boundingRect() const {
  if (shadow_ == 0) {
    return rect_;
  }
  const qreal k = shadow_;
  return rect_.adjusted(0, 0, 3.0 * k + 4.0 * k, 3.0 * k + 4.0 * k);
}

QPointF NodeItem::AnchorTowards(const QPointF& other_center) const {
  // Parametrize the ray center -> other and find where it exits the rect:
  // t = min over axes of half_extent / |direction_axis|. For an "oval" shape
  // this is the bounding-rect approximation (line may graze the corner).
  const QPointF self_center = pos();
  const QPointF d = other_center - self_center;
  if (d.x() == 0 && d.y() == 0) {
    return self_center;
  }
  const double hw = rect_.width() / 2.0;
  const double hh = rect_.height() / 2.0;
  constexpr double kInf = std::numeric_limits<double>::max();
  const double tx = (d.x() != 0) ? hw / std::abs(d.x()) : kInf;
  const double ty = (d.y() != 0) ? hh / std::abs(d.y()) : kInf;
  return self_center + d * std::min(tx, ty);
}

void NodeItem::paint(QPainter* painter, const QStyleOptionGraphicsItem* /*option*/,
                     QWidget* /*widget*/) {
  const auto draw_shape = [this, painter](const QRectF& r) {
    if (shape_ == "rect") {
      painter->drawRect(r);
    } else if (shape_ == "rounded_rect") {
      painter->drawRoundedRect(r, 10, 10);
    } else if (shape_ == "oval") {
      painter->drawEllipse(r);
    }
  };

  // shadow
  if (shadow_ > 0) {
    const qreal k = shadow_;
    const QRectF base = rect_.translated(3.0 * k, 3.0 * k);
    constexpr QColor kShadowColor(64, 64, 64);
    painter->save();
    painter->setPen(Qt::NoPen);
    const int blur = static_cast<int>(4.0 * k);
    for (int i = blur; i >= 0; --i) {
      QColor c = kShadowColor;
      c.setAlphaF(0.35 * (1.0 - qreal(i) / (blur + 1)));
      painter->setBrush(c);
      draw_shape(base.adjusted(0, 0, i, i));
    }
    painter->restore();
  }

  // shape
  painter->setBrush(QColor(color_));
  painter->setPen(QColor(frame_color_));
  draw_shape(rect_);

  // text
  QFont f = painter->font();
  f.setPointSizeF(f.pointSizeF() * size_);
  painter->setFont(f);
  painter->drawText(rect_, Qt::AlignCenter, title_);
}

void NodeItem::Change(const QString& title, const QString& type, const QString& color,
                      const std::string& shape, const QString& frame_color) {
  if (title_ == title && type_ == type && color_ == color && shape_ == shape &&
      frame_color_ == frame_color) {
    return;
  }
  title_ = title;
  type_ = type;
  color_ = color;
  shape_ = shape;
  frame_color_ = frame_color;
  update();
}

QVariant NodeItem::itemChange(GraphicsItemChange change, const QVariant& value) {
  // Notify AFTER the position is committed, not during ItemPositionChange:
  // Moved handlers touch other items (RelationItem::setLine), and mutating
  // the scene from inside the change-negotiation phase makes the scene drop
  // or defer those repaint requests (stale arrows, smear trails).
  if (change == ItemPositionHasChanged) {
    emit Moved(id_, pos());
  }
  return QGraphicsObject::itemChange(change, value);
}

void NodeItem::mousePressEvent(QGraphicsSceneMouseEvent* e) {
  if (e->button() == Qt::LeftButton && (e->modifiers() & Qt::ControlModifier)) {
    rel_ = new QGraphicsLineItem;
    rel_->setLine(QLineF(pos(), e->scenePos()));
    scene()->addItem(rel_);
    return;
  }
  if (e->button() == Qt::LeftButton) {
    drag_start_ = this->pos();
  }
  QGraphicsObject::mousePressEvent(e);
}

void NodeItem::mouseReleaseEvent(QGraphicsSceneMouseEvent* e) {
  if (rel_ != nullptr) {
    scene()->removeItem(rel_);
    delete rel_;
    rel_ = nullptr;
    if (e->button() == Qt::LeftButton) {
      const QList<QGraphicsItem*> under_cursor = scene()->items(e->scenePos());
      for (QGraphicsItem* cur : under_cursor) {
        if (auto* target = dynamic_cast<NodeItem*>(cur); target != nullptr && target != this) {
          emit RelationFinished(id_, target->id_, title_, target->title_);
          break;
        }
      }
    }
    return;
  }
  if (e->button() == Qt::LeftButton) {
    QPointF cur = this->pos();
    if (cur != drag_start_) {
      emit MoveFinished(id_, cur);
    }
  }
  QGraphicsObject::mouseReleaseEvent(e);
}

void NodeItem::mouseMoveEvent(QGraphicsSceneMouseEvent* e) {
  if (rel_ != nullptr) {
    rel_->setLine(QLineF(pos(), e->scenePos()));
    return;
  }
  QGraphicsObject::mouseMoveEvent(e);
}

}  // namespace ravel::ui
