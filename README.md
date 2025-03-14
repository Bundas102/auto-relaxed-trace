# Automatic Step Size Relaxation in Sphere Tracing
## The SDFRenderer project
![The running project with an example SDF](imgs/SDFRenderer.png)

This project (https://github.com/Bundas102/auto-relaxed-trace) contains the implementation for the Eurographics 2023 Short Paper: *Automatic Step Size Relaxation in Sphere Tracing*, Róbert Bán and Gábor Valasek. URL: https://doi.org/10.2312/egs.20231014

The project implements signed distance function and field rendering using sphere tracing and its variants.

The project uses procedural SDFs from (https://github.com/tovacinni/sdf-explorer) ported to HLSL/Slang.

## Building and running the project
- Clone [Falcor 8.0](https://github.com/NVIDIAGameWorks/Falcor/tree/8.0)
- Clone this repository into `Falcor\Source\Samples\SDFRenderer\`
- Add `SDFRenderer` to the cmake file `Falcor\Source\Samples\CMakeLists.txt`
    - Add the line `add_subdirectory(SDFRenderer)`
- Run `setup` in `Falcor`'s root
- On Windows and Visual Studio 2022
    - Run `Falcor\setup_vs2022.bat`
    - Run `Falcor\build\windows-vs2022\Falcor.sln`
    - Set `SDFRenderer` as the Startup Project
    - Build & run (some dependencies are not set right in Falcor, Build Solution might be necessary)
