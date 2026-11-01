#ifdef _MSC_VER
#pragma warning(disable : 26495 26439)
#endif

#include "algorithms.hpp"

#include <gtest/gtest.h>

using ravel::CycleHint;
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

  const std::vector<CycleHint> hints = FindCycleHints(g);
  ASSERT_EQ(hints.size(), 1);
  EXPECT_EQ(hints[0].type, "is_a");
  // Порядок обхода: первое ребро выходит из a, замыкающее — последнее.
  EXPECT_EQ(hints[0].edges, (std::vector<RelationId>{ab, bc, ca}));
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

  const std::vector<CycleHint> hints = FindCycleHints(g);
  ASSERT_EQ(hints.size(), 2);
  EXPECT_EQ(hints[0].type, "is_a");
  EXPECT_EQ(hints[1].type, "is_a");
  // Детерминизм: сканирование s по возрастанию id, порядок hint'ов стабилен.
  EXPECT_EQ(hints[0].edges, (std::vector<RelationId>{ab, ba}));
  EXPECT_EQ(hints[1].edges, (std::vector<RelationId>{cd, dc}));
  const std::vector<CycleHint> again = FindCycleHints(g);
  ASSERT_EQ(again.size(), hints.size());
  for (size_t i = 0; i < hints.size(); ++i) {
    EXPECT_EQ(again[i].type, hints[i].type);
    EXPECT_EQ(again[i].edges, hints[i].edges);
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

  const std::vector<CycleHint> hints = FindCycleHints(g);
  // Один и тот же цикл, но параллельные рёбра A->B дают две материализации.
  ASSERT_EQ(hints.size(), 2);
  for (const CycleHint& hint : hints) {
    EXPECT_EQ(hint.type, "is_a");
    ASSERT_EQ(hint.edges.size(), 3);
    EXPECT_EQ(hint.edges[1], bc);
    EXPECT_EQ(hint.edges[2], ca);
  }
  EXPECT_EQ(hints[0].edges[0], ab1);
  EXPECT_EQ(hints[1].edges[0], ab2);
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

  const std::vector<CycleHint> hints = FindCycleHints(g);
  ASSERT_EQ(hints.size(), 1);
  EXPECT_EQ(hints[0].type, "is_a");
  EXPECT_EQ(hints[0].edges, (std::vector<RelationId>{bc, cb}));
}

TEST(FindCycleHints, SelfLoopIsSingleEdgeHint) {
  Graph g;
  g.AddNodeType("model", "#4a90d9", "rect", "#000000");
  g.AddRelationType("is_a", "#3ba895", "solid", false, true);
  const NodeId a = g.AddNode("A", "model");
  const RelationId aa = g.AddRelation(a, a, "is_a");

  const std::vector<CycleHint> hints = FindCycleHints(g);
  ASSERT_EQ(hints.size(), 1);
  EXPECT_EQ(hints[0].type, "is_a");
  EXPECT_EQ(hints[0].edges, (std::vector<RelationId>{aa}));
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
