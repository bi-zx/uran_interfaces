#include "uran_interfaces/contracts.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <set>
#include <sstream>
#include <stdexcept>
#if defined(__unix__) || defined(__APPLE__)
#include <dlfcn.h>
#endif

namespace uran_interfaces
{
namespace
{
using Issues = std::vector<ValidationIssue>;
using Catalog = std::map<std::string, uran_interfaces_fs::path>;

Json read_json(const uran_interfaces_fs::path & path)
{
  std::ifstream stream(path);
  if (!stream) {
    throw std::runtime_error("cannot open schema: " + path.string());
  }
  Json result;
  stream >> result;
  return result;
}

bool has_type(const Json & value, const std::string & type)
{
  if (type == "object") {return value.is_object();}
  if (type == "array") {return value.is_array();}
  if (type == "string") {return value.is_string();}
  if (type == "boolean") {return value.is_boolean();}
  if (type == "integer") {return value.is_number_integer() || value.is_number_unsigned();}
  if (type == "number") {return value.is_number() && !value.is_boolean();}
  if (type == "null") {return value.is_null();}
  return false;
}

void add(Issues & issues, const std::string & path, const std::string & message)
{
  issues.push_back({path.empty() ? "$" : path, message});
}

void reject_non_finite(const Json & value, const std::string & path, Issues & issues)
{
  if (value.is_number_float() && !std::isfinite(value.get<double>())) {
    add(issues, path, "number must be finite");
  } else if (value.is_object()) {
    for (const auto & item : value.items()) {
      reject_non_finite(item.value(), path + "/" + item.key(), issues);
    }
  } else if (value.is_array()) {
    for (std::size_t index = 0; index < value.size(); ++index) {
      reject_non_finite(value.at(index), path + "/" + std::to_string(index), issues);
    }
  }
}

void validate_node(const Json & schema, const Json & value,
  const uran_interfaces_fs::path & schema_path, const Catalog & catalog,
  const std::string & path, Issues & issues)
{
  if (schema.contains("$ref")) {
    const auto reference = schema.at("$ref").get<std::string>();
    if (reference.empty() || reference.front() == '#') {
      add(issues, path, "local schema references are not supported by this contract version");
      return;
    }
    auto found = catalog.find(reference);
    // GCC 7's experimental filesystem lacks lexically_normal(); schema
    // references in this catalog are already normalized relative paths.
    const auto target = found == catalog.end() ?
      (schema_path.parent_path() / reference) : found->second;
    validate_node(read_json(target), value, target, catalog, path, issues);
    return;
  }
  if (schema.contains("const") && value != schema.at("const")) {
    add(issues, path, "value does not match const");
  }
  if (schema.contains("enum")) {
    const auto & choices = schema.at("enum");
    if (std::find(choices.begin(), choices.end(), value) == choices.end()) {
      add(issues, path, "value is not in enum");
    }
  }
  if (schema.contains("type")) {
    const auto expected = schema.at("type").get<std::string>();
    if (!has_type(value, expected)) {
      add(issues, path, "expected " + expected);
      return;
    }
  }
  if (schema.contains("oneOf")) {
    std::size_t matches = 0;
    for (const auto & candidate : schema.at("oneOf")) {
      Issues candidate_issues;
      validate_node(candidate, value, schema_path, catalog, path, candidate_issues);
      if (candidate_issues.empty()) {++matches;}
    }
    if (matches != 1) {add(issues, path, "must match exactly one allowed payload");}
  }
  if (value.is_object()) {
    if (schema.contains("required")) {
      for (const auto & name : schema.at("required")) {
        const auto key = name.get<std::string>();
        if (!value.contains(key)) {add(issues, path + "/" + key, "required property is missing");}
      }
    }
    const auto properties = schema.value("properties", Json::object());
    for (const auto & item : value.items()) {
      if (properties.contains(item.key())) {
        validate_node(properties.at(item.key()), item.value(), schema_path, catalog,
          path + "/" + item.key(), issues);
      } else if (schema.value("additionalProperties", true) == false) {
        add(issues, path + "/" + item.key(), "additional property is not allowed");
      }
    }
  }
  if (value.is_array()) {
    if (schema.contains("minItems") && value.size() < schema.at("minItems").get<std::size_t>()) {
      add(issues, path, "array has too few items");
    }
    if (schema.contains("maxItems") && value.size() > schema.at("maxItems").get<std::size_t>()) {
      add(issues, path, "array has too many items");
    }
    if (schema.value("uniqueItems", false)) {
      std::set<std::string> seen;
      for (const auto & item : value) {
        if (!seen.insert(item.dump()).second) {add(issues, path, "array items must be unique"); break;}
      }
    }
    if (schema.contains("items")) {
      for (std::size_t index = 0; index < value.size(); ++index) {
        validate_node(schema.at("items"), value.at(index), schema_path, catalog,
          path + "/" + std::to_string(index), issues);
      }
    }
  }
  if (value.is_string() && schema.contains("minLength") &&
    value.get_ref<const std::string &>().size() < schema.at("minLength").get<std::size_t>()) {
    add(issues, path, "string is too short");
  }
  if (value.is_number() && !value.is_boolean()) {
    const long double number = value.get<long double>();
    if (!std::isfinite(number)) {add(issues, path, "number must be finite");}
    if (schema.contains("minimum") && number < schema.at("minimum").get<long double>()) {
      add(issues, path, "number is below minimum");
    }
    if (schema.contains("maximum") && number > schema.at("maximum").get<long double>()) {
      add(issues, path, "number is above maximum");
    }
  }
}

void validate_semantics(const std::string & name, const Json & document, Issues & issues)
{
  if (name == "motion/command.schema.json") {
    if (document.contains("issued_at_ns") && document.contains("valid_until_ns") &&
      document.at("valid_until_ns").get<std::uint64_t>() <= document.at("issued_at_ns").get<std::uint64_t>()) {
      add(issues, "/valid_until_ns", "must be later than issued_at_ns");
    }
  } else if (name == "components/control-lease.schema.json") {
    if (document.contains("acquired_at_ns") && document.contains("expires_at_ns") &&
      document.at("expires_at_ns").get<std::uint64_t>() <= document.at("acquired_at_ns").get<std::uint64_t>()) {
      add(issues, "/expires_at_ns", "must be later than acquired_at_ns");
    }
  } else if (name == "operations/timeline.schema.json" && document.contains("events") &&
    document.at("events").is_array() && !document.at("events").empty()) {
    const auto & events = document.at("events");
    const auto operation = events.front().value("operation_id", "");
    const auto target = events.front().value("target", Json::object());
    std::uint64_t sequence = 0;
    bool first = true;
    OperationPhase previous = OperationPhase::accepted;
    for (const auto & event : events) {
      if (event.value("operation_id", "") != operation || event.value("target", Json::object()) != target) {
        add(issues, "/events", "all events must identify the same operation and target");
        break;
      }
      const auto current_sequence = event.value("sequence", std::uint64_t{0});
      if (!first && current_sequence <= sequence) {add(issues, "/events", "sequence must increase");}
      try {
        const auto current = operation_phase_from_string(event.value("status", ""));
        if (first && current != OperationPhase::accepted && current != OperationPhase::rejected) {
          add(issues, "/events/0/status", "timeline must begin with accepted or rejected");
        } else if (!first && !can_transition(previous, current)) {
          add(issues, "/events", "invalid operation phase transition");
        }
        previous = current;
      } catch (const std::exception &) {}
      sequence = current_sequence;
      first = false;
    }
  }
}
}  // namespace

const char * to_string(OperationPhase phase) noexcept
{
  switch (phase) {
    case OperationPhase::accepted: return "accepted";
    case OperationPhase::running: return "running";
    case OperationPhase::succeeded: return "succeeded";
    case OperationPhase::failed: return "failed";
    case OperationPhase::cancelled: return "cancelled";
    case OperationPhase::rejected: return "rejected";
  }
  return "failed";
}

OperationPhase operation_phase_from_string(const std::string & value)
{
  if (value == "accepted") {return OperationPhase::accepted;}
  if (value == "running") {return OperationPhase::running;}
  if (value == "succeeded") {return OperationPhase::succeeded;}
  if (value == "failed") {return OperationPhase::failed;}
  if (value == "cancelled") {return OperationPhase::cancelled;}
  if (value == "rejected") {return OperationPhase::rejected;}
  throw std::invalid_argument("unknown operation phase: " + value);
}

bool terminal(OperationPhase phase) noexcept
{
  return phase == OperationPhase::succeeded || phase == OperationPhase::failed ||
    phase == OperationPhase::cancelled || phase == OperationPhase::rejected;
}

bool can_transition(OperationPhase previous, OperationPhase current) noexcept
{
  if (terminal(previous) || current == OperationPhase::accepted || current == OperationPhase::rejected) {
    return false;
  }
  return current == OperationPhase::running || terminal(current);
}

Json ComponentTarget::to_json() const
{
  Json result{{"device_id", device_id}, {"component_id", component_id}};
  if (!instance_id.empty()) {result["instance_id"] = instance_id;}
  if (!capability_id.empty()) {result["capability_id"] = capability_id;}
  return result;
}

ComponentTarget ComponentTarget::from_json(const Json & document)
{
  return {document.at("device_id"), document.at("component_id"),
    document.value("instance_id", ""), document.value("capability_id", "")};
}

bool ComponentTarget::operator==(const ComponentTarget & other) const noexcept
{
  return device_id == other.device_id && component_id == other.component_id &&
    instance_id == other.instance_id && capability_id == other.capability_id;
}

Json OperationEvent::to_json() const
{
  Json document{{"schema_version", "1.0"}, {"operation_id", operation_id},
    {"target", target.to_json()}, {"sequence", sequence}, {"status", uran_interfaces::to_string(phase)},
    {"timestamp_ns", timestamp_ns}, {"cancellable", cancellable}};
  if (has_progress) {document["progress"] = progress;}
  if (!code.empty()) {document["code"] = code;}
  if (!message.empty()) {document["message"] = message;}
  if (!result.is_null()) {document["result"] = result;}
  return document;
}

OperationEvent OperationEvent::from_json(const Json & document)
{
  OperationEvent result;
  result.operation_id = document.at("operation_id");
  result.target = ComponentTarget::from_json(document.at("target"));
  result.sequence = document.at("sequence");
  result.phase = operation_phase_from_string(document.at("status"));
  result.timestamp_ns = document.at("timestamp_ns");
  result.has_progress = document.contains("progress");
  result.progress = document.value("progress", 0.0);
  result.cancellable = document.value("cancellable", false);
  result.code = document.value("code", "");
  result.message = document.value("message", "");
  result.result = document.value("result", Json());
  return result;
}

ContractValidator::ContractValidator(uran_interfaces_fs::path root) : root_(std::move(root))
{
  if (!uran_interfaces_fs::is_directory(root_)) {
    throw std::invalid_argument("schema root is not a directory: " + root_.string());
  }
  for (const auto & entry : uran_interfaces_fs::recursive_directory_iterator(root_)) {
    if (!uran_interfaces_fs::is_regular_file(entry.path()) ||
        entry.path().filename().string().find(".schema.json") == std::string::npos) {
      continue;
    }
    const auto schema = read_json(entry.path());
    if (!schema.contains("$id") || !schema.at("$id").is_string()) {
      throw std::invalid_argument("schema has no $id: " + entry.path().string());
    }
    if (!schemas_by_id_.emplace(schema.at("$id").get<std::string>(), entry.path()).second) {
      throw std::invalid_argument("duplicate schema $id: " + schema.at("$id").get<std::string>());
    }
  }
}

std::vector<std::string> ContractValidator::schema_names() const
{
  std::vector<std::string> names;
  for (const auto & entry : uran_interfaces_fs::recursive_directory_iterator(root_)) {
    if (uran_interfaces_fs::is_regular_file(entry.path()) &&
        entry.path().filename().string().find(".schema.json") != std::string::npos) {
      const auto root = root_.generic_string();
      const auto full = entry.path().generic_string();
      names.push_back(full.compare(0, root.size(), root) == 0 && full.size() > root.size() &&
          full[root.size()] == '/' ? full.substr(root.size() + 1) : full);
    }
  }
  std::sort(names.begin(), names.end());
  return names;
}

std::vector<ValidationIssue> ContractValidator::validate(
  const std::string & schema_name, const Json & document) const
{
  if (schema_name.empty() || schema_name.front() == '/' || schema_name.find("..") != std::string::npos) {
    throw std::invalid_argument("invalid schema name: " + schema_name);
  }
  const auto path = root_ / schema_name;
  Issues issues;
  reject_non_finite(document, "", issues);
  if (!issues.empty()) {return issues;}
  validate_node(read_json(path), document, path, schemas_by_id_, "", issues);
  if (issues.empty()) {validate_semantics(schema_name, document, issues);}
  return issues;
}

void ContractValidator::validate_or_throw(const std::string & schema_name, const Json & document) const
{
  const auto issues = validate(schema_name, document);
  if (!issues.empty()) {throw std::invalid_argument(issues.front().path + ": " + issues.front().message);}
}

uran_interfaces_fs::path default_schema_directory()
{
  if (const char * configured = std::getenv("URAN_INTERFACES_SCHEMA_DIR")) {return configured;}
#if defined(__unix__) || defined(__APPLE__)
  Dl_info information{};
  if (dladdr(reinterpret_cast<const void *>(&default_schema_directory), &information) != 0 &&
    information.dli_fname != nullptr) {
    const auto library = uran_interfaces_fs::canonical(information.dli_fname);
    const auto candidate = library.parent_path().parent_path() / "share/uran_interfaces/schemas";
    if (uran_interfaces_fs::is_directory(candidate)) {return candidate;}
  }
#endif
  throw std::runtime_error(
          "cannot locate installed URAN schemas; set URAN_INTERFACES_SCHEMA_DIR");
}
}  // namespace uran_interfaces
