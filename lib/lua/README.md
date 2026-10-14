# Lua 5.4.7

From https://www.lua.org/ftp/lua-5.4.7.tar.gz (SHA-256
9fbf5e28ef86c69858f6d3d34eccc32e911c1a28b4120ff3e84aaa70cfbf1e30, the sum lua.org publishes), unchanged.
Only `src/` is kept, without the files apps must never reach or that a device has no use for: `lua.c`, `luac.c`
(the command-line tools), `liolib.c`, `loslib.c`, `loadlib.c`, `ldblib.c` (files, the OS, modules, debugging) and
`linit.c` (which would open all of them). Licence: MIT, in `LICENSE`.
