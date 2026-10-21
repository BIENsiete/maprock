# maprock

**maprock** is a cross-platform Minecraft: Bedrock Edition world exporter that turns
your worlds into 3D models (`.obj` / `.gltf`). It is inspired by
[Mineways](https://www.realtimerendering.com/erich/minecraft/public/mineways/) and
features a map view — just like the in-game map — where you drag a rectangle to
choose the part of the world you want to export. You can export the **whole world**
or just a **selection**, pick a height range, and choose between OBJ and glTF 2.0.

Built in C++17 with [CMake](https://cmake.org/), it compiles on **Windows (MinGW-w64)**,
**Linux** and **macOS** with the exact same code base (CI runs on all three via
GitHub Actions).

## Features

- Reads Bedrock **LevelDB** worlds (the `db/` folder) directly — no export needed.
- Supports the modern paletted **subchunk format** (version 8/9) and legacy worlds.
- Resolves block appearances from the official
  [Mojang/bedrock-samples](https://github.com/Mojang/bedrock-samples) resource pack
  (`blocks.json` + `terrain_texture.json` + the `textures/blocks/*.png|tga` atlas).
- Builds a packed **texture atlas**, applies tinting (grass, leaves, water, wool,
  concrete, …) and maps every face to its correct texture.
- Hidden-face culling, water surfaces, cross-shaped plants (flowers, saplings, …).
- Exports to:
  - **Wavefront OBJ** (`model.obj` + `model.mtl` + atlas PNG)
  - **glTF 2.0** (`model.gltf` + `.bin` + atlas PNG)
- **GUI**: a top-down map of your world where you drag to select a region, just like
  the map item in the game.
- **CLI**: scriptable export of a whole world or an exact bounding box.

## Building

Requirements: a C++17 compiler, CMake ≥ 3.16 and internet access (dependencies are
fetched automatically at configure time via CMake `FetchContent`).

### Windows (MinGW-w64, MSYS2)

```powershell
pacman -S --needed mingw-w64-x86_64-gcc mingw-w64-x86_64-cmake mingw-w64-x86_64-ninja
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Linux (Ubuntu / Debian)

```bash
sudo apt-get install -y build-essential cmake ninja-build libgl1-mesa-dev \
  libxrandr-dev libxinerama-dev libxcursor-dev libxi-dev
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### macOS

```bash
brew install cmake ninja
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

Disable the GUI (CLI-only build, no OpenGL/GLFW required):

```bash
cmake -S . -B build -DMAPROCK_BUILD_GUI=OFF
```

## Usage

The exporter needs two things: a world and the Bedrock resource pack.

```bash
# Whole world, OBJ
maprock --world "path/to/world" --resource-pack "path/to/bedrock-samples/resource_pack" \
        --whole --format obj --output world.obj

# A selection (inclusive block coordinates), glTF
maprock --world "path/to/world" --resource-pack "path/to/bedrock-samples/resource_pack" \
        --min -64,0,-64 --max 64,127,64 --format gltf --output selection

# Interactive map GUI
maprock --world "path/to/world" --resource-pack "path/to/bedrock-samples/resource_pack" --gui
```

On Windows the build also produces `maprock-gui.exe`, a GUI-only executable with no
console window: **double-click it** to open the map window directly and pick your
world folder (double-clicking `maprock.exe` with no arguments does the same).
```

`--world` can point at the world directory (the one containing `level.dat`) or at the
`db/` folder itself. `.mcworld` archives must be unzipped first.

You can download the official assets from
[bedrock-samples releases](https://github.com/Mojang/bedrock-samples/releases); pass
the extracted `resource_pack` directory.

## Project layout

```
src/
  nbt.*              little/big-endian NBT parser
  world.*            LevelDB reader + subchunk parser
  block_registry.*   blocks.json + terrain_texture.json resolution, block shapes
  texture_atlas.*    texture packing + tinting (stb_image)
  mesher.*           face culling + triangle generation
  obj_exporter.*     Wavefront OBJ writer
  gltf_exporter.*    glTF 2.0 writer
  map_image.*        top-down "in-game map" color image generator
  cli.*              command-line entry point
  gui/*              Dear ImGui + GLFW map view
tools/
  make_test_world.cpp  generates a tiny synthetic world for testing
```

## Credits & licenses

This project is only possible thanks to these great tools and data sources:

- **[Mojang/bedrock-samples](https://github.com/Mojang/bedrock-samples)** — the
  official Bedrock block definitions, textures and models. © Mojang Studios /
  Microsoft. Minecraft content and textures remain property of Mojang/Microsoft;
  please respect their
  [usage guidelines](https://www.minecraft.net/en-us/usage-guidelines). The assets are
  **not** redistributed in this repository — you download them separately.
- **[leveldb-mcpe](https://github.com/Mojang/leveldb)** (via
  [teampathfinders/leveldb](https://github.com/teampathfinders/leveldb)) — Google
  LevelDB with Mojang's zlib compression support (BSD-3-Clause).
- **[nlohmann/json](https://github.com/nlohmann/json)** — JSON for Modern C++ (MIT).
- **[stb](https://github.com/nothings/stb)** — `stb_image` / `stb_image_write`
  (public domain / MIT).
- **[Dear ImGui](https://github.com/ocornut/imgui)** — GUI (MIT).
- **[GLFW](https://github.com/glfw/glfw)** — windowing/OpenGL context (zlib/libpng).
- **[zlib](https://www.zlib.net/)** — compression (bundled with leveldb-mcpe).
- **[Mineways](https://github.com/erich666/Mineways)** — the inspiration for the UI
  and workflow (its own license applies; used only as a design reference).
- The Bedrock level format documentation from the
  [Minecraft Wiki](https://minecraft.wiki/w/Bedrock_Edition_level_format), plus
  community references such as
  [minecraft_mmccoo](https://github.com/mmccoo/minecraft_mmccoo) and
  [arnis](https://github.com/louis-e/arnis), were invaluable for understanding the
  on-disk format.

See `LICENSE` for the maprock license. Third-party licenses remain with their authors.

## Disclaimer

maprock is not affiliated with Mojang Studios or Microsoft. "Minecraft" is a
trademark of Mojang Synergies AB.
