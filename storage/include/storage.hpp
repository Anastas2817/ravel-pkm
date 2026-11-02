#ifndef RAVEL_STORAGE_INCLUDE_STORAGE_HPP_
#define RAVEL_STORAGE_INCLUDE_STORAGE_HPP_

#include "graph.hpp"

#include <cstdint>
#include <istream>
#include <ostream>
#include <string>
#include <vector>

namespace ravel {

struct FileDoc {
  GraphSnapshot snapshot;
  std::vector<std::vector<RelationId>> allowed_cycles;
};

class JsonStorage {
 public:
  // Syntax and structure only; graph semantics are not validated here
  // (that is Graph::FromSnapshot's job on Load).
  static void Save(const FileDoc& f, std::ostream& out);
  static FileDoc Load(std::istream& in);
  static constexpr std::uint64_t kFormatVersion = 3;
};

}  // namespace ravel

#endif  // RAVEL_STORAGE_INCLUDE_STORAGE_HPP_
