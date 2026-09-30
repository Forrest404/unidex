# Firmware uploads use the ROM loader (--no-stub): on this board the esptool stub drops the
# USB-Serial/JTAG link after "Changing baud rate". The filesystem upload keeps the stub, because
# the ROM loader refuses its 1.5 MB erase. `upload_flags` can't do this: they land after write_flash.
Import("env")
from SCons.Script import COMMAND_LINE_TARGETS

if "uploadfs" not in COMMAND_LINE_TARGETS:
    env.Prepend(UPLOADERFLAGS=["--no-stub"])
