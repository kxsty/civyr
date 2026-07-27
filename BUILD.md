## Prerequisites

- **C Compiler** with C23 support
- **CMake** ≥ 4.0
- **Build system** (Ninja is recommended)
- **pkg-config**
- **OpenGL**
- **libvips** library (e.g. [mingw libvips](https://packages.msys2.org/base/mingw-w64-libvips))
- **Python 3** (required by glad)
- **Internet connection** (required by FetchContent in CMake)

## Quick Start

### Installing dependencies

#### Windows (UCRT64, GCC, Ninja)

```bash
pacman -Syu --noconfirm

pacman -S mingw-w64-ucrt-x86_64-gcc \
          mingw-w64-ucrt-x86_64-ninja \
          mingw-w64-ucrt-x86_64-cmake \
          mingw-w64-ucrt-x86_64-pkgconf \
          mingw-w64-ucrt-x86_64-libvips

pacman -S --asdeps mingw-w64-ucrt-x86_64-libheif \
                   mingw-w64-ucrt-x86_64-libjxl \
                   mingw-w64-ucrt-x86_64-imagemagick \
                   mingw-w64-ucrt-x86_64-openslide \
                   mingw-w64-ucrt-x86_64-poppler \
                   mingw-w64-ucrt-x86_64-graphicsmagick
```

#### Ubuntu (Clang)

```bash
sudo apt update && sudo apt upgrade -y

sudo apt install gcc \
                 ninja-build \
                 cmake \
                 libvips-dev \
                 imagemagick \
                 graphicsmagick \
                 libglfw3-dev \
                 python3 \
                 python3-glad
```

### Installing

```bash
git clone https://github.com/kxsty/civyr.git

cd civyr
```

### Building

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -G Ninja

cmake --build build --config Release
```

### Optional CMake Flags

| Flag               | Default | Description                          |
|--------------------|---------|--------------------------------------|
| -DCIVYR_BUILD_ASAN | OFF     | Build Address Sanitizers (Unix only) |
| -DBUILD_TESTING    | OFF     | Build Unit tests                     |

## Packaging

```bash
cpack --config build/CPackConfig.cmake
```