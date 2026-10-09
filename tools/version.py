# PlatformIO pre-build script: the firmware version from git ("v1.4" on a release, "v1.4-3-gabc1234" between
# them, "-dirty" after it with uncommitted changes, "-test" for the test build, "dev" without git), written to a small header in the build folder (only rewritten when it changes, so
# just the files that show it are rebuilt). Settings shows it; the website's one-click sync compares it with the
# newest release.
import os
import subprocess

Import("env")

try:
    version = subprocess.check_output(["git", "describe", "--tags", "--always", "--dirty"], cwd=env.subst("$PROJECT_DIR"),
                                      stderr=subprocess.DEVNULL, text=True).strip()
except Exception:
    version = "dev"
if env["PIOENV"] == "dev":
    version += "-test"

folder = os.path.join(env.subst("$BUILD_DIR"), "generated")
os.makedirs(folder, exist_ok=True)
header = os.path.join(folder, "unidex_version.h")
text = '#pragma once\n#define UNIDEX_VERSION "%s"\n' % version
if not os.path.exists(header) or open(header).read() != text:
    with open(header, "w") as f:
        f.write(text)
env.Append(CPPPATH=[folder])
