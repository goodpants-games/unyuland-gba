# Unyuland Advance
A Gameboy Advance port of my game [Unyuland](https://pumkinhead.itch.io/unyuland)

## Building
Prerequisites:
- devkitARM (also with GBA development tools)
- Aseprite (non-free software, however)
- Tiled
- Python 3
- GNU Make

### Gameboy Advance
```bash
# this sets up devkitPro env variables. only need to be run once per terminal
# session, or if these are already set. substitute /opt/devkitpro to the
# installation path of devkitPro on your computer.
export DEVKITPRO=/opt/devkitpro/
export DEVKITARM=/opt/devkitpro/devkitARM
export PATH="/opt/devkitpro/tools/bin:$PATH"

mkdir build

# this compiles the resources and code
# creates unyuland-gba.elf and unyuland-gba.gba in the project directory.
make gba
```

### Windows/Linux
Additional prerequisites:
- GCC/Clang (*only* works with these compilers. You can use MSYS2 on Windows.)
- SDL3
- libopenmpt
- A `pkg-config` interface
- OpenGL ES drivers
- (And yes, you still need devkitPro, for grit.)

```bash
# run this if you only have pkgconf installed, with no pkg-config symlink.
export PKGCONF=pkgconf

mkdir buildpc

# creates unyuland or unyuland.exe in the project directory.
make pc
```