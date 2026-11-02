#include "controller.hpp"

#include <fstream>
#include <ranges>

namespace ravel::app {

ControllerResult Controller::Open(const std::filesystem::path& p) {
  std::ifstream file(p, std::ios::binary);
  if (!file) {
    return std::unexpected(ControllerError::NoPath);
  }

  SessionState next;

  try {
    next.g = ravel::Graph::FromSnapshot(ravel::JsonStorage::Load(file));
    next.chronicle = ravel::app::CommandChronicle{};
    next.file_path = p;
    next.saved_bookmark_ = 0;
  } catch (const std::exception&) {
    return std::unexpected(ControllerError::IoError);
  }

  s_ = std::move(next);
  return {};
}

ControllerResult Controller::Save() {
  if (s_.file_path == std::nullopt) {
    return std::unexpected(ControllerError::NoPath);
  }

  return SaveAs(s_.file_path.value());
}

ControllerResult Controller::SaveAs(std::filesystem::path p) {
  std::filesystem::path temp = p;
  temp += ".tmp";

  std::ofstream file(temp, std::ios::binary);
  if (!file) {
    return std::unexpected(ControllerError::IoError);
  }

  try {
    ravel::JsonStorage::Save(s_.g.ToSnapshot(), file);
    if (!file) {
      throw std::runtime_error("close failed");
    }
  } catch (const std::exception&) {
    file.close();
    std::error_code ec;
    std::filesystem::remove(temp, ec);
    return std::unexpected(ControllerError::IoError);
  }

  file.close();
  std::error_code ec;
  std::filesystem::rename(temp, p, ec);
  if (ec) {
    std::filesystem::remove(temp, ec);
    return std::unexpected(ControllerError::IoError);
  }

  s_.file_path = std::move(p);
  s_.saved_bookmark_ = s_.chronicle.bookmark();
  return {};
}

CommandResult Controller::AddNode(std::string title, std::string type, std::string description,
                                  double size, std::optional<ravel::Position> position) {
  const ravel::app::AddNodeCmd cmd{
      .title = std::move(title),
      .type = std::move(type),
      .description = std::move(description),
      .size = size,
      .initial_position = position.value_or(ravel::Position{0.0, 0.0})};
  return s_.chronicle.Execute(cmd, s_.g);
}

CommandResult Controller::AddRelation(ravel::NodeId from, ravel::NodeId to, std::string type,
                                      std::string description) {
  const ravel::app::AddRelationCmd cmd{
      .from = from, .to = to, .type = std::move(type), .description = std::move(description)};
  return s_.chronicle.Execute(cmd, s_.g);
}

CommandResult Controller::Delete(std::vector<ravel::NodeId> nodes,
                                 std::vector<ravel::RelationId> rels) {
  const ravel::app::DeleteCmd cmd{std::move(nodes), std::move(rels), {}};
  return s_.chronicle.Execute(cmd, s_.g);
}

CommandResult Controller::Undo() {
  return s_.chronicle.Undo(s_.g);
}

CommandResult Controller::Redo() {
  return s_.chronicle.Redo(s_.g);
}

CommandResult Controller::MoveNode(ravel::NodeId id, ravel::Position to) {
  const ravel::app::MoveNodeCmd cmd{.id = id, .to = to};
  return s_.chronicle.Execute(cmd, s_.g);
}

CommandResult Controller::ChangeNode(ravel::NodeId id, std::string title, std::string type,
                                     std::string description, double size) {
  // No-op guard applies only to LIVE nodes: a dead id must fall through to
  // the command and throw std::invalid_argument from core (see the
  // ChangeNodeUnknownIdThrowsThrough contract test).
  const std::optional<NodeView> node = FindNode(id);
  if (node.has_value() && node->title == title && node->type == type &&
      node->description == description && node->size == size) {
    return {};
  }
  const ravel::app::ChangeNodeCmd cmd{.id = id,
                                      .title = std::move(title),
                                      .type = std::move(type),
                                      .description = std::move(description),
                                      .size = size};
  return s_.chronicle.Execute(cmd, s_.g);
}

CommandResult Controller::ChangeRelation(ravel::RelationId id, std::string type,
                                         std::string description) {
  const std::optional<RelationView> rel = FindRelation(id);
  if (rel.has_value() && rel->type == type && rel->description == description) {
    return {};
  }
  const ravel::app::ChangeRelationCmd cmd{
      .id = id, .type = std::move(type), .description = std::move(description)};
  return s_.chronicle.Execute(cmd, s_.g);
}

std::vector<NodeView> Controller::Nodes() const {
  std::vector<NodeView> result;
  s_.g.ForEachLiveNode([this, &result](const Node& node) {
    const ravel::NodeType& t = s_.g.ResolveNode(node.type());
    result.emplace_back(node.id(), node.title(), node.description(), node.size(),
                        s_.g.GetPosition(node.id()), node.type(), t.color(), t.shape(),
                        t.frame_color());
    return true;
  });
  return result;
}

std::optional<NodeView> Controller::FindNode(ravel::NodeId id) const {
  for (const NodeView& node : this->Nodes()) {
    if (node.id == id) {
      return node;
    }
  }
  return std::nullopt;
}

std::vector<std::string> Controller::NodeTypeNames() const {
  std::vector<std::string> result;
  for (const std::string& key : s_.g.ontology_node() | std::views::keys) {
    result.emplace_back(key);
  }
  return result;
}

std::vector<RelationView> Controller::Relations() const {
  std::vector<RelationView> result;
  s_.g.ForEachLiveRelation([this, &result](const Relation& rel) {
    const ravel::RelationType& t = s_.g.ResolveRelation(rel.type());
    result.emplace_back(rel.id(), rel.from(), rel.to(), rel.description(), rel.type(), t.color(),
                        t.arrow(), t.symmetric());
    return true;
  });
  return result;
}

std::optional<RelationView> Controller::FindRelation(ravel::RelationId id) const {
  for (const RelationView& rel : this->Relations()) {
    if (rel.id == id) {
      return rel;
    }
  }
  return std::nullopt;
}

std::vector<std::string> Controller::RelationTypeNames() const {
  std::vector<std::string> result;
  for (const std::string& key : s_.g.ontology_relation() | std::views::keys) {
    result.emplace_back(key);
  }
  return result;
}

std::vector<std::vector<RelationId>> Controller::CycleHints() const {
  return FindCycleHints(s_.g);
}

}  // namespace ravel::app
