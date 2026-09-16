#pragma once

#include <cstdint>
#include <filesystem>
#include <map>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace uran_interfaces
{
using Json = nlohmann::json;

enum class OperationPhase
{
  accepted,
  running,
  succeeded,
  failed,
  cancelled,
  rejected
};

const char * to_string(OperationPhase phase) noexcept;
OperationPhase operation_phase_from_string(const std::string & value);
bool terminal(OperationPhase phase) noexcept;
bool can_transition(OperationPhase previous, OperationPhase current) noexcept;

struct ComponentTarget
{
  std::string device_id;
  std::string component_id;
  std::string instance_id;
  std::string capability_id;

  Json to_json() const;
  static ComponentTarget from_json(const Json & document);
  bool operator==(const ComponentTarget & other) const noexcept;
};

struct OperationEvent
{
  std::string operation_id;
  ComponentTarget target;
  std::uint64_t sequence{0};
  OperationPhase phase{OperationPhase::accepted};
  std::uint64_t timestamp_ns{0};
  double progress{0.0};
  bool has_progress{false};
  bool cancellable{false};
  std::string code;
  std::string message;
  Json result;

  Json to_json() const;
  static OperationEvent from_json(const Json & document);
};

struct ValidationIssue
{
  std::string path;
  std::string message;
};

class ContractValidator
{
public:
  explicit ContractValidator(std::filesystem::path schema_root);
  std::vector<std::string> schema_names() const;
  std::vector<ValidationIssue> validate(
    const std::string & schema_name, const Json & document) const;
  void validate_or_throw(const std::string & schema_name, const Json & document) const;

private:
  std::filesystem::path root_;
  std::map<std::string, std::filesystem::path> schemas_by_id_;
};

std::filesystem::path default_schema_directory();
}  // namespace uran_interfaces
