// Тесты Controller (app layer). Observable surface: протокол команд
// (expected с кодом ошибки), файловые эффекты Open/Save/SaveAs и read-API
// (Nodes()/NodeTypeNames() — DTO NodeView). Проверяем это; состояние графа
// покрывают graph_test (ядро) и property_test (оракулы).
//
// Файловые тесты работают в temp_directory_path() с уникальными именами
// и подчищают за собой. SaveAs атомарен (temp + rename) — его корректность
// проверяем косвенно: записанный файл должен читаться JsonStorage::Load.

#include "controller.hpp"

#include "storage.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <stdexcept>

#include <gtest/gtest.h>

namespace {

namespace fs = std::filesystem;

// Фикстура: уникальные пути во временном каталоге + подчистка (включая
// возможный недоперемещённый .tmp — след атомарного SaveAs при сбоях).
class ControllerIoTest : public ::testing::Test {
 protected:
  void TearDown() override {
    std::error_code ec;
    fs::remove(good_path_, ec);
    fs::remove(good_path_.string() + ".tmp", ec);
    fs::remove(source_path_, ec);
  }

  fs::path good_path_ = fs::temp_directory_path() / "ravel_ctrl_good.json";
  fs::path source_path_ = fs::temp_directory_path() / "ravel_ctrl_source.json";
};

// Готовит файл с графом, в котором есть онтология (тип "nt"): контроллеру
// нужен такой файл, чтобы команды добавления проходили успешно — своего
// API для AddNodeType у Controller нет (это намеренно: онтология в v1
// задаётся загруженным файлом).
void WriteGraphWithOntology(const fs::path& p) {
  ravel::Graph g;
  g.AddNodeType("nt", "#112233", "rect", "#445566");
  g.AddRelationType("rt", "#112233", "solid", false, true);
  std::ofstream out(p, std::ios::binary);
  ravel::JsonStorage::Save(g.ToSnapshot(), out);
}

TEST_F(ControllerIoTest, OpenMissingFileReturnsNoPath) {
  ravel::app::Controller c;
  const auto res = c.Open(good_path_);
  ASSERT_FALSE(res.has_value());
  EXPECT_EQ(res.error(), ravel::app::ControllerError::NoPath);
}

TEST_F(ControllerIoTest, OpenGarbageFileReturnsIoError) {
  {
    std::ofstream out(good_path_, std::ios::binary);
    out << "this is not json {{{";
  }
  ravel::app::Controller c;
  const auto res = c.Open(good_path_);
  ASSERT_FALSE(res.has_value());
  EXPECT_EQ(res.error(), ravel::app::ControllerError::IoError);
}

TEST(ControllerSession, FreshControllerHasNothingToUndo) {
  ravel::app::Controller c;
  const auto res = c.Undo();
  ASSERT_FALSE(res.has_value());
  EXPECT_EQ(res.error(), ravel::app::CommandError::NothingToUndo);
}

TEST(ControllerSession, FreshControllerHasNothingToRedo) {
  ravel::app::Controller c;
  const auto res = c.Redo();
  ASSERT_FALSE(res.has_value());
  EXPECT_EQ(res.error(), ravel::app::CommandError::NothingToRedo);
}

TEST(ControllerSession, SaveWithoutPathReturnsNoPath) {
  ravel::app::Controller c;
  const auto res = c.Save();
  ASSERT_FALSE(res.has_value());
  EXPECT_EQ(res.error(), ravel::app::ControllerError::NoPath);
}

// Текущий контракт доменных проверок ядра — исключения (переход на
// expected зафиксирован в roadmap как направление, не как этот тест).
// Фиксируем поведение как регрессионную границу: ошибка не должна
// проглатываться и не должна портить сессию.
TEST(ControllerSession, AddNodeWithUnknownTypeThrowsThrough) {
  ravel::app::Controller c;
  EXPECT_THROW((void)c.AddNode("n", "missing"), std::invalid_argument);
}

TEST_F(ControllerIoTest, SaveAsWritesFileReadableByStorage) {
  ravel::app::Controller c;
  const auto res = c.SaveAs(good_path_);
  ASSERT_TRUE(res.has_value()) << "error=" << (res ? -1 : static_cast<int>(res.error()));
  std::ifstream in(good_path_, std::ios::binary);
  ASSERT_TRUE(in.good()) << "SaveAs не оставил файла по целевому пути";
  // Атомарность косвенно: файл цел, читается и парсится.
  EXPECT_NO_THROW((void)ravel::JsonStorage::Load(in));
}

TEST_F(ControllerIoTest, OpenThenSaveWithoutExplicitPath) {
  WriteGraphWithOntology(source_path_);
  ravel::app::Controller c;
  ASSERT_TRUE(c.Open(source_path_).has_value());
  // Save после Open использует путь, полученный из Open.
  ASSERT_TRUE(c.Save().has_value());
}

// Регрессия: Save() передаёт в SaveAs ссылку на s_.file_path.value() —
// самоприсваивание внутри (optional уничтожает значение до копирования)
// оставляло {nullptr, non-zero} wstring, падавшую в дебаг-куче при закрытии.
// Повторный Save + разрушение контроллера под отладочной кучей ловит баг.
TEST_F(ControllerIoTest, RepeatedSaveKeepsFilePathValid) {
  WriteGraphWithOntology(source_path_);
  ravel::app::Controller c;
  ASSERT_TRUE(c.Open(source_path_).has_value());
  ASSERT_TRUE(c.Save().has_value());
  ASSERT_TRUE(c.Save().has_value());
  ravel::app::Controller c2;
  ASSERT_TRUE(c2.Open(source_path_).has_value());
  ASSERT_TRUE(c2.Save().has_value());
}

TEST_F(ControllerIoTest, SaveAsToMissingDirectoryReturnsIoError) {
  ravel::app::Controller c;
  const fs::path no_such_dir = fs::temp_directory_path() / "ravel_ctrl_no_such_dir" / "f.json";
  const auto res = c.SaveAs(no_such_dir);
  ASSERT_FALSE(res.has_value());
  EXPECT_EQ(res.error(), ravel::app::ControllerError::IoError);
  std::error_code ec;
  ASSERT_FALSE(fs::exists(no_such_dir, ec));
}

// Протокол команд поверх загруженной сессии: Execute → Undo → Redo →
// Undo, затем дно стека. Open сбрасывает летопись (свежая сессия — один
// undo после одного AddNode, второй — NothingToUndo).
TEST_F(ControllerIoTest, CommandFlowAfterOpen) {
  WriteGraphWithOntology(source_path_);
  ravel::app::Controller c;
  const auto open_res = c.Open(source_path_);
  ASSERT_TRUE(open_res.has_value())
      << "open error=" << (open_res ? -1 : static_cast<int>(open_res.error()));
  ASSERT_TRUE(c.AddNode("n", "nt").has_value());
  ASSERT_TRUE(c.AddRelation(1, 1, "rt").has_value());
  ASSERT_TRUE(c.Delete({}, {1}).has_value());
  ASSERT_TRUE(c.Undo().has_value());  // delete отменён
  ASSERT_TRUE(c.Undo().has_value());  // relation убран
  ASSERT_TRUE(c.Redo().has_value());  // relation восстановлен (restore-путь)
  ASSERT_TRUE(c.Undo().has_value());
  ASSERT_TRUE(c.Undo().has_value());  // AddNode убран
  const auto res = c.Undo();
  ASSERT_FALSE(res.has_value());
  EXPECT_EQ(res.error(), ravel::app::CommandError::NothingToUndo);
}

// ---- read-API (NodeView / NodeTypeNames) ----

// Файл с двумя типами узлов; имена выбраны так, чтобы порядок ключей
// std::map был очевиден: "at" < "nt".
void WriteGraphWithTwoNodeTypes(const fs::path& p) {
  ravel::Graph g;
  g.AddNodeType("nt", "#112233", "rect", "#445566");
  g.AddNodeType("at", "#aabbcc", "oval", "#ddeeff");
  std::ofstream out(p, std::ios::binary);
  ravel::JsonStorage::Save(g.ToSnapshot(), out);
}

TEST(ControllerReadApi, FreshControllerIsEmpty) {
  ravel::app::Controller c;
  EXPECT_TRUE(c.Nodes().empty());
  EXPECT_TRUE(c.NodeTypeNames().empty());
}

TEST_F(ControllerIoTest, NodeTypeNamesFromOntology) {
  WriteGraphWithTwoNodeTypes(good_path_);
  ravel::app::Controller c;
  ASSERT_TRUE(c.Open(good_path_).has_value());
  std::vector<std::string> names = c.NodeTypeNames();
  ASSERT_EQ(names.size(), 2);
  EXPECT_EQ(names[0], "at");
  EXPECT_EQ(names[1], "nt");
}

TEST_F(ControllerIoTest, NodesMapsNodeAndResolvedTypeFields) {
  WriteGraphWithTwoNodeTypes(good_path_);
  ravel::app::Controller c;
  ASSERT_TRUE(c.Open(good_path_).has_value());

  ASSERT_TRUE(c.AddNode("Title", "at", "Desc", 2.5).has_value());
  const std::vector<ravel::app::NodeView> nodes = c.Nodes();
  ASSERT_EQ(nodes.size(), 1);
  const ravel::app::NodeView& v = nodes.front();
  EXPECT_EQ(v.id, 1);
  EXPECT_EQ(v.title, "Title");
  EXPECT_EQ(v.description, "Desc");
  EXPECT_DOUBLE_EQ(v.size, 2.5);
  EXPECT_EQ(v.type, "at");
  EXPECT_EQ(v.color, "#aabbcc");
  EXPECT_EQ(v.shape, "oval");
  EXPECT_EQ(v.frame_color, "#ddeeff");
}

TEST_F(ControllerIoTest, AddNodeWithoutPositionGetsDefaultPlacement) {
  WriteGraphWithOntology(good_path_);
  ravel::app::Controller c;
  ASSERT_TRUE(c.Open(good_path_).has_value());
  ASSERT_TRUE(c.AddNode("n", "nt").has_value());

  const std::vector<ravel::app::NodeView> nodes = c.Nodes();
  ASSERT_EQ(nodes.size(), 1);
  ASSERT_TRUE(nodes.front().position.has_value());
  EXPECT_EQ(nodes.front().position.value(), (ravel::Position{0.0, 0.0}));
}

TEST_F(ControllerIoTest, AddNodeStoresExplicitPosition) {
  WriteGraphWithOntology(good_path_);
  ravel::app::Controller c;
  ASSERT_TRUE(c.Open(good_path_).has_value());
  ASSERT_TRUE(c.AddNode("n", "nt", "", 1, ravel::Position{3.5, -2.0}).has_value());

  const std::vector<ravel::app::NodeView> nodes = c.Nodes();
  ASSERT_EQ(nodes.size(), 1);
  ASSERT_TRUE(nodes.front().position.has_value());
  EXPECT_DOUBLE_EQ(nodes.front().position->x_, 3.5);
  EXPECT_DOUBLE_EQ(nodes.front().position->y_, -2.0);
}

TEST_F(ControllerIoTest, NodesReturnsLiveNodesInAscendingIdOrder) {
  WriteGraphWithOntology(good_path_);
  ravel::app::Controller c;
  ASSERT_TRUE(c.Open(good_path_).has_value());
  ASSERT_TRUE(c.AddNode("A", "nt").has_value());
  ASSERT_TRUE(c.AddNode("B", "nt").has_value());
  ASSERT_TRUE(c.AddNode("C", "nt").has_value());

  const std::vector<ravel::app::NodeView> nodes = c.Nodes();
  ASSERT_EQ(nodes.size(), 3);
  EXPECT_EQ(nodes[0].id, 1);
  EXPECT_EQ(nodes[1].id, 2);
  EXPECT_EQ(nodes[2].id, 3);
}

TEST_F(ControllerIoTest, NodesFollowsDeleteAndUndo) {
  WriteGraphWithOntology(good_path_);
  ravel::app::Controller c;
  ASSERT_TRUE(c.Open(good_path_).has_value());
  ASSERT_TRUE(c.AddNode("A", "nt").has_value());
  ASSERT_TRUE(c.AddNode("B", "nt").has_value());

  ASSERT_TRUE(c.Delete({1}, {}).has_value());
  {
    const std::vector<ravel::app::NodeView> nodes = c.Nodes();
    ASSERT_EQ(nodes.size(), 1);
    EXPECT_EQ(nodes.front().id, 2);
  }
  ASSERT_TRUE(c.Undo().has_value());
  EXPECT_EQ(c.Nodes().size(), 2);
}

TEST_F(ControllerIoTest, MoveNodeUpdatesPositionVisibleViaNodes) {
  WriteGraphWithOntology(good_path_);
  ravel::app::Controller c;
  ASSERT_TRUE(c.Open(good_path_).has_value());
  ASSERT_TRUE(c.AddNode("n", "nt", "", 1, ravel::Position{1.0, 2.0}).has_value());

  ASSERT_TRUE(c.MoveNode(1, ravel::Position{5.0, -3.0}).has_value());
  const std::vector<ravel::app::NodeView> nodes = c.Nodes();
  ASSERT_EQ(nodes.size(), 1);
  ASSERT_TRUE(nodes.front().position.has_value());
  EXPECT_EQ(nodes.front().position.value(), (ravel::Position{5.0, -3.0}));
}

TEST_F(ControllerIoTest, MoveNodeUndoRedoRoundTrip) {
  WriteGraphWithOntology(good_path_);
  ravel::app::Controller c;
  ASSERT_TRUE(c.Open(good_path_).has_value());
  ASSERT_TRUE(c.AddNode("n", "nt", "", 1, ravel::Position{1.0, 2.0}).has_value());

  ASSERT_TRUE(c.MoveNode(1, ravel::Position{5.0, -3.0}).has_value());
  ASSERT_TRUE(c.Undo().has_value());
  {
    const std::vector<ravel::app::NodeView> nodes = c.Nodes();
    ASSERT_TRUE(nodes.front().position.has_value());
    EXPECT_EQ(nodes.front().position.value(), (ravel::Position{1.0, 2.0}));
  }
  ASSERT_TRUE(c.Redo().has_value());
  {
    const std::vector<ravel::app::NodeView> nodes = c.Nodes();
    ASSERT_TRUE(nodes.front().position.has_value());
    EXPECT_EQ(nodes.front().position.value(), (ravel::Position{5.0, -3.0}));
  }
}

TEST_F(ControllerIoTest, RelationsReturnsLiveRelationsInAscendingIdOrder) {
  WriteGraphWithOntology(good_path_);
  ravel::app::Controller c;
  ASSERT_TRUE(c.Open(good_path_).has_value());
  ASSERT_TRUE(c.AddNode("A", "nt").has_value());
  ASSERT_TRUE(c.AddNode("B", "nt").has_value());
  ASSERT_TRUE(c.AddNode("C", "nt").has_value());
  ASSERT_TRUE(c.AddRelation(1, 2, "rt").has_value());
  ASSERT_TRUE(c.AddRelation(2, 3, "rt").has_value());
  ASSERT_TRUE(c.AddRelation(3, 1, "rt").has_value());

  const std::vector<ravel::app::RelationView> relations = c.Relations();
  ASSERT_EQ(relations.size(), 3);
  EXPECT_EQ(relations[0].id, 1);
  EXPECT_EQ(relations[0].from, 1);
  EXPECT_EQ(relations[0].to, 2);
  EXPECT_EQ(relations[1].id, 2);
  EXPECT_EQ(relations[1].from, 2);
  EXPECT_EQ(relations[1].to, 3);
  EXPECT_EQ(relations[2].id, 3);
  EXPECT_EQ(relations[2].from, 3);
  EXPECT_EQ(relations[2].to, 1);
}

TEST_F(ControllerIoTest, RelationsFollowsDeleteAndUndo) {
  WriteGraphWithOntology(good_path_);
  ravel::app::Controller c;
  ASSERT_TRUE(c.Open(good_path_).has_value());
  ASSERT_TRUE(c.AddNode("A", "nt").has_value());
  ASSERT_TRUE(c.AddNode("B", "nt").has_value());
  ASSERT_TRUE(c.AddNode("C", "nt").has_value());
  ASSERT_TRUE(c.AddRelation(1, 2, "rt").has_value());
  ASSERT_TRUE(c.AddRelation(2, 3, "rt").has_value());
  ASSERT_TRUE(c.AddRelation(3, 1, "rt").has_value());

  ASSERT_TRUE(c.Delete({}, {2}).has_value());
  {
    const std::vector<ravel::app::RelationView> relations = c.Relations();
    ASSERT_EQ(relations.size(), 2);
    EXPECT_EQ(relations.front().id, 1);
    EXPECT_EQ(relations.back().id, 3);
  }
  ASSERT_TRUE(c.Undo().has_value());
  EXPECT_EQ(c.Relations().size(), 3);
}

TEST_F(ControllerIoTest, DeleteMixedSelectionIsOneUndoStep) {
  WriteGraphWithOntology(good_path_);
  ravel::app::Controller c;
  ASSERT_TRUE(c.Open(good_path_).has_value());
  ASSERT_TRUE(c.AddNode("A", "nt").has_value());
  ASSERT_TRUE(c.AddNode("B", "nt").has_value());
  ASSERT_TRUE(c.AddNode("C", "nt").has_value());
  ASSERT_TRUE(c.AddRelation(1, 2, "rt").has_value());  // id 1: touches node 2
  ASSERT_TRUE(c.AddRelation(2, 3, "rt").has_value());  // id 2: touches node 2
  ASSERT_TRUE(c.AddRelation(3, 1, "rt").has_value());  // id 3: survives node 2

  // Selection: node 2 (cascade takes edges 1 and 2) + explicit edge 3,
  // whose endpoints survive on their own.
  ASSERT_TRUE(c.Delete({2}, {3}).has_value());
  EXPECT_EQ(c.Nodes().size(), 2);
  EXPECT_EQ(c.Relations().size(), 0);

  // The regression this guards: one Ctrl+Z must restore nodes AND edges —
  // not «nodes first, edges on the second undo».
  ASSERT_TRUE(c.Undo().has_value());
  EXPECT_EQ(c.Nodes().size(), 3);
  EXPECT_EQ(c.Relations().size(), 3);

  ASSERT_TRUE(c.Redo().has_value());
  EXPECT_EQ(c.Nodes().size(), 2);
  EXPECT_EQ(c.Relations().size(), 0);
}

TEST_F(ControllerIoTest, DeleteDeadIdThrowsInvalidArgument) {
  WriteGraphWithOntology(good_path_);
  ravel::app::Controller c;
  ASSERT_TRUE(c.Open(good_path_).has_value());
  ASSERT_TRUE(c.AddNode("A", "nt").has_value());

  // Contract as it stands: core validates and throws, the command does not
  // translate it into CommandError.
  // UI must only send live ids.
  EXPECT_THROW((void)c.Delete({42}, {}), std::invalid_argument);
}

TEST_F(ControllerIoTest, ChangeNodeUpdatesFieldsVisibleViaNodes) {
  WriteGraphWithOntology(good_path_);
  ravel::app::Controller c;
  ASSERT_TRUE(c.Open(good_path_).has_value());
  ASSERT_TRUE(c.AddNode("n", "nt", "", 1).has_value());

  ASSERT_TRUE(c.ChangeNode(1, "renamed", "nt", "details", 2).has_value());
  const std::vector<ravel::app::NodeView> nodes = c.Nodes();
  ASSERT_EQ(nodes.size(), 1);
  EXPECT_EQ(nodes.front().title, "renamed");
  EXPECT_EQ(nodes.front().description, "details");
  EXPECT_EQ(nodes.front().size, 2);
}

// Style fields of NodeView are derived from the node's type, so a type
// change must be observable through the read-API with the new style —
// this is exactly what a canvas resync will rely on.
TEST_F(ControllerIoTest, ChangeNodeTypeChangeResolvesNewStyle) {
  {
    ravel::Graph g;
    g.AddNodeType("nt", "#112233", "rect", "#445566");
    g.AddNodeType("nt2", "#AABBCC", "oval", "#DDEEFF");
    std::ofstream out(good_path_, std::ios::binary);
    ravel::JsonStorage::Save(g.ToSnapshot(), out);
  }
  ravel::app::Controller c;
  ASSERT_TRUE(c.Open(good_path_).has_value());
  ASSERT_TRUE(c.AddNode("n", "nt").has_value());
  EXPECT_EQ(c.Nodes().front().color, "#112233");

  ASSERT_TRUE(c.ChangeNode(1, "n", "nt2", "", 1).has_value());
  const std::vector<ravel::app::NodeView> nodes = c.Nodes();
  ASSERT_EQ(nodes.size(), 1);
  EXPECT_EQ(nodes.front().type, "nt2");
  EXPECT_EQ(nodes.front().color, "#AABBCC");
  EXPECT_EQ(nodes.front().shape, "oval");
  EXPECT_EQ(nodes.front().frame_color, "#DDEEFF");
}

TEST_F(ControllerIoTest, ChangeNodeUndoRedoRoundTrip) {
  WriteGraphWithOntology(good_path_);
  ravel::app::Controller c;
  ASSERT_TRUE(c.Open(good_path_).has_value());
  ASSERT_TRUE(c.AddNode("n", "nt", "old", 1).has_value());

  ASSERT_TRUE(c.ChangeNode(1, "renamed", "nt", "new", 3).has_value());
  ASSERT_TRUE(c.Undo().has_value());
  {
    const std::vector<ravel::app::NodeView> nodes = c.Nodes();
    ASSERT_EQ(nodes.size(), 1);
    EXPECT_EQ(nodes.front().title, "n");
    EXPECT_EQ(nodes.front().description, "old");
    EXPECT_EQ(nodes.front().size, 1);
  }
  ASSERT_TRUE(c.Redo().has_value());
  {
    const std::vector<ravel::app::NodeView> nodes = c.Nodes();
    ASSERT_EQ(nodes.size(), 1);
    EXPECT_EQ(nodes.front().title, "renamed");
    EXPECT_EQ(nodes.front().description, "new");
    EXPECT_EQ(nodes.front().size, 3);
  }
}

// Current contract of domain validation — exceptions (the expected-based
// rewrite is recorded in roadmap as a direction). Regression boundary:
// an invalid Change must throw, not corrupt the session.
TEST_F(ControllerIoTest, ChangeNodeUnknownTypeThrowsThrough) {
  WriteGraphWithOntology(good_path_);
  ravel::app::Controller c;
  ASSERT_TRUE(c.Open(good_path_).has_value());
  ASSERT_TRUE(c.AddNode("n", "nt").has_value());
  EXPECT_THROW((void)c.ChangeNode(1, "n", "missing", "", 1), std::invalid_argument);
  // Failed command must not have touched the node nor been pushed: the only
  // undoable step left is AddNode, and Undo removing the node proves it.
  EXPECT_EQ(c.Nodes().front().type, "nt");
  ASSERT_TRUE(c.Undo().has_value());
  EXPECT_TRUE(c.Nodes().empty());
}

TEST_F(ControllerIoTest, ChangeNodeUnknownIdThrowsThrough) {
  WriteGraphWithOntology(good_path_);
  ravel::app::Controller c;
  ASSERT_TRUE(c.Open(good_path_).has_value());
  EXPECT_THROW((void)c.ChangeNode(42, "n", "nt", "", 1), std::invalid_argument);
}

TEST_F(ControllerIoTest, ChangeRelationUpdatesTypeAndDescriptionVisibleViaRelations) {
  {
    ravel::Graph g;
    g.AddNodeType("nt", "#112233", "rect", "#445566");
    g.AddRelationType("rt", "#112233", "solid", false, true);
    g.AddRelationType("rt2", "#AABBCC", "dashed", true, false);
    std::ofstream out(good_path_, std::ios::binary);
    ravel::JsonStorage::Save(g.ToSnapshot(), out);
  }
  ravel::app::Controller c;
  ASSERT_TRUE(c.Open(good_path_).has_value());
  ASSERT_TRUE(c.AddNode("A", "nt").has_value());
  ASSERT_TRUE(c.AddNode("B", "nt").has_value());
  ASSERT_TRUE(c.AddRelation(1, 2, "rt").has_value());

  ASSERT_TRUE(c.ChangeRelation(1, "rt2", "why").has_value());
  const std::vector<ravel::app::RelationView> relations = c.Relations();
  ASSERT_EQ(relations.size(), 1);
  const ravel::app::RelationView& rel = relations.front();
  EXPECT_EQ(rel.type, "rt2");
  EXPECT_EQ(rel.description, "why");
  EXPECT_EQ(rel.color, "#AABBCC");
  EXPECT_EQ(rel.arrow, "dashed");
  EXPECT_TRUE(rel.symmetric);
  // Endpoints are immutable — ChangeRelation must not touch them.
  EXPECT_EQ(rel.from, 1);
  EXPECT_EQ(rel.to, 2);
}

TEST_F(ControllerIoTest, ChangeRelationUndoRestoresPreviousType) {
  WriteGraphWithOntology(good_path_);
  ravel::app::Controller c;
  ASSERT_TRUE(c.Open(good_path_).has_value());
  ASSERT_TRUE(c.AddNode("A", "nt").has_value());
  ASSERT_TRUE(c.AddNode("B", "nt").has_value());
  ASSERT_TRUE(c.AddRelation(1, 2, "rt").has_value());

  ASSERT_TRUE(c.ChangeRelation(1, "rt", "why").has_value());
  ASSERT_TRUE(c.Undo().has_value());
  const std::vector<ravel::app::RelationView> relations = c.Relations();
  ASSERT_EQ(relations.size(), 1);
  EXPECT_EQ(relations.front().type, "rt");
  EXPECT_EQ(relations.front().description, "");
}

TEST_F(ControllerIoTest, ChangeRelationUnknownTypeThrowsThrough) {
  WriteGraphWithOntology(good_path_);
  ravel::app::Controller c;
  ASSERT_TRUE(c.Open(good_path_).has_value());
  ASSERT_TRUE(c.AddNode("A", "nt").has_value());
  ASSERT_TRUE(c.AddNode("B", "nt").has_value());
  ASSERT_TRUE(c.AddRelation(1, 2, "rt").has_value());
  EXPECT_THROW((void)c.ChangeRelation(1, "missing", ""), std::invalid_argument);
}

}  // namespace
