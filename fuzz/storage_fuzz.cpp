// Fuzz target: JsonStorage::Load + Graph::FromSnapshot.
//
// Валидация снапшота ручная и длинная — именно тот код, который ломают
// malformed-входы. Property-тесты покрывают штатные пути; здесь — мутации.
// Любое исключение — легальный исход (вход отбраковывается), крашем считается
// только то, что ловят ASan/UBSan (выход за границу id − 1, UB-арифметика).

#include "graph.hpp"
#include "storage.hpp"

#include <cstddef>
#include <cstdint>
#include <istream>
#include <sstream>
#include <string>

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size) {
  const std::string input(reinterpret_cast<const char*>(data), size);
  std::istringstream in(input);
  try {
    const ravel::FileDoc doc = ravel::JsonStorage::Load(in);
    const ravel::Graph graph = ravel::Graph::FromSnapshot(doc.snapshot);
    (void)graph;
  } catch (...) {
    // Malformed input is rejected by design — not a crash.
  }
  return 0;
}
