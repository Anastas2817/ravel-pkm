#ifndef RAVEL_APP_INCLUDE_CONTROLLER_HPP_
#define RAVEL_APP_INCLUDE_CONTROLLER_HPP_

#include "algorithms.hpp"
#include "command.hpp"
#include "storage.hpp"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace ravel::app {

enum class ControllerError : std::uint8_t { NoPath, IoError, NothingToUndo, NothingToRedo };
using ControllerResult = std::expected<void, ControllerError>;

struct NodeView {  // DTO
  ravel::NodeId id;
  std::string title;
  std::string description;
  double size = 0;
  std::optional<ravel::Position> position;

  std::string type;
  std::string color;
  std::string shape;
  std::string frame_color;
};

struct RelationView {
  ravel::RelationId id;
  ravel::NodeId from;
  ravel::NodeId to;
  std::string description;

  std::string type;
  std::string color;
  std::string arrow;
  bool symmetric;
};

class Controller {
 public:
  ControllerResult Open(const std::filesystem::path& p);
  ControllerResult Save();
  // By value on purpose: Save() passes s_.file_path.value(), and a reference
  // parameter would alias into s_ — `s_.file_path = p` at the end would then
  // copy from the value just destroyed (optional destroys before copying),
  // leaving a {nullptr, non-zero} wstring that trips the debug heap at close.
  ControllerResult SaveAs(std::filesystem::path p);

  CommandResult AddNode(std::string title, std::string type, std::string description = "",
                        double size = 1, std::optional<ravel::Position> position = std::nullopt);
  CommandResult AddRelation(ravel::NodeId from, ravel::NodeId to, std::string type,
                            std::string description = "");
  CommandResult Delete(std::vector<ravel::NodeId> nodes, std::vector<ravel::RelationId> rels);
  CommandResult Undo();
  CommandResult Redo();
  CommandResult MoveNode(ravel::NodeId id, ravel::Position to);
  CommandResult ChangeNode(ravel::NodeId id, std::string title, std::string type,
                           std::string description, double size);
  CommandResult ChangeRelation(ravel::RelationId id, std::string type, std::string description);

  std::vector<NodeView> Nodes() const;
  std::optional<NodeView> FindNode(ravel::NodeId id) const;
  std::vector<std::string> NodeTypeNames() const;

  std::vector<RelationView> Relations() const;
  std::optional<RelationView> FindRelation(ravel::RelationId id) const;
  std::vector<std::string> RelationTypeNames() const;

  std::vector<std::vector<RelationId>> CycleHints() const;

 private:
  struct SessionState {
    ravel::Graph g;
    ravel::app::CommandChronicle chronicle;
    std::optional<std::filesystem::path> file_path = std::nullopt;

    std::size_t saved_bookmark_ = 0;
  };

  SessionState s_;
};

}  // namespace ravel::app

#endif  // RAVEL_APP_INCLUDE_CONTROLLER_HPP_
