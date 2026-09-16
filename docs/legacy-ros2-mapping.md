# Legacy ROS 2 mapping

The ROS 2 adapter owns this conversion. These mappings preserve old topic
behavior while the typed contract remains authoritative.

| Legacy field | Contract field |
| --- | --- |
| `UnifiedMoveCmd.device_id` | `target.device_id` |
| adapter parameter `default_motion_component` | `target.component_id` (legacy message has no part identity) |
| `controller` | `controller` |
| `timestamp_ns` | `issued_at_ns`; adapter policy supplies `valid_until_ns` |
| adapter sequence counter | `sequence` (legacy message has no sequence) |
| `linear_vel_x/y/z` | `payload.linear_mps.x/y/z` |
| `angular_vel_z` | `payload.angular_rps.z`; x/y are zero |
| `target_roll/pitch/yaw` | a separate `base_posture` request when finite and requested |
| `action` | `safety_action.action`; unknown vendor actions are rejected, not passed through |
| `extra_json` | `metadata.legacy_extra`; never interpreted as a typed hardware command |
| `TaskCtrlCmd.task_id/type/action` | `task_id/task_type/action` |
| `TaskCtrlCmd.task_params_json` | parsed `parameters`; malformed JSON is rejected |
| `MediaCtrlCmd.action/protocol/channel_id` | same fields plus adapter-created `session_id` and `operation_id` |
| `MediaCtrlCmd.signal_json` | parsed `signal`; malformed JSON is rejected |

The old messages have no operation acknowledgement. The bridge emits an
`accepted` or `rejected` event after validation and routing; providers emit
subsequent `running` and terminal events. A successful ROS publish alone is not
an acceptance. Cancellation uses `operations/cancel.schema.json` and cannot be
represented by `UnifiedMoveCmd.action=stop`, which is a new motion operation.

Legacy `ModeSwitchCmd` is an input to a control-arbiter implementation. It does
not itself grant a lease. The arbiter publishes a
`components/control-lease.schema.json` document scoped to the selected
component/resource group.

