"""Schema catalog and semantic validation."""

import json
import math
import os
from pathlib import Path
from typing import Any, Dict, List, Mapping, Optional

try:
    from jsonschema import Draft202012Validator
except ImportError:
    Draft202012Validator = None
try:
    from referencing import Registry, Resource
except ImportError:
    Registry = None
    Resource = None
from jsonschema import Draft7Validator, RefResolver

from .model import OperationPhase, can_transition


class ContractError(ValueError):
    pass


def schema_root() -> Path:
    configured = os.environ.get("URAN_INTERFACES_SCHEMA_DIR")
    if configured:
        return Path(configured)
    packaged = Path(__file__).parent / "schemas"
    if packaged.is_dir():
        return packaged
    source = Path(__file__).parents[3] / "schemas"
    if source.is_dir():
        return source
    raise ContractError("URAN contract schema directory was not found")


class ContractValidator:
    def __init__(self, root: Optional[Path] = None):
        self.root = (root or schema_root()).resolve()
        self._schemas = {}  # type: Dict[str, Dict[str, Any]]
        resources = []
        store = {}
        for path in self.root.rglob("*.schema.json"):
            schema = json.loads(path.read_text(encoding="utf-8"))
            relative = str(path.relative_to(self.root))
            self._schemas[relative] = schema
            schema_id = schema.get("$id")
            if not schema_id:
                raise ContractError(f"schema has no $id: {relative}")
            store[schema_id] = schema
            if Resource is not None:
                resources.append((schema_id, Resource.from_contents(schema)))
        self._registry = Registry().with_resources(resources) if Registry is not None else None
        self._store = store

    def schema_names(self) -> List[str]:
        return sorted(str(path.relative_to(self.root)) for path in self.root.rglob("*.schema.json"))

    def validate(self, schema_name: str, document: Mapping[str, Any]) -> None:
        path = (self.root / schema_name).resolve()
        if self.root not in path.parents or not path.is_file():
            raise ContractError(f"unknown contract schema: {schema_name}")
        schema = self._schemas[schema_name]
        _reject_non_finite(document)
        if Draft202012Validator is not None and self._registry is not None:
            validator = Draft202012Validator(schema, registry=self._registry)
        else:
            resolver = RefResolver.from_schema(schema, store=self._store)
            validator = Draft7Validator(schema, resolver=resolver)
        errors = sorted(validator.iter_errors(document), key=lambda item: list(item.path))
        if errors:
            error = errors[0]
            location = ".".join(str(item) for item in error.absolute_path) or "$"
            raise ContractError(f"{location}: {error.message}")
        self._semantic_validate(schema_name, document)

    @staticmethod
    def _semantic_validate(schema_name: str, document: Mapping[str, Any]) -> None:
        if schema_name in {"management/command.schema.json", "management/grant.schema.json"}:
            if document["expires_at"] <= document["issued_at"]:
                raise ContractError("expires_at must be later than issued_at")
        elif schema_name in {"management/signed-command.schema.json", "management/signed-grant.schema.json"}:
            ContractValidator._semantic_validate(
                "management/command.schema.json" if "signed-command" in schema_name
                else "management/grant.schema.json", document["payload"])
            if "grant" in document:
                ContractValidator._semantic_validate("management/signed-grant.schema.json", document["grant"])
        elif schema_name == "motion/command.schema.json":
            if document["valid_until_ns"] <= document["issued_at_ns"]:
                raise ContractError("valid_until_ns must be later than issued_at_ns")
            payload = document["payload"]
        elif schema_name == "components/control-lease.schema.json":
            if document["expires_at_ns"] <= document["acquired_at_ns"]:
                raise ContractError("expires_at_ns must be later than acquired_at_ns")
        elif schema_name == "operations/timeline.schema.json":
            events = document["events"]
            identity = (events[0]["operation_id"], events[0]["target"])
            previous = None
            previous_sequence = -1
            for event in events:
                if (event["operation_id"], event["target"]) != identity:
                    raise ContractError("all timeline events must identify the same operation and target")
                if event["sequence"] <= previous_sequence:
                    raise ContractError("timeline event sequence must increase")
                phase = OperationPhase(event["status"])
                if previous is None and phase not in {OperationPhase.ACCEPTED, OperationPhase.REJECTED}:
                    raise ContractError("an operation timeline must begin with accepted or rejected")
                if previous is not None and not can_transition(previous, phase):
                    raise ContractError(f"invalid operation transition: {previous.value} -> {phase.value}")
                previous = phase
                previous_sequence = event["sequence"]


def _reject_non_finite(value: Any, path: str = "$") -> None:
    if isinstance(value, bool):
        return
    if isinstance(value, float) and not math.isfinite(value):
        raise ContractError(f"{path}: number must be finite")
    elif isinstance(value, Mapping):
        for key, item in value.items():
            _reject_non_finite(item, f"{path}.{key}")
    elif isinstance(value, list):
        for index, item in enumerate(value):
            _reject_non_finite(item, f"{path}[{index}]")
