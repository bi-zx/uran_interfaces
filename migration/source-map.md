# Source map

| Source | Commit | Target | Treatment |
| --- | --- | --- | --- |
| `uran_ws/src/uran_msgs/msg/UnifiedMoveCmd.msg` | `300d703a99f5b950f780210694bf7b2e678e5075` | `schemas/motion/command.schema.json` | Split generic velocity/posture/action fields into typed payloads and add component target, operation, sequence and expiry. |
| `uran_ws/src/uran_msgs/msg/MediaCtrlCmd.msg` | `300d703a99f5b950f780210694bf7b2e678e5075` | `schemas/media/session-command.schema.json` | Preserve actions/protocol/signal while adding target, session and operation identity. |
| `uran_ws/src/uran_msgs/msg/TaskCtrlCmd.msg` | `300d703a99f5b950f780210694bf7b2e678e5075` | `schemas/tasks/command.schema.json` | Preserve task actions and JSON parameters as structured data with an explicit target and operation. |
| rebuild API 2.0 envelopes | `2cd172e8fbfee90f10fc8687b73f62b8ac32a2a7` | `docs/compatibility.md` | Retained as transport boundary; domain acceptance/result semantics remain in interfaces. |

