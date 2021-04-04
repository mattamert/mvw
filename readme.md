# mvw

mvw is a simple model viewer for .obj files. It largely serves as a basis for my experiments with D3D12 and Windows programming.

Usage: d3d12_renderer.exe &lt;obj_file&gt;

## Build instructions:

This project uses:
  - gn & ninja for the build system
  - clang for the compiler
  - MSVC/Visual Studio for the system libraries.

1. Download/Install: gn, ninja, clang, and visual studio.
2. Clone the repo with --recurse-submodules
3. cd into the repo and run: "gn gen out/x64_Debug" (only x64 is supported right now)
4. Build it with either of the following ways:
    - Open the solution file with Visual Studio & run it.
    - Alternatively, cd into the out/x64_Debug directory and run "ninja all".
