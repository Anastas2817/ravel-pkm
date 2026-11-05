#include "canvas_view.hpp"

#include <QScrollBar>

namespace ravel::ui {

CanvasView::CanvasView(QWidget* parent) : QGraphicsView(parent) {
  setTransformationAnchor(QGraphicsView::AnchorUnderMouse);
  setDragMode(QGraphicsView::RubberBandDrag);
  setRenderHint(QPainter::Antialiasing);
}

void CanvasView::wheelEvent(QWheelEvent* e) {
  if (e->modifiers() & Qt::ControlModifier) {
    constexpr double kFactor = 1.15;
    const double factor = (e->angleDelta().y() > 0) ? kFactor : 1.0 / kFactor;
    const double next = transform().m11() * factor;
    if (next >= 0.2 && next <= 5.0) {
      scale(factor, factor);
    }
    e->accept();
    return;
  }
  int delta = e->angleDelta().y();
  if (delta == 0) {
    delta = e->angleDelta().x();
  }
  QScrollBar* bar =
      (e->modifiers() & Qt::ShiftModifier) ? horizontalScrollBar() : verticalScrollBar();
  bar->setValue(bar->value() - delta);
  e->accept();
}

}  // namespace ravel::ui
