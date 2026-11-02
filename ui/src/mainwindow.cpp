#include "mainwindow.hpp"

#include "add_node_dialog.hpp"
#include "add_relation_dialog.hpp"
#include "firefly.hpp"
#include "node_item.hpp"
#include "open_node_dialog.hpp"
#include "open_relation_dialog.hpp"
#include "relation_item.hpp"

#include <QFileDialog>
#include <QKeySequence>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QStringList>
#include <QTimer>
#include <QVariantAnimation>
#include <algorithm>
#include <cmath>
#include <optional>

namespace ravel::ui {

namespace {
// Border-to-border line between two nodes.
QLineF RelationLine(const NodeItem* from, const NodeItem* to) {
  return QLineF(from->AnchorTowards(to->pos()), to->AnchorTowards(from->pos()));
}
}  // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent),
      scene_(new QGraphicsScene(this)),
      view_(new CanvasView(this)),
      open_action_(new QAction(tr("Открыть…"), this)),
      save_action_(new QAction(tr("Сохранить"), this)),
      save_as_action_(new QAction(tr("Сохранить как…"), this)),
      undo_action_(new QAction(tr("Отменить"), this)),
      redo_action_(new QAction(tr("Восстановить"), this)),
      delete_action_(new QAction(tr("Удалить"), this)) {
  setCentralWidget(view_);
  view_->setScene(scene_);
  // Full redraw instead of the default MinimalViewportUpdate: with region
  // updates the scene left smear trails behind dragged relations (the root
  // cause sits in Qt's deferred dirty-item processing; not worth chasing for
  // this scale). At 10^2-10^3 items a full viewport repaint is unnoticeable.
  view_->setViewportUpdateMode(QGraphicsView::FullViewportUpdate);

  open_action_->setShortcut(QKeySequence::Open);  // Ctrl+O
  connect(open_action_, &QAction::triggered, this, &MainWindow::OnOpen);

  save_action_->setShortcut(QKeySequence::Save);  // Ctrl+S
  connect(save_action_, &QAction::triggered, this, &MainWindow::OnSave);

  save_as_action_->setShortcut(QKeySequence::SaveAs);  // Ctrl+Shift+S
  connect(save_as_action_, &QAction::triggered, this, &MainWindow::OnSaveAs);

  undo_action_->setShortcut(QKeySequence::Undo);  // Ctrl+Z
  connect(undo_action_, &QAction::triggered, this, &MainWindow::OnUndo);

  redo_action_->setShortcut(QKeySequence::Redo);  // Ctrl+Y on Windows, Ctrl+Shift+Z on macOS/Linux
  connect(redo_action_, &QAction::triggered, this, &MainWindow::OnRedo);

  delete_action_->setShortcuts({QKeySequence("Ctrl+D"), QKeySequence::Delete});
  connect(delete_action_, &QAction::triggered, this, &MainWindow::OnDeleteSelected);

  connect(view_, &CanvasView::DoubleClicked, this, &MainWindow::OnSceneDoubleClick);

  QMenu* file_menu = menuBar()->addMenu(tr("&Файл"));
  file_menu->addAction(open_action_);
  file_menu->addAction(save_action_);
  file_menu->addAction(save_as_action_);
  QMenu* edit = menuBar()->addMenu(tr("&Правка"));
  edit->addAction(undo_action_);
  edit->addAction(redo_action_);
  edit->addAction(delete_action_);

  statusBar()->showMessage(tr("Готово"));
}

QString MainWindow::ErrorToString(ravel::app::ControllerError err) {
  using ravel::app::ControllerError;
  switch (err) {
    case ControllerError::NoPath:
      return tr("Файл не найден (или путь не задан).");
    case ControllerError::IoError:
      return tr("Ошибка чтения/записи файла (битый JSON или не та версия формата).");
    case ControllerError::NothingToUndo:
      return tr("Нечего отменять.");
    case ControllerError::NothingToRedo:
      return tr("Нечего возвращать.");
    default:
      return tr("Неизвестная ошибка.");
  }
}

void MainWindow::OnOpen() {
  const QString path =
      QFileDialog::getOpenFileName(this, tr("Открыть файл"), QString(), tr("Ravel (*.ravel.json)"));
  if (path.isEmpty()) {
    return;  // user close the dialog
  }
  const ravel::app::ControllerResult result =
      controller_.Open(std::filesystem::path(path.toStdWString()));
  if (!result.has_value()) {
    QMessageBox::warning(this, tr("Ошибка открытия"), ErrorToString(result.error()));
    return;
  }
  ResyncScene();
  statusBar()->showMessage(tr("Открыто: %1").arg(path));
}

void MainWindow::OnSave() {
  const ravel::app::ControllerResult result = controller_.Save();
  if (!result.has_value()) {
    // NoPath: Ctrl+S -> SaveAs.
    if (result.error() == ravel::app::ControllerError::NoPath) {
      OnSaveAs();
      return;
    }
    QMessageBox::warning(this, tr("Ошибка сохранения"), ErrorToString(result.error()));
    return;
  }
  statusBar()->showMessage(tr("Сохранено"));
}

void MainWindow::OnSaveAs() {
  const QString path = QFileDialog::getSaveFileName(this, tr("Сохранить файл"), QString(),
                                                    tr("Ravel (*.ravel.json)"));
  if (path.isEmpty()) {
    return;  // user close the dialog
  }
  const ravel::app::ControllerResult result =
      controller_.SaveAs(std::filesystem::path(path.toStdWString()));
  if (!result.has_value()) {
    QMessageBox::warning(this, tr("Ошибка сохранения"), ErrorToString(result.error()));
    return;
  }
  statusBar()->showMessage(tr("Сохранено: %1").arg(path));
}

void MainWindow::ResyncScene() {
  const std::vector<ravel::app::NodeView> nodes = controller_.Nodes();
  const std::vector<ravel::app::RelationView> relations = controller_.Relations();

  QHash<quint64, NodeItem*> old_nodes;
  old_nodes.swap(node_items_);
  QHash<quint64, RelationItem*> old_rels;
  old_rels.swap(relation_items_);

  for (const ravel::app::NodeView& view : nodes) {
    // NodeItem rect is center-origin: setPos() places the model position at the
    // visual center of the node.
    const qreal kWidth = 140.0 * view.size;
    const qreal kHeight = 60.0 * view.size;
    const QRectF rect(-kWidth / 2, -kHeight / 2, kWidth, kHeight);
    NodeItem* item = old_nodes.take(static_cast<quint64>(view.id));
    if (item == nullptr) {
      item = new NodeItem(view.id, QString::fromStdString(view.title),
                          QString::fromStdString(view.type), view.size, rect,
                          QString::fromStdString(view.color), view.shape,
                          QString::fromStdString(view.frame_color));
      scene_->addItem(item);
      connect(item, &NodeItem::MoveFinished, this, &MainWindow::OnNodeMoved);
      connect(item, &NodeItem::RelationFinished, this, &MainWindow::OnAddRelation);
      // Per-step geometry broadcast: RelationItems follow the dragged node.
      connect(item, &NodeItem::Moved, this, &MainWindow::OnNodeDragged);
    }
    node_items_.insert(view.id, item);
    if (view.position.has_value()) {
      const QPointF target(view.position->x_, view.position->y_);
      if (item->pos() != target) {
        item->setPos(target);
      }
    }
    if (item->boundingRect() != rect) {
      item->SetRect(rect, view.size);
    }
    // QString vs std::string has no operator!= (GCC/Qt6 on Linux) —
    // convert once, use for both the guard and the update.
    const QString title = QString::fromStdString(view.title);
    const QString type = QString::fromStdString(view.type);
    if (item->title() != title || item->GetType() != type) {
      item->Change(title, type, QString::fromStdString(view.color), view.shape,
                   QString::fromStdString(view.frame_color));
    }
  }

  // Whatever is left in `items` no longer exists in the model. removeItem()
  // passes ownership to the caller, so we delete explicitly (no QObject
  // parent-child for scene items).
  for (NodeItem* item : old_nodes) {
    scene_->removeItem(item);
    delete item;
  }

  // Reconcile relation items by model id (same pattern as nodes). Geometry is
  // refreshed on every resync; live following during drag is driven by
  // NodeItem::Moved (OnNodeDragged).
  for (const ravel::app::RelationView& view : relations) {
    RelationItem* rel = old_rels.take(view.id);
    if (rel == nullptr) {
      rel = new RelationItem(view.id, view.from, view.to, QString::fromStdString(view.color),
                             view.arrow, view.symmetric);
      scene_->addItem(rel);
    }
    relation_items_.insert(view.id, rel);
    const NodeItem* from_item = FindNode(view.from);
    const NodeItem* to_item = FindNode(view.to);
    if (from_item != nullptr && to_item != nullptr) {
      rel->setLine(RelationLine(from_item, to_item));
    }
    const QString rel_type = QString::fromStdString(view.type);
    if (rel->GetType() != rel_type) {
      rel->Change(rel_type, QString::fromStdString(view.color), view.arrow, view.symmetric);
    }
  }
  for (RelationItem* rel : old_rels) {
    scene_->removeItem(rel);
    delete rel;
  }

  RefreshCycleHints();
}

static QStringList ToQStringList(const std::vector<std::string>& v) {
  QStringList out;
  out.reserve(static_cast<qsizetype>(v.size()));
  for (const auto& s : v) {
    out << QString::fromStdString(s);
  }
  return out;
}

void MainWindow::OnSceneDoubleClick(QPointF pos) {
  RelationItem* rel = nullptr;
  for (QGraphicsItem* item : scene_->items(pos)) {
    if (auto* node = dynamic_cast<NodeItem*>(item)) {
      std::optional<ravel::app::NodeView> cur = controller_.FindNode(node->id());
      if (!cur) {
        return;
      }
      OpenNodeDialog dlg(cur.value(), ToQStringList(controller_.NodeTypeNames()), this);
      dlg.setWindowOpacity(0.9);
      dlg.setWindowTitle(tr("Редактирование понятия"));
      if (dlg.exec() != QDialog::Accepted) {
        return;
      }
      const ravel::app::CommandResult result =
          controller_.ChangeNode(node->id(), dlg.title().toStdString(), dlg.type().toStdString(),
                                 dlg.description().toStdString(), dlg.size());
      if (!result.has_value()) {
        QMessageBox::warning(this, tr("Ошибка редактирования"),
                             tr("Не удалось изменить узел: неизвестный тип (онтология пуста?)."));
        return;
      }
      ResyncScene();
      return;
    } else if (auto* cur = dynamic_cast<RelationItem*>(item)) {
      rel = cur;
    }
  }

  if (rel != nullptr) {
    std::optional<ravel::app::RelationView> cur = controller_.FindRelation(rel->id());
    if (!cur) {
      return;
    }
    std::optional<ravel::app::NodeView> from = controller_.FindNode(cur.value().from);
    std::optional<ravel::app::NodeView> to = controller_.FindNode(cur.value().to);
    if (!from || !to) {
      return;
    }
    OpenRelationDialog dlg(cur.value(), ToQStringList(controller_.RelationTypeNames()),
                           QString::fromStdString(from.value().title),
                           QString::fromStdString(to.value().title), this);
    dlg.setWindowOpacity(0.9);
    dlg.setWindowTitle(tr("Редактирование связи"));
    if (dlg.exec() != QDialog::Accepted) {
      return;
    }
    const ravel::app::CommandResult result = controller_.ChangeRelation(
        rel->id(), dlg.type().toStdString(), dlg.description().toStdString());
    if (!result.has_value()) {
      QMessageBox::warning(this, tr("Ошибка редактирования"),
                           tr("Не удалось изменить связь: неизвестный тип (онтология пуста?)."));
      return;
    }
  } else {
    AddNodeDialog dlg(ToQStringList(controller_.NodeTypeNames()), this);
    dlg.setWindowOpacity(0.9);
    dlg.setWindowTitle(tr("Новое понятие"));
    if (dlg.exec() != QDialog::Accepted) {
      return;
    }
    const ravel::app::CommandResult result = controller_.AddNode(
        dlg.title().toStdString(), dlg.type().toStdString(), dlg.description().toStdString(),
        dlg.size(), ravel::Position{pos.x(), pos.y()});
    if (!result.has_value()) {
      QMessageBox::warning(this, tr("Ошибка создания"),
                           tr("Не удалось создать узел: неизвестный тип (онтология пуста?)."));
      return;
    }
  }

  ResyncScene();
}

void MainWindow::OnAddRelation(std::uint64_t from, std::uint64_t to, const QString& from_title,
                               const QString& to_title) {
  AddRelationDialog dlg(ToQStringList(controller_.RelationTypeNames()), from_title, to_title, this);
  dlg.setWindowOpacity(0.9);
  dlg.setWindowTitle(tr("Новая связь"));
  if (dlg.exec() != QDialog::Accepted) {
    return;
  }
  const ravel::app::CommandResult result =
      controller_.AddRelation(from, to, dlg.type().toStdString(), dlg.description().toStdString());
  if (!result.has_value()) {
    QMessageBox::warning(this, tr("Ошибка создания"),
                         tr("Не удалось создать отношение: неизвестный тип (онтология пуста?)."));
    return;
  }
  ResyncScene();
}

void MainWindow::OnUndo() {
  const ravel::app::CommandResult result = controller_.Undo();
  if (!result.has_value()) {
    QMessageBox::warning(this, tr("Ошибка отмены"), tr("нечего отменять?"));
    return;
  }
  ResyncScene();
}

void MainWindow::OnRedo() {
  const ravel::app::CommandResult result = controller_.Redo();
  if (!result.has_value()) {
    QMessageBox::warning(this, tr("Ошибка отмены"), tr("нечего восстанавливать?"));
    return;
  }
  ResyncScene();
}

void MainWindow::OnNodeMoved(std::uint64_t id, QPointF to) {
  const ravel::app::CommandResult result =
      controller_.MoveNode(id, ravel::Position{to.x(), to.y()});
  if (!result.has_value()) {
    QMessageBox::warning(this, tr("Ошибка перемещения"), tr("не отменить операцию"));
    return;
  }
  ResyncScene();
}

NodeItem* MainWindow::FindNode(std::uint64_t id) const {
  return node_items_.value(id);
}

void MainWindow::OnNodeDragged(std::uint64_t id, const QPointF& /*new_pos*/) {
  // new_pos equals the node's pos(): Moved is emitted from ItemPositionHasChanged,
  // after the position is committed.
  for (QGraphicsItem* item : scene_->items()) {
    auto* rel = dynamic_cast<RelationItem*>(item);
    if (rel == nullptr || (rel->from() != id && rel->to() != id)) {
      continue;
    }
    const NodeItem* from_item = FindNode(rel->from());
    const NodeItem* to_item = FindNode(rel->to());
    if (from_item == nullptr || to_item == nullptr) {
      continue;
    }
    rel->setLine(RelationLine(from_item, to_item));
  }
}

void MainWindow::OnDeleteSelected() {
  std::vector<ravel::NodeId> node_ids;
  std::vector<ravel::RelationId> rel_ids;
  for (QGraphicsItem* item : scene_->selectedItems()) {
    if (auto* node = dynamic_cast<NodeItem*>(item)) {
      node_ids.push_back(node->id());
    } else if (auto* rel = dynamic_cast<RelationItem*>(item)) {
      rel_ids.push_back(rel->id());
    }
  }
  if (node_ids.empty() && rel_ids.empty()) {
    return;
  }

  const ravel::app::CommandResult r = controller_.Delete(node_ids, rel_ids);
  if (!r.has_value()) {
    QMessageBox::warning(this, tr("Ошибка удаления"), tr("не удалось удалить"));
    return;
  }

  ResyncScene();
}

void MainWindow::RefreshCycleHints() {
  const auto hints = controller_.CycleHints();

  // Collect first, retire after: RetireFireFly take()s the hash entry, so it
  // must still be present — no mutating the hash mid-iteration.
  std::vector<FireFly*> dead;
  const QList<FireFly*> flies = fire_flies_.keys();
  for (FireFly* ff : flies) {
    if (std::find(hints.begin(), hints.end(), ff->rels()) == hints.end()) {
      dead.push_back(ff);
    }
  }
  for (FireFly* ff : dead) {
    RetireFireFly(ff);
  }

  for (const std::vector<ravel::RelationId>& hint : hints) {
    const bool exists = std::any_of(fire_flies_.keyBegin(), fire_flies_.keyEnd(),
                                    [&](const FireFly* ff) { return ff->rels() == hint; });
    if (!exists) {
      CreateFireFly(hint);
    }
  }
}

void MainWindow::CreateFireFly(const std::vector<std::uint64_t>& hint) {
  auto* ff = new FireFly(hint);
  scene_->addItem(ff);
  // Route: three full rounds over the cycle's edges. Captured by value:
  // `hint` refers to a caller-local that dies before the animation's first tick.
  std::vector<std::uint64_t> path = hint;
  // A cycle is born when its newest edge is added and ids grow monotonically,
  // so the max id in a fresh hint is exactly that edge. Rotate the closed walk
  // to start there: any rotation of a cycle is still a valid route.
  const auto newest = std::max_element(path.begin(), path.end());
  std::rotate(path.begin(), newest, path.end());
  constexpr int kRounds = 3;
  auto* flight = new QVariantAnimation(this);
  // Parameter t runs 0..size*rounds: integer part picks the edge, fraction
  // picks the point along it. 3000 ms per edge per round.
  flight->setDuration(3000 * static_cast<int>(path.size()) * kRounds);
  flight->setStartValue(0.0);
  flight->setEndValue(static_cast<double>(path.size()) * kRounds);
  flight->setEasingCurve(QEasingCurve::InOutSine);
  connect(flight, &QVariantAnimation::valueChanged, this, [this, ff, path](const QVariant& v) {
    const double t = v.toDouble();
    const std::size_t edge_idx = static_cast<std::size_t>(std::floor(t)) % path.size();
    const double frac = t - std::floor(t);
    RelationItem* rel = relation_items_.value(path[edge_idx]);
    if (rel == nullptr) {
      return;
    }
    ff->setPos(rel->line().pointAt(frac));
  });
  // Resting pulse per advisors: after landing the cycle's edges breathe
  // (opacity 1.0 -> 0.6 -> 1.0) until the hint goes away.
  auto* pulse = new QVariantAnimation(this);
  pulse->setDuration(1600);
  pulse->setLoopCount(-1);
  pulse->setEasingCurve(QEasingCurve::InOutSine);
  pulse->setKeyValueAt(0.0, 1.0);
  pulse->setKeyValueAt(0.5, 0.35);
  pulse->setKeyValueAt(1.0, 1.0);
  connect(pulse, &QVariantAnimation::valueChanged, this, [this, path](const QVariant& v) {
    for (const std::uint64_t rel_id : path) {
      if (RelationItem* rel = relation_items_.value(rel_id)) {
        rel->setOpacity(v.toDouble());
      }
    }
  });
  connect(flight, &QVariantAnimation::finished, this, [pulse] { pulse->start(); });
  // The hash owns the animations for the fly's whole lifetime. Do NOT delete on
  // finished: the hash entry would dangle and the removal branch in
  // RefreshCycleHints would stop() a dead pointer.
  fire_flies_.insert(ff, {flight, pulse});
  // Defer past the press dispatch: accepting the press made the scene this
  // item's mouse grabber, and deleting it from inside the event handler
  // would leave a dangling grabber (crash at scene destruction).
  ff->on_clicked = [this, ff] {
    QTimer::singleShot(0, this, [this, ff] { OnFireFlyClicked(ff); });
  };
  flight->start();
}

void MainWindow::RetireFireFly(FireFly* ff) {
  const FlyAnims anims = fire_flies_.take(ff);
  // Stop before deleting: a running animation would still tick valueChanged
  // into the dead item.
  anims.flight->stop();
  delete anims.flight;
  if (anims.pulse != nullptr) {
    anims.pulse->stop();
    delete anims.pulse;
  }
  // A stopped pulse may have left the edges dimmed: restore them.
  for (const std::uint64_t rel_id : ff->rels()) {
    if (RelationItem* rel = relation_items_.value(rel_id)) {
      rel->setOpacity(1.0);
    }
  }
  scene_->removeItem(ff);
  delete ff;
}

void MainWindow::OnFireFlyClicked(FireFly* ff) {
  // "Нет" — кнопка по умолчанию: пометка необратима до удаления рёбер, Enter
  // не должен срабатывать случайно.
  const auto answer = QMessageBox::question(
      this, tr("Циклическая связь"),
      tr("Пометить этот цикл как допустимый?\nСветлячок больше не будет появляться."),
      QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
  if (answer != QMessageBox::Yes) {
    return;
  }
  controller_.MarkCycleAllowed(ff->rels());
  // The press was accepted, so the scene may hold ff as the mouse grabber —
  // release the grab before deleting, else the grabber pointer dangles.
  if (scene_->mouseGrabberItem() == ff) {
    ff->ungrabMouse();
  }
  RetireFireFly(ff);
}

}  // namespace ravel::ui
