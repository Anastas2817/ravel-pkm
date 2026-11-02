// Property-based тесты: случайные последовательности операций
// (add/delete/move/раскладка/round-trip) с проверкой инвариантов после
// каждого шага и тремя оракулами на всю последовательность.
//
// Детерминизм: собственный PRNG с фиксированными сидами — CI воспроизводит
// любой прогон по номеру сида. Зависимости (rapidcheck и т.п.) намеренно не
// берём: генератор и оракулы — это ~150 строк, владение ими своё.
//
// Оракулы (по нарастанию силы):
//   1. полный undo  — живое состояние равно исходному (онтология, сущности,
//                     позиции). Строго говоря, бит-в-бит нельзя: счётчики
//                     next_id не откатываются (монотонны, ADR-002), а «тихие
//                     трупы» позиций удалённых узлов в памяти остаются —
//                     оба свойства по дизайну (implementation.md R14).
//   2. полный redo  — после Redo до конца граф бит-в-бит равен состоянию
//                     на момент undo (воспроизводимость replay, включая
//                     эффекты санкционного обхода раскладки);
//   3. round-trip   — Save/Load/FromSnapshot на случайных чекпоинтах
//                     сохраняет ToSnapshot целиком, включая next_id.
//                     Контракт storage — снапшотный: «трупы» позиций в файл
//                     не пишутся (R14), поэтому сравнение через SessionEqual
//                     было бы заведомо ложным срабатыванием.
//
// Операция kLayoutPlace мутирует Graph в обход летописи намеренно: первое
// (вычисленное) позиционирование — инициализация, а не действие пользователя
// (implementation.md R14, «политика позиций»), санкционный обход.

#include "command.hpp"
#include "graph.hpp"
#include "storage.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <sstream>
#include <vector>

#include <gtest/gtest.h>

namespace {

// xorshift32: компактный детерминированный PRNG. Сид 0 подменяется на 1
// (для xorshift нулевое состояние вырождено).
class Rng {
 public:
  explicit Rng(std::uint32_t seed) : state_(seed == 0 ? 1 : seed) {
  }

  std::uint32_t Next() {
    state_ ^= state_ << 13;
    state_ ^= state_ >> 17;
    state_ ^= state_ << 5;
    return state_;
  }

  std::size_t Below(std::size_t n) {
    return static_cast<std::size_t>(Next() % n);
  }

  double Coord() {
    return static_cast<double>(Next() % 2001) - 1000.0;
  }

 private:
  std::uint32_t state_;
};

// Инварианты по наблюдаемой части API (без доступа к приватным полям):
// окрестности ⇔ инцидентность (инварианты 2–3), концы живого ребра живы
// (инвариант 4), позиция читается для каждого живого узла (наблюдаемая
// часть инварианта 8).
void CheckInvariants(const ravel::Graph& g) {
  g.ForEachLiveNode([&g](const ravel::Node& node) {
    for (ravel::RelationId rid : g.GetIncidentRelations(node.id())) {
      EXPECT_TRUE(g.HasRelation(rid));
      const ravel::Relation& relation = g.GetRelation(rid);
      EXPECT_TRUE(relation.from() == node.id() || relation.to() == node.id())
          << "окрестность содержит неинцидентное или мёртвое ребро";
    }
    EXPECT_NO_THROW((void)g.GetPosition(node.id()));
    return true;
  });
  g.ForEachLiveRelation([&g](const ravel::Relation& relation) {
    EXPECT_TRUE(g.HasNode(relation.from()));
    EXPECT_TRUE(g.HasNode(relation.to()));
    const auto from_incident = g.GetIncidentRelations(relation.from());
    const auto to_incident = g.GetIncidentRelations(relation.to());
    EXPECT_NE(std::find(from_incident.begin(), from_incident.end(), relation.id()),
              from_incident.end())
        << "from не знает о живом ребре";
    EXPECT_NE(std::find(to_incident.begin(), to_incident.end(), relation.id()), to_incident.end())
        << "to не знает о живом ребре";
    return true;
  });
}

// Сравнение снапшотов поля в поле. next_id сравнивается отдельно: round-trip
// storage обязан их сохранить, а вот undo — нет (счётчики монотонны).
bool SnapshotsEqual(const ravel::GraphSnapshot& a, const ravel::GraphSnapshot& b,
                    bool with_next_id) {
  if (a.ontology_node != b.ontology_node || a.ontology_relation != b.ontology_relation ||
      a.nodes != b.nodes || a.relations != b.relations || a.positions != b.positions) {
    return false;
  }
  return !with_next_id ||
         (a.next_id_node == b.next_id_node && a.next_id_relation == b.next_id_relation);
}

void RoundTrip(const ravel::Graph& g) {
  std::stringstream stream;
  ravel::JsonStorage::Save({g.ToSnapshot(), {}}, stream);
  const ravel::FileDoc doc = ravel::JsonStorage::Load(stream);
  const ravel::Graph restored = ravel::Graph::FromSnapshot(doc.snapshot);
  EXPECT_TRUE(SnapshotsEqual(restored.ToSnapshot(), g.ToSnapshot(), /*with_next_id=*/true))
      << "round-trip разошёлся с оригиналом";
  EXPECT_TRUE(doc.allowed_cycles.empty());
}

void RunFuzz(std::uint32_t seed) {
  Rng rng(seed);
  ravel::Graph g;
  g.AddNodeType("nt", "#112233", "rect", "#445566");
  g.AddRelationType("rt", "#112233", "solid", false, true);
  const ravel::Graph initial = g;  // оракул полного undo: конечная цель отката

  ravel::app::CommandChronicle chronicle;
  std::vector<ravel::NodeId> created_nodes;
  std::vector<ravel::RelationId> created_relations;
  std::vector<ravel::NodeId> positioned;
  std::size_t executed = 0;  // число команд в летописи (redo-граница)

  auto live_nodes = [&g, &created_nodes]() {
    std::vector<ravel::NodeId> live;
    for (ravel::NodeId id : created_nodes) {
      if (g.HasNode(id)) {
        live.push_back(id);
      }
    }
    return live;
  };
  auto live_relations = [&g, &created_relations]() {
    std::vector<ravel::RelationId> live;
    for (ravel::RelationId id : created_relations) {
      if (g.HasRelation(id)) {
        live.push_back(id);
      }
    }
    return live;
  };

  constexpr int kOps = 150;
  for (int step = 0; step < kOps; ++step) {
    const std::size_t dice = rng.Below(100);
    if (dice < 25) {
      ravel::app::AddNodeCmd cmd{.title = "n", .type = "nt", .description = ""};
      ASSERT_TRUE(chronicle.Execute(cmd, g).has_value()) << "seed " << seed << " op " << step;
      // Execute takes the command by value: the caller's copy never receives
      // obtained_id. The created id is still recoverable — ids are never
      // reused (ADR-002), so it is the last allocated one.
      created_nodes.push_back(g.next_id_node() - 1);
      ++executed;
    } else if (dice < 50) {
      std::vector<ravel::NodeId> live = live_nodes();
      if (live.size() < 2) {
        continue;
      }
      // Loops (from == to) are legal in the current model — the from != to
      // invariant was deliberately left out — so independent picks are fine.
      const ravel::NodeId from = live[rng.Below(live.size())];
      const ravel::NodeId to = live[rng.Below(live.size())];
      ravel::app::AddRelationCmd cmd{.from = from, .to = to, .type = "rt", .description = ""};
      ASSERT_TRUE(chronicle.Execute(cmd, g).has_value()) << "seed " << seed << " op " << step;
      created_relations.push_back(g.next_id_relation() - 1);
      ++executed;
    } else if (dice < 65) {
      std::vector<ravel::NodeId> live = live_nodes();
      if (live.empty()) {
        continue;
      }
      std::vector<ravel::NodeId> victims;
      const std::size_t count = 1 + rng.Below(3);
      for (std::size_t k = 0; k < count && !live.empty(); ++k) {
        const std::size_t index = rng.Below(live.size());
        victims.push_back(live[index]);
        live.erase(live.begin() + static_cast<std::ptrdiff_t>(index));
      }
      ravel::app::DeleteCmd cmd{victims, {}, {}};
      ASSERT_TRUE(chronicle.Execute(cmd, g).has_value()) << "seed " << seed << " op " << step;
      ++executed;
    } else if (dice < 75) {
      std::vector<ravel::RelationId> live = live_relations();
      if (live.empty()) {
        continue;
      }
      std::vector<ravel::RelationId> victims;
      const std::size_t count = 1 + rng.Below(3);
      for (std::size_t k = 0; k < count && !live.empty(); ++k) {
        const std::size_t index = rng.Below(live.size());
        victims.push_back(live[index]);
        live.erase(live.begin() + static_cast<std::ptrdiff_t>(index));
      }
      ravel::app::DeleteCmd cmd{{}, victims, {}};
      ASSERT_TRUE(chronicle.Execute(cmd, g).has_value()) << "seed " << seed << " op " << step;
      ++executed;
    } else if (dice < 90) {
      if (positioned.empty()) {
        continue;
      }
      const ravel::NodeId id = positioned[rng.Below(positioned.size())];
      if (!g.HasNode(id)) {
        continue;  // узел умер — запись в positioned устарела, это нормально
      }
      ravel::app::MoveNodeCmd cmd{id, {0.0, 0.0}, {rng.Coord(), rng.Coord()}};
      ASSERT_TRUE(chronicle.Execute(cmd, g).has_value()) << "seed " << seed << " op " << step;
      ++executed;
    } else if (dice < 95) {
      // kLayoutPlace: первое позиционирование мимо летописи (см. шапку
      // файла). Как и настоящая раскладка, заполняет ТОЛЬКО незанятые
      // позиции: повторная раскладка размещённого узла вне контракта R14
      // (и ломала бы from у MoveNodeCmd при undo — см. обсуждение).
      const std::vector<ravel::NodeId> live = live_nodes();
      std::vector<ravel::NodeId> candidates;
      for (ravel::NodeId id : live) {
        if (!g.GetPosition(id).has_value()) {
          candidates.push_back(id);
        }
      }
      if (candidates.empty()) {
        continue;
      }
      const ravel::NodeId id = candidates[rng.Below(candidates.size())];
      g.MoveNode(id, {rng.Coord(), rng.Coord()});
      positioned.push_back(id);
    } else {
      RoundTrip(g);
    }
    CheckInvariants(g);
  }

  const ravel::Graph before_undo = g;  // оракул полного redo

  while (chronicle.bookmark() > 0) {
    ASSERT_TRUE(chronicle.Undo(g).has_value()) << "undo seed " << seed;
    CheckInvariants(g);
  }
  EXPECT_TRUE(SnapshotsEqual(g.ToSnapshot(), initial.ToSnapshot(), /*with_next_id=*/false))
      << "полный undo не вернул исходное живое состояние, seed " << seed;

  while (chronicle.bookmark() < executed) {
    ASSERT_TRUE(chronicle.Redo(g).has_value()) << "redo seed " << seed;
    CheckInvariants(g);
  }
  EXPECT_TRUE(g.SessionEqual(before_undo)) << "полный redo не вернул состояние, seed " << seed;

  RoundTrip(g);
}

TEST(GraphProperty, RandomSequencesFullUndoRedoRoundTrip) {
  for (std::uint32_t seed = 1; seed <= 25; ++seed) {
    RunFuzz(seed);
  }
}

}  // namespace
