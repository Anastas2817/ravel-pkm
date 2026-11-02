#ifdef _MSC_VER
#pragma warning(disable : 26495 26439)
#endif

#include "storage.hpp"

#include <sstream>

#include <gtest/gtest.h>

using ravel::Graph;
using ravel::GraphSnapshot;
using ravel::JsonStorage;
using ravel::Node;
using ravel::NodeId;
using ravel::NodeType;
using ravel::Relation;
using ravel::RelationId;
using ravel::RelationType;

TEST(StorageLoadSave, RoundTrip) {
  GraphSnapshot src;
  src.ontology_node.emplace("model", NodeType("#002030", "rect", "#a0500b"));
  src.ontology_relation.emplace("reduce_to", RelationType("#003d00", "dashed", false, true));

  src.nodes.push_back(
      Node(1, "Уравнения с разделяющимися переменными", "model", "f(x)dx=g(y)dy", 2));
  src.positions.push_back(ravel::Position{1.0, 2.0});
  src.nodes.push_back(Node(2, "Уравнения, сводящиеся к однородным", "model"));
  src.positions.push_back(std::nullopt);
  src.nodes.push_back(Node(3, "Уравнения Бернулли", "model"));
  src.positions.push_back(std::nullopt);
  src.nodes.push_back(Node(4, "Однородные уравнения", "model"));
  src.positions.push_back(std::nullopt);

  src.relations.push_back(Relation(1, 2, 4, "reduce_to"));
  src.relations.push_back(Relation(1, 3, 4, "reduce_to"));
  src.relations.push_back(Relation(1, 4, 1, "reduce_to"));

  std::stringstream saved;
  JsonStorage::Save({src, {}}, saved);
  GraphSnapshot dest = JsonStorage::Load(saved).snapshot;

  EXPECT_EQ(src.next_id_node, dest.next_id_node);
  EXPECT_EQ(src.next_id_relation, dest.next_id_relation);

  EXPECT_EQ(src.ontology_node, dest.ontology_node);
  EXPECT_EQ(src.ontology_relation, dest.ontology_relation);

  EXPECT_EQ(src.nodes, dest.nodes);
  EXPECT_EQ(src.relations, dest.relations);
  EXPECT_EQ(src.positions, dest.positions);
}

TEST(StorageLoadSave, ValidEmpty) {
  GraphSnapshot src;
  std::stringstream saved;
  JsonStorage::Save({src, {}}, saved);
  GraphSnapshot dest = JsonStorage::Load(saved).snapshot;

  const NodeId kStandardNodeIdValue = 1;      // implementation: R1/R2
  const NodeId kStandardRelationIdValue = 1;  // implementation: R1/R2
  EXPECT_EQ(dest.next_id_node, kStandardNodeIdValue);
  EXPECT_EQ(dest.next_id_relation, kStandardRelationIdValue);
}

TEST(StorageLoadSave, SurvivalOfLoners) {
  GraphSnapshot src;
  src.ontology_node.emplace("model", NodeType("#002030", "rect", "#a0500b"));
  src.ontology_relation.emplace("reduce_to", RelationType("#003d00", "dashed", false, true));
  src.nodes.push_back(
      Node(1, "Уравнения с разделяющимися переменными", "model", "f(x)dx=g(y)dy", 2));
  src.positions.push_back(ravel::Position{2.0, 1.0});
  src.nodes.push_back(Node(2, "Уравнения, сводящиеся к однородным", "model"));
  src.positions.push_back(std::nullopt);
  src.nodes.push_back(Node(3, "Уравнения Бернулли", "model"));
  src.positions.push_back(std::nullopt);
  src.nodes.push_back(Node(4, "Однородные уравнения", "model"));
  src.positions.push_back(std::nullopt);

  src.relations.push_back(Relation(1, 2, 4, "reduce_to"));
  src.relations.push_back(Relation(1, 3, 4, "reduce_to"));
  src.relations.push_back(Relation(1, 4, 1, "reduce_to"));

  src.nodes.push_back(Node(5, "Уравнение в полных дифференциалах", "model"));
  src.positions.push_back(std::nullopt);

  std::stringstream saved;
  JsonStorage::Save({src, {}}, saved);
  GraphSnapshot dest = JsonStorage::Load(saved).snapshot;

  EXPECT_EQ(src.next_id_node, dest.next_id_node);
  EXPECT_EQ(src.next_id_relation, dest.next_id_relation);

  EXPECT_EQ(src.ontology_node, dest.ontology_node);
  EXPECT_EQ(src.ontology_relation, dest.ontology_relation);

  EXPECT_EQ(src.nodes, dest.nodes);
  EXPECT_EQ(src.relations, dest.relations);
  EXPECT_EQ(src.positions, dest.positions);
}

TEST(StorageLoad, WrongFormatVersion) {
  // Тест 1: format_version = 4 (не поддерживается)
  {
    std::stringstream ss{R"({
            "format_version": 4,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types": [],
            "relation_types": [],
            "nodes": [],
            "relations": [],
            "positions": [],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }

  // Тест 2: нет format_version
  {
    std::stringstream ss{R"({
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types": [],
            "relation_types": [],
            "nodes": [],
            "relations": [],
            "positions": [],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }

  // Тест 3: format_version — строка вместо числа
  {
    std::stringstream ss{R"({
            "format_version": "3",
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types": [],
            "relation_types": [],
            "nodes": [],
            "relations": [],
            "positions": [],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }
}

TEST(StorageLoad, AbsenceOfAnObject) {
  // Тест 1: нет next_id_node
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_relation": 4,
            "node_types": [],
            "relation_types": [],
            "nodes": [],
            "relations": [],
            "positions": [],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }

  // Тест 2: нет next_id_relation
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "node_types": [],
            "relation_types": [],
            "nodes": [],
            "relations": [],
            "positions": [],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }

  // Тест 3: нет node_types
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "relation_types" : [],
            "nodes": [],
            "relations": [],
            "positions": [],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }

  // Тест 4: нет relation_types
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [],
            "nodes" : [],
            "relations": [],
            "positions": [],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }

  // Тест 5: нет nodes
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [],
            "relation_types" : [],
            "relations" : [],
            "positions": [],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }

  // Тест 6: нет relations
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [],
            "relation_types" : [],
            "nodes" : [],
            "positions": [],
            "allowed_cycles" : []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }

  // Тест 7: нет positions
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [],
            "relation_types" : [],
            "nodes" : [],
            "relations": [],
            "allowed_cycles" : []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }
}

TEST(StorageLoad, WrongTypeObject) {
  // Тест 1: str next_id_node
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node" : "4",
            "next_id_relation": 4,
            "node_types": [],
            "relation_types": [],
            "nodes": [],
            "relations": [],
            "positions": [],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }

  // Тест 2: array next_id_relation
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation" : [],
            "node_types": [],
            "relation_types": [],
            "nodes": [],
            "relations": [],
            "positions": [],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }

  // Тест 3: int node_types
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : 7,
            "relation_types" : [],
            "nodes": [],
            "relations": [],
            "positions": [],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }

  // Тест 4: str relation_types
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [],
            "relation_types" : "wtf",
            "nodes" : [],
            "relations": [],
            "positions": [],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }

  // Тест 5: str nodes
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [],
            "relation_types" : [],
            "nodes" : "gg",
            "relations" : [],
            "positions": [],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }

  // Тест 6: int relations
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [],
            "relation_types" : [],
            "nodes" : [],
            "relations" : 97097,
            "positions": [],
            "allowed_cycles" : []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }

  // Тест 7: int allowed_cycles
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [],
            "relation_types" : [],
            "nodes" : [],
            "relations" : [],
            "positions": [],
            "allowed_cycles" : 68626
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }

  // Тест 8: int positions
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [],
            "relation_types" : [],
            "nodes" : [],
            "relations" : [],
            "positions": 890,
            "allowed_cycles" : []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }
}

TEST(StorageLoad, StrangeNodeType) {
  // Тест 1.1: имя отсутствует
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [
              {"color": "#4a90d9", "shape": "rect",
              "frame_color": "#2c5f94"}
            ],
            "relation_types" : [],
            "nodes": [],
            "relations": [],
            "positions": [],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }

  // Тест 2.1: color отсутствует
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [
              {"name": "#4a90d9", "shape": "rect",
              "frame_color": "#2c5f94"}
            ],
            "relation_types" : [],
            "nodes": [],
            "relations": [],
            "positions": [],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }

  // Тест 3.1: shape отсутствует
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [
              {"name": "#4a90d9", "color": "rect",
              "frame_color": "#2c5f94"}
            ],
            "relation_types" : [],
            "nodes": [],
            "relations": [],
            "positions": [],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }

  // Тест 4.1: frame_color отсутствует
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [
              {"name": "#4a90d9", "color": "rect",
              "shape": "#2c5f94"}
            ],
            "relation_types" : [],
            "nodes": [],
            "relations": [],
            "positions": [],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }

  // Тест 1.2: имя int
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [
              {"name": 67, "color": "#4a90d9", "shape": "rect",
              "frame_color": "#2c5f94"}
            ],
            "relation_types" : [],
            "nodes": [],
            "relations": [],
            "positions": [],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }

  // Тест 2.2: color array
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [
              {"name": "67", "color": [], "shape": "rect",
              "frame_color": "#2c5f94"}
            ],
            "relation_types" : [],
            "nodes": [],
            "relations": [],
            "positions": [],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }

  // Тест 3.2: shape array
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [
              {"name": "67", "color": "#000000", "shape": [],
              "frame_color": "#2c5f94"}
            ],
            "relation_types" : [],
            "nodes": [],
            "relations": [],
            "positions": [],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }

  // Тест 4.2: frame_color int
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [
              {"name": "67", "color": "#000000", "shape": "[]",
              "frame_color": 111119}
            ],
            "relation_types" : [],
            "nodes": [],
            "relations": [],
            "positions": [],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }
}

TEST(StorageLoad, StrangeRelationType) {
  // Тест 1.1: name отсутствует
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [],
            "relation_types" : [
              {"color": "#4a90d9", "arrow": "solid",
               "symmetric": false, "transitive": true}
            ],
            "nodes": [],
            "relations": [],
            "positions": [],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }
  // Тест 2.1: color отсутствует
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [],
            "relation_types" : [
              {"name": "is_a", "arrow": "solid",
               "symmetric": false, "transitive": true}
            ],
            "nodes": [],
            "relations": [],
            "positions": [],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }
  // Тест 3.1: arrow отсутствует
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [],
            "relation_types" : [
              {"name": "is_a", "color": "#4a90d9",
               "symmetric": false, "transitive": true}
            ],
            "nodes": [],
            "relations": [],
            "positions": [],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }
  // Тест 4.1: symmetric отсутствует
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [],
            "relation_types" : [
              {"name": "is_a", "color": "#4a90d9", "arrow": "solid",
               "transitive": true}
            ],
            "nodes": [],
            "relations": [],
            "positions": [],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }
  // Тест 5.1: transitive отсутствует
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [],
            "relation_types" : [
              {"name": "is_a", "color": "#4a90d9", "arrow": "solid",
               "symmetric": false}
            ],
            "nodes": [],
            "relations": [],
            "positions": [],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }
  // Тест 1.2: name array
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [],
            "relation_types" : [
              {"name": [], "color": "#4a90d9", "arrow": "solid",
               "symmetric": false, "transitive": true}
            ],
            "nodes": [],
            "relations": [],
            "positions": [],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }
  // Тест 2.2: color int
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [],
            "relation_types" : [
              {"name": "is_a", "color": 67, "arrow": "solid",
               "symmetric": false, "transitive": true}
            ],
            "nodes": [],
            "relations": [],
            "positions": [],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }
  // Тест 3.2: arrow array
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [],
            "relation_types" : [
              {"name": "is_a", "color": "#4a90d9", "arrow": [],
               "symmetric": false, "transitive": true}
            ],
            "nodes": [],
            "relations": [],
            "positions": [],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }
  // Тест 4.2: symmetric str
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [],
            "relation_types" : [
              {"name": "is_a", "color": "#4a90d9", "arrow": "solid",
               "symmetric": "false", "transitive": true}
            ],
            "nodes": [],
            "relations": [],
            "positions": [],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }
  // Тест 5.2: transitive int
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [],
            "relation_types" : [
              {"name": "is_a", "color": "#4a90d9", "arrow": "solid",
               "symmetric": false, "transitive": 1}
            ],
            "nodes": [],
            "relations": [],
            "positions": [],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }
}

TEST(StorageLoad, StrangeNode) {
  // Тест 1.1: id отсутствует
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [],
            "relation_types" : [],
            "nodes" : [
              {"title": "Степенная функция", "type": "function",
              "description": "f(x) = x^n"}
            ],
            "relations": [],
            "positions": [ null ],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }
  // Тест 2.1: title отсутствует
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [],
            "relation_types" : [],
            "nodes" : [
              {"id": 1, "type": "function",
              "description": "f(x) = x^n"}
            ],
            "relations": [],
            "positions": [ null ],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }
  // Тест 3.1: type отсутствует
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [],
            "relation_types" : [],
            "nodes" : [
              {"id": 1, "title": "Степенная функция",
              "description": "f(x) = x^n"}
            ],
            "relations": [],
            "positions": [ null ],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }
  // Тест 1.2: id str
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [],
            "relation_types" : [],
            "nodes" : [
              {"id": "max", "title": "Степенная функция", "type": "function",
              "description": "f(x) = x^n"}
            ],
            "relations": [],
            "positions": [ null ],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }
  // Тест 2.2: title int
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [],
            "relation_types" : [],
            "nodes" : [
              {"id": 1, "title": 56, "type": "function",
              "description": "f(x) = x^n"}
            ],
            "relations": [],
            "positions": [ null ],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }
  // Тест 3.2: type array
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [],
            "relation_types" : [],
            "nodes" : [
              {"id": 1, "title": "Степенная функция", "type": [],
              "description": "f(x) = x^n"}
            ],
            "relations": [],
            "positions": [ null ],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }
  // Тест 4.2: description array
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [],
            "relation_types" : [],
            "nodes" : [
              {"id": 1, "title": "Степенная функция", "type": "function",
              "description": []}
            ],
            "relations": [],
            "positions": [ null ],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }
  // Тест 5.2: size str
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [],
            "relation_types" : [],
            "nodes" : [
              {"id": 1, "title": "Степенная функция", "type": "function",
              "description": "f(x) = x^n", "size": "big"}
            ],
            "relations": [],
            "positions": [ null ],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }
}

TEST(StorageLoad, StrangeRelation) {
  // Тест 1.1: id отсутствует
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [],
            "relation_types" : [],
            "nodes" : [],
            "relations" : [
              {"from": 1, "to": 3, "type": "is_defined_on",
               "description": "степенная функция с натуральным показателем задана на всей прямой"}
            ],
            "positions": [],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }
  // Тест 1.2: from отсутствует
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [],
            "relation_types" : [],
            "nodes" : [],
            "relations" : [
              {"id": 5, "to": 3, "type": "is_defined_on",
               "description": "степенная функция с натуральным показателем задана на всей прямой"}
            ],
            "positions": [],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }
  // Тест 3.1: to отсутствует
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [],
            "relation_types" : [],
            "nodes" : [],
            "relations" : [
              {"id": 5, "from": 1, "type": "is_defined_on",
               "description": "степенная функция с натуральным показателем задана на всей прямой"}
            ],
            "positions": [],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }
  // Тест 4.1: type отсутствует
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [],
            "relation_types" : [],
            "nodes" : [],
            "relations" : [
              {"id": 5, "from": 1, "to": 3,
               "description": "степенная функция с натуральным показателем задана на всей прямой"}
            ],
            "positions": [],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }
  // Тест 1.2: id array
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [],
            "relation_types" : [],
            "nodes" : [],
            "relations" : [
              {"id": [], "from": 1, "to": 3, "type": "is_defined_on",
               "description": "степенная функция с натуральным показателем задана на всей прямой"}
            ],
            "positions": [],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }
  // Тест 2.2: from str
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [],
            "relation_types" : [],
            "nodes" : [],
            "relations" : [
              {"id": 5, "from": "функция", "to": 3, "type": "is_defined_on",
               "description": "степенная функция с натуральным показателем задана на всей прямой"}
            ],
            "positions": [],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }
  // Тест 3.2: to str
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [],
            "relation_types" : [],
            "nodes" : [],
            "relations" : [
              {"id": 5, "from": 1, "to": "3", "type": "is_defined_on",
               "description": "степенная функция с натуральным показателем задана на всей прямой"}
            ],
            "positions": [],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }
  // Тест 4.2: type int
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [],
            "relation_types" : [],
            "nodes" : [],
            "relations" : [
              {"id": 5, "from": 1, "to": 3, "type": 567,
               "description": "степенная функция с натуральным показателем задана на всей прямой"}
            ],
            "positions": [],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }
  // Тест 5.2: description array
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 3,
            "next_id_relation": 4,
            "node_types" : [],
            "relation_types" : [],
            "nodes" : [],
            "relations" : [
              {"id": 5, "from": 1, "to": 3, "type": "is_defined_on",
               "description": []}
            ],
            "positions": [],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }
}

TEST(StorageLoad, FailStateStream) {
  std::stringstream ss;
  ss.setstate(std::ios::failbit);
  EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
}

TEST(StorageSave, FailStateStream) {
  std::stringstream ss;
  ss.setstate(std::ios::failbit);
  GraphSnapshot g;
  EXPECT_THROW(JsonStorage::Save({g, {}}, ss), std::runtime_error);
}

TEST(StorageLoad, RepeatedNodeTypeName) {
  // Правило 13/22: повтор имени типа в словаре — битые внешние данные,
  // а не случайность, которую можно проигнорировать.
  std::stringstream ss{R"({
          "format_version": 3,
          "next_id_node": 1,
          "next_id_relation": 1,
          "node_types" : [
            {"name": "model", "color": "#4a90d9", "shape": "rect",
             "frame_color": "#2c5f94"},
            {"name": "model", "color": "#3ba895", "shape": "ellipse",
             "frame_color": "#000000"}
          ],
          "relation_types" : [],
          "nodes": [],
          "relations": [],
          "positions": [],
          "allowed_cycles": []
      })"};
  EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
}

TEST(StorageLoad, RepeatedRelationTypeName) {
  // То же для relation_types.
  std::stringstream ss{R"({
          "format_version": 3,
          "next_id_node": 1,
          "next_id_relation": 1,
          "node_types" : [
            {"name": "model", "color": "#4a90d9", "shape": "rect",
             "frame_color": "#2c5f94"}
          ],
          "relation_types" : [
            {"name": "is_a", "color": "#000000", "arrow": "solid",
             "symmetric": false, "transitive": true},
            {"name": "is_a", "color": "#ffffff", "arrow": "dashed",
             "symmetric": false, "transitive": false}
          ],
          "nodes": [],
          "relations": [],
          "positions": [],
          "allowed_cycles": []
      })"};
  EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
}

TEST(StorageLoad, BrokenJson) {
  // Синтаксический уровень: JSON не разбирается парсером в принципе.
  // Отличие от Strange*-тестов: там вход валиден для парсера и дефект
  // семантический (один, изолированный — правило 22), здесь дефект —
  // сам синтаксис. Все сбои обязаны прилетать как std::runtime_error
  // (json::exception оборачивается в Load), а не проскакивать.

  // Тест 1: незакрытая скобка — документ обрывается посередине
  {
    std::stringstream ss{R"({
            "format_version": 2,
            "next_id_node": 3,
            "next_id_relation": 1,
            "node_types" : [],
            "relation_types" : [],
            "nodes": [)"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }

  // Тест 2: полностью не JSON
  {
    std::stringstream ss{"это не json"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }

  // Тест 3: пустой ввод — парсер падает на пустом потоке
  {
    std::stringstream ss;
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }

  // Тест 4: валидный JSON, но мусор после документа (строгий парсинг)
  {
    std::stringstream ss{R"({"format_version": 2}) это уже лишнее)"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }

  // Тест 5: валидный JSON неверной формы — массив на верхнем уровне
  // вместо объекта (парсер проходит, доступ по ключам бросает type_error)
  {
    std::stringstream ss{"[1, 2, 3]"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }

  // Тест 6: литерал вместо значения — недопустимый токен внутри документа
  {
    std::stringstream ss{R"({
            "format_version": 2,
            "next_id_node": undefined
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }
}

TEST(StorageLoad, StrangePosition) {
  // ADR-003: элемент positions — объект {"x", "y"} или null. Остальное —
  // битые внешние данные (правило 22): каждый дефект изолирован и обязан
  // прилетать как std::runtime_error.
  // (Число вместо объекта: см. бывший StrangeNode Тест 6.2 — его сюда.)

  // Тест 1: x отсутствует
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 2,
            "next_id_relation": 1,
            "node_types" : [],
            "relation_types" : [],
            "nodes" : [
              {"id": 1, "title": "Степенная функция", "type": "function"}
            ],
            "relations": [],
            "positions": [ {"y": 5} ],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }
  // Тест 2: x — строка
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 2,
            "next_id_relation": 1,
            "node_types" : [],
            "relation_types" : [],
            "nodes" : [
              {"id": 1, "title": "Степенная функция", "type": "function"}
            ],
            "relations": [],
            "positions": [ {"x": "широта", "y": 5} ],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }
  // Тест 3: элемент — bool (не объект и не null)
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 2,
            "next_id_relation": 1,
            "node_types" : [],
            "relation_types" : [],
            "nodes" : [
              {"id": 1, "title": "Степенная функция", "type": "function"}
            ],
            "relations": [],
            "positions": [ true ],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }
  // Тест 4: элемент — число (перенесённый бывший StrangeNode 6.2)
  {
    std::stringstream ss{R"({
            "format_version": 3,
            "next_id_node": 2,
            "next_id_relation": 1,
            "node_types" : [],
            "relation_types" : [],
            "nodes" : [
              {"id": 1, "title": "Степенная функция", "type": "function"}
            ],
            "relations": [],
            "positions": [ 78 ],
            "allowed_cycles": []
        })"};
    EXPECT_THROW(JsonStorage::Load(ss), std::runtime_error);
  }
}
