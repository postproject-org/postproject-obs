"""Run an actual isolated OBS recording and check public PostProject readback."""

import argparse
import os
import shutil
import subprocess
from pathlib import Path

from postproject import Production, RepresentationAvailability

parser = argparse.ArgumentParser()
parser.add_argument("root", type=Path)
parser.add_argument("--build", type=Path, default=Path("build"))
parser.add_argument("--library", required=True)
parser.add_argument(
    "--mode",
    choices=(
        "normal",
        "retry",
        "failure",
        "shutdown",
        "no-plugin",
        "no-library",
        "no-production",
    ),
    default="normal",
)
args = parser.parse_args()
root = args.root.resolve()
root.mkdir(parents=True, exist_ok=True)
config = root / "config" / "obs-studio"
profile = config / "basic" / "profiles" / "PostProject"
profile.mkdir(parents=True, exist_ok=True)
recordings = root / "recordings"
recordings.mkdir(exist_ok=True)
(config / "global.ini").write_text("[General]\nLastVersion=537001984\n")
(config / "user.ini").write_text(
    "[General]\nFirstRun=true\n[Basic]\nProfile=PostProject\nProfileDir=PostProject\n"
)
(profile / "basic.ini").write_text(
    "[General]\nName=PostProject\n[Video]\nBaseCX=64\nBaseCY=64\n"
    "OutputCX=64\nOutputCY=64\nFPSType=0\nFPSCommon=30\nAutoRemux=false\n"
    "[Output]\nMode=Simple\n[SimpleOutput]\nRecEncoder=x264\nRecQuality=Small\n"
    f"FilePath={recordings}\nRecFormat2=mkv\nRecFormat=mkv\n"
)
for name in ("postproject-obs", "postproject-obs-driver"):
    if name == "postproject-obs" and args.mode == "no-plugin":
        continue
    directory = config / "plugins" / name
    (directory / "bin" / "64bit").mkdir(parents=True, exist_ok=True)
    (directory / "data").mkdir(exist_ok=True)
    shutil.copy2(args.build / f"lib{name}.so", directory / "bin" / "64bit")
environment = os.environ | {
    "XDG_CONFIG_HOME": str(root / "config"),
    "POSTPROJECT_OBS_PRODUCTION": str(root / "shared.pproj"),
    "POSTPROJECT_OBS_CREATE": "1",
    "POSTPROJECT_ABI_TRACE": str(root / "obs-normal.txt"),
    "QT_QPA_PLATFORM": "xcb",
    "LIBGL_ALWAYS_SOFTWARE": "1",
    "POSTPROJECT_OBS_TEST_MODE": args.mode,
    "LD_LIBRARY_PATH": str(Path(args.library).resolve().parent),
}
if args.mode == "no-library":
    environment.pop("LD_LIBRARY_PATH")
if args.mode == "no-production":
    environment.pop("POSTPROJECT_OBS_PRODUCTION")
with (root / "xvfb.log").open("w") as xlog:
    display = subprocess.Popen(
        ["Xvfb", "-displayfd", "1", "-screen", "0", "1024x768x24", "-nolisten", "tcp"],
        stdout=subprocess.PIPE,
        stderr=xlog,
        text=True,
    )
    try:
        environment["DISPLAY"] = ":" + display.stdout.readline().strip()
        with (root / "obs.log").open("w") as log:
            subprocess.run(
                [
                    "obs",
                    "--multi",
                    "--disable-shutdown-check",
                    "--disable-missing-files-check",
                    "--profile",
                    "PostProject",
                ],
                env=environment,
                stdout=log,
                stderr=subprocess.STDOUT,
                timeout=45,
                check=True,
            )
    finally:
        display.terminate()
        display.wait(timeout=5)
files = list(recordings.glob("*.mkv"))
assert len(files) == 1, files
subprocess.run(
    ["ffmpeg", "-v", "error", "-i", str(files[0]), "-f", "null", "-"], check=True
)
log = (root / "obs.log").read_text()
if args.mode == "failure":
    assert "Registration failed; retry this attempt" in log
    (root / "shared.pproj.offline").rename(root / "shared.pproj")
if args.mode in {"no-plugin", "no-library", "no-production"}:
    assert not (root / "shared.pproj").exists()
    print(f"OBS playable recording without {args.mode}: {root}")
    raise SystemExit(0)
assert "Worker joined before frontend exit boundary" in log
if args.mode == "retry":
    assert log.count("Recording registered successfully") == 2
with Production.open(root / "shared.pproj", library_path=args.library) as production:
    if args.mode in {"failure", "shutdown"} and len(production.assets) == 0:
        print(f"OBS {args.mode} preserved playable media and joined its worker: {root}")
        raise SystemExit(0)
    assert len(production.assets) == 1
    asset = next(iter(production.assets)).id
    representation = production.representations[asset][0]
    assert (
        production.resolutions[asset][0].availability
        == RepresentationAvailability.ONLINE
    )
    assert len(production.activities_producing[representation.id]) == 1
print(f"OBS recording and public readback passed: {root}")
