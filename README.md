# uran_interfaces

Platform-independent URAN business contracts. JSON Schema files are the
authoritative wire definitions. The C++ package and Python wheel provide the
same catalog, validation, typed identity/operation helpers, and fixtures; they
do not depend on ROS or the URAN Runtime.

This repository defines semantics only. It does not implement device drivers,
control algorithms, emergency actions, or claim hardware support.

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

