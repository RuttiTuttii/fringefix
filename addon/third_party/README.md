vendored headers for building the ReShade add-on, no code from them is linked into the exe

- `reshade/`: `include/*.hpp` of crosire/reshade tag v6.8.0 (BSD-3-Clause, see `reshade/LICENSE.md`)
- `imgui/`: `imgui.h` and `imconfig.h` of ocornut/imgui at commit 3912b3d9a9c1b3f17431aebafd86d2f40ee6e59c, the revision ReShade v6.8.0 pins as its submodule (version 1.92.5, MIT, see `imgui/LICENSE.txt`)

the add-on asks ReShade for the function table of exactly this ImGui version, so both directories are updated together with the ReShade tag
