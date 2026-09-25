# uran_interfaces

Platform-independent URAN business contracts. JSON Schema files are the
authoritative wire definitions. The C++ package and Python wheel provide the
same catalog, validation, typed identity/operation helpers, and fixtures; they
do not depend on ROS or the URAN Runtime.

This repository defines semantics only. It does not implement device drivers,
control algorithms, emergency actions, or claim hardware support.

`schemas/state/snapshot.schema.json` defines the Runtime state document. Its
`components` array is the authoritative component inventory: each entry is
identified by `(device_id, component_id)` and lists all plugin instances,
including components that currently have no state fields. The flat `fields`
member is retained for consumers that still use the legacy state view.

## Build and test

```bash
cmake -S . -B build -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
python3 -m venv /tmp/uran-interfaces-venv
/tmp/uran-interfaces-venv/bin/pip install -e '.[test]'
/tmp/uran-interfaces-venv/bin/pytest
```

The CMake install exports `uran_interfaces::uran_interfaces`. The Python import
is `uran_interfaces`; the CLI is `uran-contract-validate`.
