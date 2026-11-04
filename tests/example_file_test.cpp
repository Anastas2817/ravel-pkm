#include "controller.hpp"

#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

#include <gtest/gtest.h>

namespace {

std::string ReadBytes(const std::filesystem::path& p) {
  std::ifstream in(p, std::ios::binary);
  return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
}

std::filesystem::path ExamplePath() {
  return std::filesystem::path(RAVEL_REPO_ROOT) / "examples" / "example.ravel.json";
}

// Золотой файл: examples/example.ravel.json — эталонный сценарий «граф
// с онтологией и узлами» (регрессионная граница бага пустого снапшота в
// FromSnapshot + ручной сценарий resync UI). Тест держит пример открываемым:
// сломался формат или словарь значений (shape/arrow/цвет) — падает здесь,
// а не в руках юзера.
TEST(ExampleFile, OpensViaController) {
  const std::filesystem::path p = ExamplePath();
  ravel::app::Controller c;
  const auto res = c.Open(p);
  ASSERT_TRUE(res.has_value()) << "error=" << (res.has_value() ? -1 : static_cast<int>(res.error()))
                               << " path=" << p;
}

// Побайтовая стабильность Save: открыть золотой файл, сохранить во временный,
// сравнить байт-к-байту. Детерминизм сериализации (порядок ключей, отступы,
// отсутствие trailing newline, LF-окончания) — контракт, и он закреплён
// золотым файлом: любая правка storage, меняющая байты, падает здесь.
// Окончания строк golden-файла зафиксированы через .gitattributes (-text).
TEST(ExampleFile, ResaveIsByteIdentical) {
  const std::filesystem::path p = ExamplePath();
  ravel::app::Controller c;
  ASSERT_TRUE(c.Open(p).has_value());

  const std::filesystem::path tmp =
      std::filesystem::temp_directory_path() / "ravel_example_resave.ravel.json";
  ASSERT_TRUE(c.SaveAs(tmp).has_value());

  const std::string original = ReadBytes(p);
  const std::string resaved = ReadBytes(tmp);
  std::filesystem::remove(tmp);

  ASSERT_EQ(original.size(), resaved.size())
      << "Save изменил размер: " << original.size() << " -> " << resaved.size();
  const auto mismatch = std::mismatch(original.begin(), original.end(), resaved.begin());
  ASSERT_TRUE(mismatch.first == original.end())
      << "Save недетерминированен: первое расхождение на байте "
      << std::distance(original.begin(), mismatch.first);
}

}  // namespace
