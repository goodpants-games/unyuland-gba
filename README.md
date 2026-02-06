# Unyuland Advance
A Gameboy Advance port of my game [Unyuland](https://pumkinhead.itch.io/unyuland)

## Building
Prerequisites:
- devkitARM (also with GBA development tools)
- Aseprite (non-free software, however)
- Tiled
- Python 3
- GNU Make

```bash
# this sets up devkitPro env variables. only need to be run once per terminal
# session. substitute /opt/devkitpro to the installation path of devkitPro on
# your computer.
export DEVKITPRO=/opt/devkitpro/
export DEVKITARM=/opt/devkitpro/devkitARM
export PATH="/opt/devkitpro/tools/bin:$PATH"

mkdir build

# this compiles the resources and code
# creates unyuland-gba.elf and unyuland-gba.gba in the project directory.
make
```