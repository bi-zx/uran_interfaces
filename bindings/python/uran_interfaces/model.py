"""Typed helpers for URAN contract documents."""

from dataclasses import dataclass
from enum import Enum
from typing import Any, Mapping


class OperationPhase(str, Enum):
    ACCEPTED = "accepted"
    RUNNING = "running"
    SUCCEEDED = "succeeded"
    FAILED = "failed"
    CANCELLED = "cancelled"
    REJECTED = "rejected"

    @property
    def terminal(self) -> bool:
        return self in {
            OperationPhase.SUCCEEDED,
            OperationPhase.FAILED,
            OperationPhase.CANCELLED,
            OperationPhase.REJECTED,
        }


_TRANSITIONS = {
    OperationPhase.ACCEPTED: {
        OperationPhase.RUNNING,
        OperationPhase.SUCCEEDED,
        OperationPhase.FAILED,
        OperationPhase.CANCELLED,
    },
    OperationPhase.RUNNING: {
        OperationPhase.RUNNING,
        OperationPhase.SUCCEEDED,
        OperationPhase.FAILED,
        OperationPhase.CANCELLED,
    },
}


def can_transition(previous: OperationPhase, current: OperationPhase) -> bool:
    """Return whether a lifecycle phase can follow another phase."""
    return current in _TRANSITIONS.get(previous, set())


@dataclass(frozen=True)
class ComponentTarget:
    device_id: str
    component_id: str
    instance_id: str = ""
    capability_id: str = ""

    def to_dict(self) -> dict[str, str]:
        result = {"device_id": self.device_id, "component_id": self.component_id}
        if self.instance_id:
            result["instance_id"] = self.instance_id
        if self.capability_id:
            result["capability_id"] = self.capability_id
        return result


@dataclass(frozen=True)
class BaseVelocity:
    linear_x_mps: float = 0.0
    linear_y_mps: float = 0.0
    linear_z_mps: float = 0.0
    angular_z_rps: float = 0.0
    frame_id: str = "base_link"

    def to_dict(self) -> dict[str, Any]:
        return {
            "kind": "base_velocity",
            "frame_id": self.frame_id,
            "linear_mps": {
                "x": self.linear_x_mps,
                "y": self.linear_y_mps,
                "z": self.linear_z_mps,
            },
            "angular_rps": {"x": 0.0, "y": 0.0, "z": self.angular_z_rps},
        }


@dataclass(frozen=True)
class BasePosture:
    roll_rad: float
    pitch_rad: float
    yaw_rad: float

    def to_dict(self) -> dict[str, Any]:
        return {
            "kind": "base_posture",
            "roll_rad": self.roll_rad,
            "pitch_rad": self.pitch_rad,
            "yaw_rad": self.yaw_rad,
        }


@dataclass(frozen=True)
class MotionAction:
    action: str
    reason: str = ""

    def to_dict(self) -> dict[str, str]:
        result = {"kind": "safety_action", "action": self.action}
        if self.reason:
            result["reason"] = self.reason
        return result


def motion_command(
    *,
    operation_id: str,
    target: ComponentTarget,
    controller: str,
    sequence: int,
    issued_at_ns: int,
    valid_until_ns: int,
    payload: Mapping[str, Any] | BaseVelocity | BasePosture | MotionAction,
    lease_id: str = "",
) -> dict[str, Any]:
    body = payload.to_dict() if hasattr(payload, "to_dict") else dict(payload)
    result: dict[str, Any] = {
        "schema_version": "1.0",
        "operation_id": operation_id,
        "target": target.to_dict(),
        "controller": controller,
        "sequence": sequence,
        "issued_at_ns": issued_at_ns,
        "valid_until_ns": valid_until_ns,
        "payload": body,
    }
    if lease_id:
        result["lease_id"] = lease_id
    return result

