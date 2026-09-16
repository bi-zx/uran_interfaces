# Compatibility and transport boundary

Contract version `1.0` is platform independent. The supported public imports
are `uran_interfaces` for Python and `<uran_interfaces/contracts.hpp>` plus the
`uran_interfaces::uran_interfaces` CMake target for C++.

The API 2.0 `UplinkEnvelope` and `DownlinkEnvelope` are transport/routing
containers. An adapter maps `message_type` to a schema and places the complete
contract JSON in `payload`; `DownlinkEnvelope.target` must agree with the
contract `target`. A transport success means the message was routed. Only an
operation event with phase `accepted` means the provider accepted work, and
only `succeeded`, `failed`, `cancelled`, or `rejected` is terminal.

This repository defines no vendor-specific safety behavior. In particular,
`emergency_stop` is a typed intent whose physical action belongs to the device
plugin and must be hardware tested there.

