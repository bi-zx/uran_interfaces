import subprocess
import sys


def test_import_uses_public_name():
    result = subprocess.run(
        [sys.executable, "-c", "import uran_interfaces; print(uran_interfaces.OperationPhase.ACCEPTED.value)"],
        check=True, text=True, capture_output=True,
    )
    assert result.stdout.strip() == "accepted"

