#ifdef _MSC_VER
#pragma warning(disable : 26495 26819)
#endif

#include "storage.hpp"

#include <algorithm>

#include <nlohmann/json.hpp>

using nlohmann::json;

namespace ravel {

void JsonStorage::Save(const FileDoc& f, std::ostream& out) {
  if (!out) {
    throw std::runtime_error("JsonStorage::Save: output stream is in fail state");
  }
  json j;
  GraphSnapshot g = f.snapshot;
  j["format_version"] = kFormatVersion;
  j["next_id_node"] = g.next_id_node;
  j["next_id_relation"] = g.next_id_relation;

  j["node_types"] = json::array();
  for (const auto& [name, node_type] : g.ontology_node) {
    json type;
    type["name"] = name;
    type["color"] = node_type.color();
    type["shape"] = node_type.shape();
    type["frame_color"] = node_type.frame_color();
    j["node_types"].push_back(type);
  }

  j["relation_types"] = json::array();
  for (const auto& [name, relation_type] : g.ontology_relation) {
    json type;
    type["name"] = name;
    type["color"] = relation_type.color();
    type["arrow"] = relation_type.arrow();
    type["symmetric"] = relation_type.symmetric();
    type["transitive"] = relation_type.transitive();
    j["relation_types"].push_back(type);
  }

  j["nodes"] = json::array();
  for (const auto& node : g.nodes) {
    json obj;
    obj["id"] = node.id();
    obj["title"] = node.title();
    obj["type"] = node.type();
    if (!node.description().empty()) {
      obj["description"] = node.description();
    }
    if (node.size() != 1) {
      obj["size"] = node.size();
    }
    j["nodes"].push_back(obj);
  }

  j["relations"] = json::array();
  for (const auto& relation : g.relations) {
    json obj;
    obj["id"] = relation.id();
    obj["from"] = relation.from();
    obj["to"] = relation.to();
    obj["type"] = relation.type();
    if (!relation.description().empty()) {
      obj["description"] = relation.description();
    }
    j["relations"].push_back(obj);
  }

  j["positions"] = json::array();
  for (const auto& pos : g.positions) {
    if (pos != std::nullopt) {
      json obj;
      obj["x"] = pos->x_;
      obj["y"] = pos->y_;
      j["positions"].push_back(obj);
    } else {
      j["positions"].push_back(nullptr);
    }
  }

  j["allowed_cycles"] = json::array();
  for (const auto& cycle : f.allowed_cycles) {
    j["allowed_cycles"].push_back(cycle);
  }

  out << j.dump(2);

  if (!out) {
    throw std::runtime_error("JsonStorage::Save: failed to write");
  }
}

FileDoc JsonStorage::Load(std::istream& in) {
  if (!in) {
    throw std::runtime_error("JsonStorage::Load: input stream is in fail state");
  }
  try {
    json j = json::parse(in);
    const std::uint64_t format_version = j.at("format_version");
    if (format_version != kFormatVersion) {
      throw std::runtime_error("JsonStorage::Load: wrong format_version");
    }
    GraphSnapshot g;
    g.next_id_node = j.at("next_id_node").get<NodeId>();
    g.next_id_relation = j.at("next_id_relation").get<RelationId>();
    for (const json& el : j.at("node_types")) {
      const std::string name = el.at("name").get<std::string>();
      if (g.ontology_node.find(name) != g.ontology_node.end()) {
        throw std::runtime_error("JsonStorage::Load: repeated NodeTypes");
      }
      g.ontology_node.emplace(
          name, NodeType(el.at("color").get<std::string>(), el.at("shape").get<std::string>(),
                         el.at("frame_color").get<std::string>()));
    }
    for (const json& el : j.at("relation_types")) {
      const std::string name = el.at("name").get<std::string>();
      if (g.ontology_relation.find(name) != g.ontology_relation.end()) {
        throw std::runtime_error("JsonStorage::Load: repeated RelationTypes");
      }
      g.ontology_relation.emplace(
          name, RelationType(el.at("color").get<std::string>(), el.at("arrow").get<std::string>(),
                             el.at("symmetric").get<bool>(), el.at("transitive").get<bool>()));
    }
    for (const json& el : j.at("nodes")) {
      g.nodes.push_back(Node(el.at("id").get<NodeId>(), el.at("title").get<std::string>(),
                             el.at("type").get<std::string>(),
                             el.value("description", std::string{}), el.value("size", 1.0)));
    }
    for (const json& el : j.at("relations")) {
      g.relations.push_back(Relation(el.at("id").get<RelationId>(), el.at("from").get<NodeId>(),
                                     el.at("to").get<NodeId>(), el.at("type").get<std::string>(),
                                     el.value("description", std::string{})));
    }

    for (const json& el : j.at("positions")) {
      if (!el.is_null()) {
        g.positions.push_back(Position(el.at("x").get<double>(), el.at("y").get<double>()));
      } else {
        g.positions.push_back(std::nullopt);
      }
    }

    std::vector<std::vector<RelationId>> allowed_cycles;
    for (const json& el : j.at("allowed_cycles")) {
      allowed_cycles.push_back(el.get<std::vector<RelationId>>());
    }

    if (!in) {
      throw std::runtime_error("JsonStorage::Load: stream error");
    }
    return FileDoc{.snapshot = g, .allowed_cycles = allowed_cycles};
  } catch (const json::exception& e) {
    throw std::runtime_error(std::string("JsonStorage::Load: ") + e.what());
  }
}

}  // namespace ravel
