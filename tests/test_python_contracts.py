import json
from pathlib import Path

import pytest

from uran_interfaces import (
    BaseVelocity,
    ComponentTarget,
    ContractError,
    ContractValidator,
    OperationPhase,
    can_transition,
    motion_command,
)


ROOT = Path(__file__).parents[1]
VALIDATOR = ContractValidator(ROOT / "schemas")


def test_cross_language_fixture_manifest():
    manifest = json.loads((ROOT / "fixtures/manifest.json").read_text())
    for case in manifest["cases"]:
        document = json.loads((ROOT / "fixtures" / case["file"]).read_text())
        for token in case.get("pointer", "").strip("/").split("/"):
            if token:
                document = document[int(token)] if isinstance(document, list) else document[token]
        if case["valid"]:
            VALIDATOR.validate(case["schema"], document)
        else:
            with pytest.raises(ContractError):
                VALIDATOR.validate(case["schema"], document)


def test_base_and_arm_are_independent_targets():
    fixture = json.loads((ROOT / "fixtures/valid/base_and_arm.json").read_text())
    VALIDATOR.validate("motion/command.schema.json", fixture["base"])
    VALIDATOR.validate("motion/command.schema.json", fixture["arm"])
    assert fixture["base"]["target"]["component_id"] != fixture["arm"]["target"]["component_id"]


def test_operation_timeline_and_terminal_rules():
    timeline = json.loads((ROOT / "fixtures/valid/operation_cancelled.json").read_text())
    VALIDATOR.validate("operations/timeline.schema.json", timeline)
    assert OperationPhase.CANCELLED.terminal
    assert can_transition(OperationPhase.RUNNING, OperationPhase.CANCELLED)
    assert not can_transition(OperationPhase.SUCCEEDED, OperationPhase.RUNNING)


def test_control_leases_are_component_scoped():
    leases = json.loads((ROOT / "fixtures/valid/control_leases.json").read_text())
    for lease in leases:
        VALIDATOR.validate("components/control-lease.schema.json", lease)
    assert {item["resource"]["resource_group"] for item in leases} == {"base-motion", "arm-motion"}


@pytest.mark.parametrize(
    ("fixture", "schema"),
    [("expired_command.json", "motion/command.schema.json"),
     ("terminal_transition.json", "operations/timeline.schema.json")],
)
def test_invalid_fixtures_are_rejected(fixture, schema):
    document = json.loads((ROOT / "fixtures/invalid" / fixture).read_text())
    with pytest.raises(ContractError):
        VALIDATOR.validate(schema, document)


def test_typed_builder_round_trip():
    command = motion_command(
        operation_id="op-typed",
        target=ComponentTarget("robot_001", "base"),
        controller="cloud",
        sequence=7,
        issued_at_ns=100,
        valid_until_ns=200,
        payload=BaseVelocity(linear_x_mps=0.1, angular_z_rps=-0.2),
    )
    VALIDATOR.validate("motion/command.schema.json", command)


def test_local_catalog_resolves_urn_refs_without_network():
    gps = {
        "schema_version": "1.0",
        "source": {"device_id": "robot_001", "component_id": "gps"},
        "timestamp_ns": 123,
        "status": "fix",
        "latitude_deg": 31.2,
        "longitude_deg": 121.5,
        "altitude_m": 4.0,
    }
    VALIDATOR.validate("sensors/gps-fix.schema.json", gps)


@pytest.mark.parametrize("value", [float("nan"), float("inf"), float("-inf")])
def test_every_public_schema_rejects_non_finite_numbers(value):
    payload = {
        "kind": "base_velocity",
        "frame_id": "base_link",
        "linear_mps": {"x": value, "y": 0.0, "z": 0.0},
        "angular_rps": {"x": 0.0, "y": 0.0, "z": 0.0},
    }
    with pytest.raises(ContractError, match="finite"):
        VALIDATOR.validate("motion/payloads/base-velocity.schema.json", payload)


def test_large_integer_does_not_overflow_validator():
    gps = {
        "schema_version": "1.0",
        "source": {"device_id": "robot_001", "component_id": "gps"},
        "timestamp_ns": 10**1000,
        "status": "fix",
        "latitude_deg": 31.2,
        "longitude_deg": 121.5,
        "altitude_m": 4.0,
    }
    VALIDATOR.validate("sensors/gps-fix.schema.json", gps)
