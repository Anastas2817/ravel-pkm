#include "firefly.hpp"

#include <QGraphicsSceneMouseEvent>
#include <QPainter>
#include <QRadialGradient>

namespace ravel::ui {

namespace {
constexpr qreal kGlowRadius = 10.0;
// Hit area is larger than the glow: a moving 20px target is hard to click,
// while the bright core (~12px) is what the user aims at.
constexpr qreal kHitRadius = 18.0;
}  // namespace

FireFly::FireFly(std::vector<std::uint64_t> rels) : rels_(rels) {
  setFlag(QGraphicsItem::ItemIgnoresTransformations);
  setZValue(1);
}

QRectF FireFly::boundingRect() const {
  return QRectF(-kHitRadius, -kHitRadius, 2 * kHitRadius, 2 * kHitRadius);
}

void FireFly::paint(QPainter* painter, const QStyleOptionGraphicsItem*, QWidget*) {
  QRadialGradient gradient(0.0, 0.0, kGlowRadius);
  gradient.setColorAt(0.0, QColor(255, 255, 255, 230));
  gradient.setColorAt(1.0, QColor(255, 255, 255, 0));
  painter->setPen(Qt::NoPen);
  painter->setBrush(gradient);
  painter->drawEllipse(QPointF(0.0, 0.0), kGlowRadius, kGlowRadius);
}

void FireFly::mousePressEvent(QGraphicsSceneMouseEvent* e) {
  if (e->button() == Qt::LeftButton && on_clicked) {
    on_clicked();
    e->accept();  // consumed: nothing underneath should also react to this click
    return;
  }
  QGraphicsItem::mousePressEvent(e);
}

}  // namespace ravel::ui
