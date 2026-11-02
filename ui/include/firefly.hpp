#ifndef RAVEL_UI_INCLUDE_FIREFLY_HPP_
#define RAVEL_UI_INCLUDE_FIREFLY_HPP_

#include <QGraphicsItem>
#include <QGraphicsSceneMouseEvent>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <vector>

class QPainter;
class QStyleOptionGraphicsItem;
class QWidget;

namespace ravel::ui {

class FireFly : public QGraphicsItem {
 public:
  FireFly(std::vector<std::uint64_t> rels);

  const std::vector<uint64_t>& rels() const noexcept {
    return rels_;
  }

  // Set by MainWindow after creation; invoked on click. A plain callback
  // keeps FireFly a non-QObject graphics item.
  std::function<void()> on_clicked;

  QRectF boundingRect() const override;
  void paint(QPainter* painter, const QStyleOptionGraphicsItem* option, QWidget* widget) override;

 protected:
  void mousePressEvent(QGraphicsSceneMouseEvent* e) override;

 private:
  std::vector<std::uint64_t> rels_;
};

}  // namespace ravel::ui

#endif  // RAVEL_UI_INCLUDE_FIREFLY_HPP_
