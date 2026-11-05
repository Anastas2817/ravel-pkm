#ifdef _MSC_VER
#pragma warning(disable : 26495 26439)
#endif

#include "algorithms.hpp"

#include <utility>
#include <vector>

#include <gtest/gtest.h>

using ravel::FindCycleHints;
using ravel::Graph;
using ravel::NodeId;
using ravel::RelationId;

namespace {

// A -> B -> C, одна связь каждого типа: циклов нет.
Graph MakeAcyclic() {
  Graph g;
  g.AddNodeType("model", "#4a90d9", "rect", "#000000");
  g.AddRelationType("is_a", "#3ba895", "solid", false, true);
  const NodeId a = g.AddNode("Ряд", "model");
  const NodeId b = g.AddNode("Числовая последовательность", "model");
  const NodeId c = g.AddNode("Множество", "model");
  g.AddRelation(a, b, "is_a");
  g.AddRelation(b, c, "is_a");
  return g;
}

}  // namespace

TEST(FindCycleHints, AcyclicGraphReturnsEmpty) {
  EXPECT_TRUE(FindCycleHints(MakeAcyclic()).empty());
}

TEST(FindCycleHints, TriangleYieldsSingleHintInTraversalOrder) {
  Graph g;
  g.AddNodeType("model", "#4a90d9", "rect", "#000000");
  g.AddRelationType("is_a", "#3ba895", "solid", false, true);
  const NodeId a = g.AddNode("A", "model");
  const NodeId b = g.AddNode("B", "model");
  const NodeId c = g.AddNode("C", "model");
  const RelationId ab = g.AddRelation(a, b, "is_a");
  const RelationId bc = g.AddRelation(b, c, "is_a");
  const RelationId ca = g.AddRelation(c, a, "is_a");

  const std::vector<std::vector<RelationId>> hints = FindCycleHints(g);
  ASSERT_EQ(hints.size(), 1);
  EXPECT_EQ(g.GetRelation(hints[0][0]).type(), "is_a");
  // Порядок обхода: первое ребро выходит из a, замыкающее — последнее.
  EXPECT_EQ(hints[0], (std::vector<RelationId>{ab, bc, ca}));
}

TEST(FindCycleHints, TwoIndependentCyclesAreSeparateHints) {
  Graph g;
  g.AddNodeType("model", "#4a90d9", "rect", "#000000");
  g.AddRelationType("is_a", "#3ba895", "solid", false, true);
  const NodeId a = g.AddNode("A", "model");
  const NodeId b = g.AddNode("B", "model");
  const NodeId c = g.AddNode("C", "model");
  const NodeId d = g.AddNode("D", "model");
  const RelationId ab = g.AddRelation(a, b, "is_a");
  const RelationId ba = g.AddRelation(b, a, "is_a");
  const RelationId cd = g.AddRelation(c, d, "is_a");
  const RelationId dc = g.AddRelation(d, c, "is_a");

  const std::vector<std::vector<RelationId>> hints = FindCycleHints(g);
  ASSERT_EQ(hints.size(), 2);
  for (const std::vector<RelationId>& hint : hints) {
    EXPECT_EQ(g.GetRelation(hint[0]).type(), "is_a");
  }
  // Детерминизм: сканирование s по возрастанию id, порядок hint'ов стабилен.
  EXPECT_EQ(hints[0], (std::vector<RelationId>{ab, ba}));
  EXPECT_EQ(hints[1], (std::vector<RelationId>{cd, dc}));
  const std::vector<std::vector<RelationId>> again = FindCycleHints(g);
  ASSERT_EQ(again.size(), hints.size());
  for (size_t i = 0; i < hints.size(); ++i) {
    EXPECT_EQ(again[i], hints[i]);
  }
}

TEST(FindCycleHints, ParallelEdgesProduceSeparateHints) {
  Graph g;
  g.AddNodeType("model", "#4a90d9", "rect", "#000000");
  g.AddRelationType("is_a", "#3ba895", "solid", false, true);
  const NodeId a = g.AddNode("A", "model");
  const NodeId b = g.AddNode("B", "model");
  const NodeId c = g.AddNode("C", "model");
  const RelationId ab1 = g.AddRelation(a, b, "is_a");
  const RelationId ab2 = g.AddRelation(a, b, "is_a");
  const RelationId bc = g.AddRelation(b, c, "is_a");
  const RelationId ca = g.AddRelation(c, a, "is_a");

  const std::vector<std::vector<RelationId>> hints = FindCycleHints(g);
  // Один и тот же цикл, но параллельные рёбра A->B дают две материализации.
  ASSERT_EQ(hints.size(), 2);
  for (const std::vector<RelationId>& hint : hints) {
    EXPECT_EQ(g.GetRelation(hint[0]).type(), "is_a");
    ASSERT_EQ(hint.size(), 3);
    EXPECT_EQ(hint[1], bc);
    EXPECT_EQ(hint[2], ca);
  }
  EXPECT_EQ(hints[0][0], ab1);
  EXPECT_EQ(hints[1][0], ab2);
}

TEST(FindCycleHints, CyclesAreDetectedPerRelationType) {
  Graph g;
  g.AddNodeType("model", "#4a90d9", "rect", "#000000");
  g.AddRelationType("is_a", "#3ba895", "solid", false, true);
  g.AddRelationType("part_of", "#c94f4f", "solid", false, true);
  const NodeId a = g.AddNode("A", "model");
  const NodeId b = g.AddNode("B", "model");
  const NodeId c = g.AddNode("C", "model");
  g.AddRelation(a, b, "is_a");
  g.AddRelation(b, a, "part_of");  // сквозное ребро: в каждом подграфе ациклично
  const RelationId bc = g.AddRelation(b, c, "is_a");
  const RelationId cb = g.AddRelation(c, b, "is_a");  // цикл только внутри is_a

  const std::vector<std::vector<RelationId>> hints = FindCycleHints(g);
  ASSERT_EQ(hints.size(), 1);
  EXPECT_EQ(g.GetRelation(hints[0][0]).type(), "is_a");
  EXPECT_EQ(hints[0], (std::vector<RelationId>{bc, cb}));
}

TEST(FindCycleHints, SelfLoopIsSingleEdgeHint) {
  Graph g;
  g.AddNodeType("model", "#4a90d9", "rect", "#000000");
  g.AddRelationType("is_a", "#3ba895", "solid", false, true);
  const NodeId a = g.AddNode("A", "model");
  const RelationId aa = g.AddRelation(a, a, "is_a");

  const std::vector<std::vector<RelationId>> hints = FindCycleHints(g);
  ASSERT_EQ(hints.size(), 1);
  EXPECT_EQ(g.GetRelation(hints[0][0]).type(), "is_a");
  EXPECT_EQ(hints[0], (std::vector<RelationId>{aa}));
}

TEST(FindCycleHints, SymmetricTypesAreExcluded) {
  Graph g;
  g.AddNodeType("model", "#4a90d9", "rect", "#000000");
  g.AddRelationType("isomorphic_to", "#c94f4f", "solid", true, false);
  const NodeId a = g.AddNode("A", "model");
  const NodeId b = g.AddNode("B", "model");
  const NodeId c = g.AddNode("C", "model");
  g.AddRelation(a, b, "isomorphic_to");
  g.AddRelation(b, a, "isomorphic_to");  // двунаправленность в данных
  g.AddRelation(b, c, "isomorphic_to");
  g.AddRelation(c, a, "isomorphic_to");  // треугольник — тоже не цикл

  EXPECT_TRUE(FindCycleHints(g).empty());
}

TEST(GraphResolveRelationIds, LookupValidationAndEmptyMatch) {
  Graph g;
  g.AddNodeType("model", "#4a90d9", "rect", "#000000");
  g.AddRelationType("is_a", "#3ba895", "solid", false, true);
  const NodeId a = g.AddNode("A", "model");
  const NodeId b = g.AddNode("B", "model");
  const RelationId ab = g.AddRelation(a, b, "is_a");

  EXPECT_EQ(g.ResolveRelationIds(a, b, "is_a"), (std::vector<RelationId>{ab}));
  EXPECT_TRUE(g.ResolveRelationIds(b, a, "is_a").empty());

  g.DeleteNodes({b});
  EXPECT_THROW(g.ResolveRelationIds(a, b, "is_a"), std::invalid_argument);
  EXPECT_THROW(g.ResolveRelationIds(b, a, "is_a"), std::invalid_argument);
  EXPECT_THROW(g.ResolveRelationIds(a, a, "unknown"), std::invalid_argument);
}

namespace {

// Граница порога намеренно продублирована из algorithms.cpp (kShadowStart):
// тест закрепляет калибровку, случайная смена константы упадёт здесь.
constexpr std::size_t kShadowStart = 6;

// Звезда: center + spokes листьев, рёбра center -> leaf.
std::pair<Graph, NodeId> MakeStar(std::size_t spokes) {
  Graph g;
  g.AddNodeType("model", "#4a90d9", "rect", "#000000");
  g.AddRelationType("is_a", "#3ba895", "solid", false, true);
  const NodeId center = g.AddNode("center", "model");
  for (std::size_t i = 0; i < spokes; ++i) {
    const NodeId leaf = g.AddNode("leaf", "model");
    g.AddRelation(center, leaf, "is_a");
  }
  return {std::move(g), center};
}

}  // namespace

using ravel::FindHubHints;

// Плотная проекция: hints[id] = перелёт за порог (0 = не хаб); размер =
// next_id_node (слот 0 всегда 0 — id с 1). Итерация по карте не нужна:
// потребитель UI ходит по item'ам и читает свой индекс.
using ravel::FindHubHints;

TEST(FindHubHints, BelowThresholdNoHint) {
  auto [g, center] = MakeStar(kShadowStart - 1);
  EXPECT_EQ(FindHubHints(g)[center], 0);
}

TEST(FindHubHints, AtThresholdYieldsOverflowOne) {
  auto [g, center] = MakeStar(kShadowStart);
  EXPECT_EQ(FindHubHints(g)[center], 1);
}

TEST(FindHubHints, OverflowIsCountedBeyondThreshold) {
  auto [g, center] = MakeStar(kShadowStart + 2);
  // value — «перелёт за порог» для градации тени: 8 связей -> 3.
  EXPECT_EQ(FindHubHints(g)[center], 3);
}

TEST(FindHubHints, LeavesAreNotHubs) {
  auto [g, center] = MakeStar(kShadowStart);
  const std::vector<std::uint8_t> hints = FindHubHints(g);
  ASSERT_EQ(hints.size(), g.next_id_node());
  EXPECT_EQ(hints[center], 1);
  for (std::size_t leaf = 1; leaf < hints.size(); ++leaf) {
    if (leaf != center) {
      EXPECT_EQ(hints[leaf], 0);
    }
  }
}

TEST(FindHubHints, SelfLoopCountsOncePerAdr004) {
  auto [g, center] = MakeStar(kShadowStart - 2);
  g.AddRelation(center, center, "is_a");
  // 4 спицы + петля (= 1 связь по ADR-004) = 5 — тени нет.
  EXPECT_EQ(FindHubHints(g)[center], 0);
  g.AddRelation(center, center, "is_a");
  // 6 — тень с перелётом 1.
  EXPECT_EQ(FindHubHints(g)[center], 1);
}

TEST(FindHubHints, DeletedRelationsDropTheHint) {
  Graph g;
  g.AddNodeType("model", "#4a90d9", "rect", "#000000");
  g.AddRelationType("is_a", "#3ba895", "solid", false, true);
  const NodeId center = g.AddNode("center", "model");
  std::vector<RelationId> rels;
  for (std::size_t i = 0; i < kShadowStart; ++i) {
    const NodeId leaf = g.AddNode("leaf", "model");
    rels.push_back(g.AddRelation(center, leaf, "is_a"));
  }
  ASSERT_EQ(FindHubHints(g)[center], 1);
  // Soft-delete (ADR-002): живые рёбра перестают считаться — тень снимается.
  g.DeleteRelations({rels[0], rels[1]});
  EXPECT_EQ(FindHubHints(g)[center], 0);
}

TEST(FindHubHints, DegreeSaturatesAtUint8Max) {
  // Лимит проекта 10³ связей на узел переполнил бы uint8_t: счётчик
  // насыщается, «больше 255» для градации тени неразличимо (UI клампит).
  Graph g;
  g.AddNodeType("model", "#4a90d9", "rect", "#000000");
  g.AddRelationType("is_a", "#3ba895", "solid", false, true);
  const NodeId center = g.AddNode("center", "model");
  for (int i = 0; i < 260; ++i) {
    const NodeId leaf = g.AddNode("leaf", "model");
    g.AddRelation(center, leaf, "is_a");
  }
  EXPECT_EQ(FindHubHints(g)[center], 255 - kShadowStart + 1);
}
