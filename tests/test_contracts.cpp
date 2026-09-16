#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <nlohmann/json.hpp>
#include "uran_interfaces/contracts.hpp"
#include "uran_interfaces/generated_catalog.hpp"

int main()
{
  using uran_interfaces::Json;
  using uran_interfaces::OperationPhase;
  const auto root = std::filesystem::path(URAN_INTERFACES_TEST_SCHEMA_DIR);
  uran_interfaces::ContractValidator validator(root);
  assert(validator.schema_names().size() == uran_interfaces::generated::kSchemas.size());
  const auto fixtures = std::filesystem::path(URAN_INTERFACES_TEST_FIXTURE_DIR);
  std::ifstream manifest_stream(fixtures / "manifest.json");
  Json manifest;
  manifest_stream >> manifest;
  for (const auto & item : manifest.at("cases")) {
    std::ifstream document_stream(fixtures / item.at("file").get<std::string>());
    Json document;
    document_stream >> document;
    if (item.contains("pointer")) {
      document = document.at(Json::json_pointer(item.at("pointer").get<std::string>()));
    }
    const auto issues = validator.validate(item.at("schema"), document);
    assert(issues.empty() == item.at("valid").get<bool>());
  }
  Json command = {
    {"schema_version", "1.0"}, {"operation_id", "op-1"},
    {"target", {{"device_id", "robot_001"}, {"component_id", "base"}}},
    {"controller", "field"}, {"sequence", 3}, {"issued_at_ns", 10},
    {"valid_until_ns", 100}, {"payload", {{"kind", "base_velocity"}, {"frame_id", "base_link"},
      {"linear_mps", {{"x", 0.2}, {"y", 0.0}, {"z", 0.0}}},
      {"angular_rps", {{"x", 0.0}, {"y", 0.0}, {"z", 0.1}}}}}
  };
  validator.validate_or_throw("motion/command.schema.json", command);
  Json naked_payload = command.at("payload");
  naked_payload["linear_mps"]["x"] = std::numeric_limits<double>::quiet_NaN();
  assert(!validator.validate("motion/payloads/base-velocity.schema.json", naked_payload).empty());
  Json gps = {{"schema_version", "1.0"},
    {"source", {{"device_id", "robot_001"}, {"component_id", "gps"}}},
    {"timestamp_ns", 123}, {"status", "fix"}, {"latitude_deg", 31.2},
    {"longitude_deg", 121.5}, {"altitude_m", 4.0}};
  validator.validate_or_throw("sensors/gps-fix.schema.json", gps);
  Json invalid = command;
  invalid["valid_until_ns"] = 5;
  assert(!validator.validate("motion/command.schema.json", invalid).empty());
  Json timeline = {{"schema_version", "1.0"}, {"events", Json::array({
    Json{{"schema_version", "1.0"}, {"operation_id", "op-1"},
      {"target", {{"device_id", "robot_001"}, {"component_id", "base"}}},
      {"sequence", 0}, {"phase", "accepted"}, {"timestamp_ns", 1}},
    Json{{"schema_version", "1.0"}, {"operation_id", "op-1"},
      {"target", {{"device_id", "robot_001"}, {"component_id", "base"}}},
      {"sequence", 1}, {"phase", "running"}, {"timestamp_ns", 2}},
    Json{{"schema_version", "1.0"}, {"operation_id", "op-1"},
      {"target", {{"device_id", "robot_001"}, {"component_id", "base"}}},
      {"sequence", 2}, {"phase", "cancelled"}, {"timestamp_ns", 3}}
  })}};
  validator.validate_or_throw("operations/timeline.schema.json", timeline);
  assert(uran_interfaces::can_transition(OperationPhase::running, OperationPhase::cancelled));
  assert(uran_interfaces::terminal(OperationPhase::cancelled));
  std::cout << "uran_interfaces contract tests passed\n";
}
