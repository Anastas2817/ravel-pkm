#ifndef RAVEL_UI_INCLUDE_MAINWINDOW_HPP_
#define RAVEL_UI_INCLUDE_MAINWINDOW_HPP_

#include "canvas_view.hpp"
#include "controller.hpp"
#include "firefly.hpp"
#include "node_item.hpp"
#include "relation_item.hpp"

#include <QAction>
#include <QGraphicsScene>
#include <QHash>
#include <QMainWindow>
#include <cstdint>

class QVariantAnimation;

namespace ravel::ui {

class NodeItem;

class MainWindow : public QMainWindow {
  Q_OBJECT
 public:
  explicit MainWindow(QWidget* parent = nullptr);

 private slots:
  void OnOpen();
  void OnSave();
  void OnSaveAs();
  void OnSceneDoubleClick(QPointF pos);
  void ResyncScene();
  void OnUndo();
  void OnRedo();
  void OnNodeMoved(std::uint64_t id, QPointF to);
  void OnNodeDragged(std::uint64_t id, const QPointF& new_pos);
  void OnAddRelation(std::uint64_t from, std::uint64_t to, const QString& from_title,
                     const QString& to_title);
  void OnDeleteSelected();
  void RefreshCycleHints();

 private:
  void CreateFireFly(const std::vector<std::uint64_t>& hint);
  // Stop both animations, restore edge opacity, remove and delete. The single
  // teardown path for RefreshCycleHints and the "mark allowed" click.
  void RetireFireFly(FireFly* ff);
  void OnFireFlyClicked(FireFly* ff);
  static QString ErrorToString(ravel::app::ControllerError err);
  NodeItem* FindNode(std::uint64_t id) const;

  QGraphicsScene* scene_;
  CanvasView* view_;
  ravel::app::Controller controller_;
  QAction* open_action_;
  QAction* save_action_;
  QAction* save_as_action_;
  QAction* undo_action_;
  QAction* redo_action_;
  QAction* delete_action_;

  // Derived at Resync: non-owning, scene owns the items. Rebuilt wholesale.
  QHash<quint64, RelationItem*> relation_items_;
  QHash<quint64, NodeItem*> node_items_;
  // Owning: firefly -> its animations (flight while airborne, resting pulse
  // after landing). Key presence == hint is alive.
  struct FlyAnims {
    QVariantAnimation* flight = nullptr;
    QVariantAnimation* pulse = nullptr;
  };
  QHash<FireFly*, FlyAnims> fire_flies_;
};

}  // namespace ravel::ui

#endif  // RAVEL_UI_INCLUDE_MAINWINDOW_HPP_
