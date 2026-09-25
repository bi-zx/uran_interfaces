#!/usr/bin/env python3
"""Generate language-specific schema catalogs from the authoritative files."""

import argparse
import json
from pathlib import Path
from typing import List, Tuple

GENERATOR_VERSION = "1.0.0"


def catalog(schema_root: Path) -> List[Tuple[str, str]]:
    result = []
    for path in sorted(schema_root.rglob("*.schema.json")):
        schema = json.loads(path.read_text(encoding="utf-8"))
        result.append((str(path.relative_to(schema_root)), schema["$id"]))
    return result


def generate_cpp(schema_root: Path, output: Path) -> None:
    entries = catalog(schema_root)
    output.parent.mkdir(parents=True, exist_ok=True)
    rows = "\n".join(f'  SchemaEntry{{"{name}", "{schema_id}"}},' for name, schema_id in entries)
    output.write_text(
        "#pragma once\n#include <array>\n#include <string_view>\n\n"
        "namespace uran_interfaces::generated {\n"
        "struct SchemaEntry { std::string_view name; std::string_view id; };\n"
        f'inline constexpr std::string_view kGeneratorVersion = "{GENERATOR_VERSION}";\n'
        f"inline constexpr std::array<SchemaEntry, {len(entries)}> kSchemas{{{{\n{rows}\n}}}};\n"
        "}  // namespace uran_interfaces::generated\n",
        encoding="utf-8",
    )


def generate_python(schema_root: Path, output: Path) -> None:
    entries = catalog(schema_root)
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(
        '"""Generated schema catalog. Do not edit."""\n\n'
        f'GENERATOR_VERSION = "{GENERATOR_VERSION}"\n'
        f"SCHEMAS = {entries!r}\n",
        encoding="utf-8",
    )


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--schemas", type=Path, required=True)
    parser.add_argument("--cpp", type=Path)
    parser.add_argument("--python", type=Path)
    args = parser.parse_args()
    if args.cpp:
        generate_cpp(args.schemas, args.cpp)
    if args.python:
        generate_python(args.schemas, args.python)


if __name__ == "__main__":
    main()
