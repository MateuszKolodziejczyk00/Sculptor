# Sculptor
Sculptor is a custom Vulkan renderer designed to explore and prototype cutting-edge real-time graphics techniques like ReSTIR GI and other ray tracing effects, denoisers, volumetric effects, GPU-driven rendering pipeline and post-processing.
Built from scratch for learning, experimentation, and performance research.
# Build
- Download with submodules (You can use: git clone --recurse-submodules https://github.com/MateuszKolodziejczyk00/Sculptor)
- Download and install newest Vulkan SDK
- Run GenerateSolution.bat script
- Open solution and set SculptorEd as startup project
# Showcase
## Video:
[![Check out the video](https://img.youtube.com/vi/zHKeeSrpyC0/0.jpg)](https://www.youtube.com/watch?v=zHKeeSrpyC0)
## Screenshots:
|  |  |  |
|---|---|---|
| ![](Images/GI2.gif) | ![](Images/GI1.gif) | ![](Images/Clouds.gif) |
| ![](Images/BistroInterior1.png) | ![](Images/SanMiguel1.png) | ![](Images/SanMiguel2.png) |
| ![](Images/SanMiguel3.png) | ![](Images/SanMiguel4.png) | ![](Images/SanMiguel5.png) |
| ![](Images/SunTemple1.png) | ![](Images/SunTemple2.png) | ![](Images/NewSponza1.png) |
| ![](Images/Sponza1.png) | ![](Images/Sponza2.png) | ![](Images/Bistro1.png) |
| ![](Images/Battle1.png) | ![](Images/Battle2.png) | ![](Images/Battle3.png) |

# Features
- Lighting
  - ReSTIR GI with Variable Rate Tracing
  - Ray Traced shadows (with VRT) and AO
  - Custom denoisers for diffuse and specular GI (based on Nvidia RELAX) and for shadows and AO (based on AMD FidelityFX Denoiser)
  - cascaded DDGI used as irradiance cache for secondary bounces
- Volumetrics & Atmosphere
  - Physically based atmosphere
  - Large scale volumetric fog based on froxels (up to 8km) affected by light sources, GI, clouds and atmosphere
  - Volumetric clouds fully integrated with GI using cloudscape probes
  - Aerial Perspective affecting and affected by clouds
- Scene Rendering
  - GPU-driven pipeline with 2-pass occlusion culling and visibility buffer
  - Instance, meshlets and per-triangle culling
  - Implemented in mesh shaders
- Post Processing
  - Localized tonemapping based on bilarteral grid
  - Automatic exposure
  - Bloom and Lens flares
  - Depth of field
  - Temporal AA
  - DLSS, DLSS RR
- General
  - Render Graph
  - Job System
  - Bindless Materials
  - Shaders hot-reloading
- Debugging
  - Custom in-engine capture tool that allows capturing all output textures from all passes and displaying them (very simple version of RenderDoc)
  - Printf in Shaders
  - Assertions in shaders
# Third Party libraries
- Eigen
- Entt
- GLFW
- ImGui
- Mesh Optimizer
- Nsight Aftermath
- Optick
- Premake
- Spdlog
- TinyGLTF
- Vulkan Memory Allocator
- Volk
- DXCompiler
- YAML