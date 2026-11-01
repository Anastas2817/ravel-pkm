#include "algorithms.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <ranges>
#include <string>

namespace ravel {
namespace {

enum class Phase : std::uint8_t { Enter, Scan, Exit };
enum class VColor : std::uint8_t { Unvisited, Entered, Exited };

constexpr NodeId kNoParent = std::numeric_limits<NodeId>::max();

struct Frame {
  NodeId v;
  size_t next;
  Phase phase;
};

struct Edge {
  NodeId from;
  NodeId to;
};

// Adjacency of the subgraph induced by one relation type: column `from`
// lists the `to` endpoints of its live relations of that type.
std::vector<std::vector<NodeId>> BuildAdjacency(const Graph& g, const std::string& key) {
  std::vector<std::vector<NodeId>> adj(g.next_id_node());
  NodeId max = 0;
  g.ForEachLiveRelation([&key, &adj, &max](const Relation& rel) {
    if (rel.type() == key) {
      adj[rel.from()].push_back(rel.to());
      max = std::max(max, rel.from());
      max = std::max(max, rel.to());
    }
    return true;
  });
  adj.resize(max + 1);
  return adj;
}

// One tick of the odometer: advance the last position that still has
// options and reset the positions right of it; false = product exhausted.
bool Advance(std::vector<RelationId>& combo, std::vector<size_t>& index,
             const std::vector<std::vector<RelationId>>& choices) {
  for (size_t over = choices.size(); over-- > 0;) {
    if (++index[over] < choices[over].size()) {
      combo[over] = choices[over][index[over]];
      for (size_t i = over + 1; i < choices.size(); ++i) {
        index[i] = 0;
        combo[i] = choices[i][0];
      }
      return true;
    }
  }
  return false;
}

// Resolves every path edge into candidate relation ids and appends one
// CycleHint per combination: parallel relations of the same type
// materialize into separate hints. The product is walked as an odometer:
// start from the first candidate of each edge, then advance until exhausted.
void EmitHints(const Graph& g, const std::string& key, const std::vector<Edge>& path,
               NodeId back_from, NodeId back_to, std::vector<std::vector<RelationId>>& result) {
  std::vector<std::vector<RelationId>> choices;
  choices.reserve(path.size() + 1);
  for (const Edge& e : path) {
    choices.push_back(g.ResolveRelationIds(e.from, e.to, key));
  }
  choices.push_back(g.ResolveRelationIds(back_from, back_to, key));

  std::vector<RelationId> combo(choices.size());
  std::vector<size_t> index(choices.size(), 0);
  for (size_t i = 0; i < choices.size(); ++i) {
    if (choices[i].empty()) {
      return;  // no relation realizes this edge — nothing to materialize
    }
    combo[i] = choices[i][0];
  }
  result.push_back(std::vector<RelationId>{combo});
  while (Advance(combo, index, choices)) {
    result.push_back(std::vector<RelationId>{combo});
  }
}

// Back edge (v -> u) with u on the current DFS stack: reconstruct the chain
// of tree edges from u up to v, reversed into traversal order, and let the
// closing edge go last.
void HandleBackEdge(const Graph& g, const std::string& key, NodeId v, NodeId u,
                    const std::vector<NodeId>& parent,
                    std::vector<std::vector<RelationId>>& result) {
  std::vector<Edge> path;
  for (NodeId cur = v; cur != u; cur = parent[cur]) {
    path.push_back({parent[cur], cur});
  }
  std::reverse(path.begin(), path.end());
  EmitHints(g, key, path, v, u, result);
}

// Iterative DFS over one type-subgraph; back edges become cycle hints.
void ExploreFrom(const Graph& g, const std::string& key,
                 const std::vector<std::vector<NodeId>>& adj, NodeId s, std::vector<VColor>& color,
                 std::vector<NodeId>& parent, std::vector<std::vector<RelationId>>& result) {
  std::vector<Frame> stack;
  stack.reserve(adj.size());
  stack.push_back({s, 0, Phase::Enter});

  while (!stack.empty()) {
    Frame& f = stack.back();
    switch (f.phase) {
      case Phase::Enter:
        color[f.v] = VColor::Entered;
        f.phase = Phase::Scan;
        break;

      case Phase::Scan:
        if (f.next >= adj[f.v].size()) {
          f.phase = Phase::Exit;
          break;
        }
        if (const NodeId u = adj[f.v][f.next++]; color[u] == VColor::Entered) {
          HandleBackEdge(g, key, f.v, u, parent, result);
        } else if (color[u] == VColor::Unvisited) {
          parent[u] = f.v;
          stack.push_back({u, 0, Phase::Enter});
        }
        break;

      case Phase::Exit:
        color[f.v] = VColor::Exited;
        stack.pop_back();
        break;
    }
  }
}

}  // namespace

std::vector<std::vector<RelationId>> FindCycleHints(const Graph& g) {
  std::vector<std::vector<RelationId>> result;
  for (const std::string& key : g.ontology_relation() | std::views::keys) {
    if (g.ResolveRelation(key).symmetric()) {
      continue;  // a cycle among symmetric relations is not an anomaly
    }
    const std::vector<std::vector<NodeId>> adj = BuildAdjacency(g, key);
    const NodeId n = static_cast<NodeId>(adj.size());
    std::vector<VColor> color(n, VColor::Unvisited);
    std::vector<NodeId> parent(n, kNoParent);
    for (NodeId s = 0; s < n; ++s) {
      if (color[s] == VColor::Unvisited) {
        ExploreFrom(g, key, adj, s, color, parent, result);
      }
    }
  }
  return result;
}

}  // namespace ravel
