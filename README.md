# scop — C++ / Vulkan OBJ viewer

This project is a Vulkan implementation of the `SCOP` subject.

It includes:
- a custom `.obj` parser
- a custom matrix / vector math module
- per-face color rendering
- perspective projection
- continuous rotation around the centered model
- translation on all three axes
- texture toggle with a smooth blend
- fallback generated UVs when the OBJ has no texture coordinates
- ear-clipping triangulation for concave polygon support as a bonus-oriented feature

## Dependencies

You need development packages for:
- Vulkan
- SDL2
- `glslc` or `glslangValidator` to compile shaders

On Debian / Ubuntu, that is usually close to:

```bash
sudo apt install build-essential libsdl2-dev libvulkan-dev glslang-tools
```

If you prefer `glslc`, install the Vulkan SDK or the package that provides it on your distro.

## Build

On macOS with the LunarG SDK in `/goinfre`, you can install it with:

```bash
make install-vulkan
source /goinfre/$USER/VulkanSDK/<version>/setup-env.sh
```

Then build with:

```bash
make
```

## Run

Default paths:

```bash
./scop assets/demo_cube.obj assets/pony.ppm
```

You can also pass another OBJ and texture:

```bash
./scop path/to/model.obj path/to/texture.ppm
```

## Controls

- `Left / Right`: move on X
- `Up / Down`: move on Y
- `PageUp / PageDown` or `Q / E`: move on Z
- `T`: toggle texture / color mode with a smooth transition
- `Space`: pause / resume rotation
- `R`: reset translation + rotation
- `Esc`: quit

## Notes

- The texture loader is intentionally simple and supports **PPM (P3 / P6)** only, to stay within the spirit of the subject.
- If the model has no UVs, the loader generates box-projected UVs so texture mode still works.
- Faces are triangulated internally. Concave coplanar faces are handled with ear clipping. Non-coplanar faces are processed with a projected best effort triangulation.
- Put the official `42.obj` resource in `assets/42.obj` for the defense demo, or pass its path directly on the command line.

## Subject coverage

Mandatory:
- custom OBJ loading
- perspective rendering
- centered rotation
- 3-axis movement
- distinguishable face colors
- texture toggle
- smooth transition between color and texture

Bonus-oriented parts:
- support for more difficult OBJ faces through triangulation
- generated UV fallback to reduce ugly texture behavior when UVs are missing


## Useful Make targets

- `make install-vulkan` — installs the LunarG Vulkan SDK on macOS using the bundled script
- `make print-config` — shows the compiler, SDL, shader, and Vulkan paths detected by the Makefile
- `make run` — builds and runs with the demo OBJ and texture by default
