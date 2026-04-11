# scop

A small **C++ / Vulkan** 3D viewer for the 42 `SCOP` project.

It loads a `.obj` model, displays it in perspective, rotates it around its center, lets you move it on all three axes, and switches between a plain shaded view and a textured/material view with a smooth transition.

## Features

-   C++ + Vulkan renderer
-   Custom `.obj` parser
-   Custom math types for vectors and matrices
-   Perspective projection
-   Continuous rotation around the model center
-   Translation on **X / Y / Z**
-   Smooth toggle between display modes with `T`
-   PPM texture loading (`P3` / `P6`)
-   Generated fallback UVs when the `.obj` has no texture coordinates
-   Internal triangulation of faces
-   Optional `.mtl` support for material values / texture path fallback
-   macOS Vulkan bootstrap through the provided install script

## Project structure

```text
.
├── assets/
│   ├── demo_cube.obj
│   └── pony.ppm
├── include/
├── shaders/
│   ├── mesh.vert
│   └── mesh.frag
├── src/
├── scripts/
│   └── install_vulkan.sh
└── Makefile
```

## Requirements

### macOS

-   Xcode command line tools
-   `brew`
-   `glfw`
-   LunarG Vulkan SDK / MoltenVK

The `Makefile` is set up to help on macOS:

-   if Vulkan is not found, it runs `scripts/install_vulkan.sh`
-   if GLFW is not found, it runs:

```bash
42-brew
brew install glfw
```

### Linux

You need:

-   a C++ compiler
-   Vulkan development files
-   GLFW
-   `glslc` or `glslangValidator`

Example packages depend on your distro.

## Build

```bash
make
```

Useful targets:

```bash
make print-config
make install-vulkan
make run
make re
```

## Run

Default demo:

```bash
./scop assets/demo_cube.obj assets/pony.ppm
```

With another model:

```bash
./scop path/to/model.obj
```

With another model and explicit texture:

```bash
./scop path/to/model.obj path/to/texture.ppm
```

## Texture / material behavior

### Explicit texture

If you pass a second argument, it is used as the texture.

Example:

```bash
./scop assets/42.obj assets/pony.ppm
```

### `.mtl` fallback

If no second argument is passed, the program can try to use material data from the `.obj` / `.mtl` pair.

That means:

-   if the `.mtl` contains a `map_Kd`, it can use that texture path
-   if the `.mtl` contains only material values like `Kd`, `Ks`, `Ns`, it can still provide a material-based shaded mode
-   if there is no usable texture, the app falls back cleanly

### Supported texture format

The texture loader supports:

-   `PPM P3`
-   `PPM P6`

It does **not** directly load `.png`, `.jpg`, or `.mtl` as image files.

## Controls

-   `Left / Right` → move on X
-   `Up / Down` → move on Y
-   `PageUp / PageDown` or `Q / E` → move on Z
-   `T` → smooth toggle between white mode and texture/material mode
-   `Space` → pause / resume rotation
-   `R` → reset transform
-   `Esc` → quit

## Rendering notes

-   The object is centered and scaled before rendering
-   The model rotates around its own center
-   Perspective projection is used
-   A soft lighting/shadow effect is applied for better depth
-   Face culling may be disabled for better compatibility with inconsistent OBJ winding
-   Fallback UV generation is used when texture coordinates are missing

## Subject coverage

### Mandatory

-   load and parse `.obj`
-   render inside a window
-   perspective projection
-   centered rotation
-   movement on all 3 axes
-   visually distinct rendering
-   toggle textured view with a key
-   smooth transition between modes

### Bonus-oriented work

-   more robust triangulation
-   fallback UV generation
-   optional material handling from `.mtl`

## Notes

-   `.mtl` is **not** a texture image
-   if you run:

```bash
./scop assets/teapot2.obj assets/teapot2.mtl
```

that is incorrect, because `.mtl` is a material file, not an image

Use either:

```bash
./scop assets/teapot2.obj
```

or:

```bash
./scop assets/teapot2.obj assets/some_texture.ppm
```

## Defense tip

For the evaluation, make sure you can launch the official `42.obj` quickly, and verify:

-   centered rotation
-   perspective view
-   movement on X / Y / Z
-   `T` transition works
-   model remains visible from multiple angles

## Author

42 `SCOP` project – Vulkan implementation
