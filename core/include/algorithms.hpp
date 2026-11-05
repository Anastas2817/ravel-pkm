#ifndef RAVEL_CORE_INCLUDE_ALGORITHMS_HPP_
#define RAVEL_CORE_INCLUDE_ALGORITHMS_HPP_

#include "graph.hpp"

#include <vector>
#include <map>

namespace ravel {

std::vector<std::vector<RelationId>> FindCycleHints(const Graph& g);
std::map<NodeId, std::size_t> FindHubHints(const Graph& g);

}  // namespace ravel

#endif  // RAVEL_CORE_INCLUDE_ALGORITHMS_HPP_
