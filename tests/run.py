"""Run pinned OBS cases serially; each has its own profile, display and production."""

import argparse
import subprocess
import sys
from pathlib import Path

parser = argparse.ArgumentParser()
parser.add_argument("root", type=Path)
parser.add_argument("--library", required=True, type=Path)
parser.add_argument("--build", type=Path, default=Path("build"))
args = parser.parse_args()
root = args.root.resolve()
root.mkdir(parents=True, exist_ok=False)
script = Path(__file__).with_name("host.py")
for mode in (
    "normal",
    "retry",
    "failure",
    "recording-failure",
    "shutdown",
    "no-plugin",
    "no-library",
    "no-production",
):
    subprocess.run(
        [
            sys.executable,
            str(script),
            str(root / mode),
            "--mode",
            mode,
            "--library",
            str(args.library.resolve()),
            "--build",
            str(args.build.resolve()),
        ],
        check=True,
        timeout=60,
    )
