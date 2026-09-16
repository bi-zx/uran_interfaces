import argparse
import json
from pathlib import Path

from .validation import ContractValidator


def main() -> None:
    parser = argparse.ArgumentParser(description="Validate a URAN contract document")
    parser.add_argument("schema", help="schema path, for example motion/command.schema.json")
    parser.add_argument("document", type=Path)
    arguments = parser.parse_args()
    ContractValidator().validate(arguments.schema, json.loads(arguments.document.read_text(encoding="utf-8")))


if __name__ == "__main__":
    main()

