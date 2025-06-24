# Sculptor
Toy vulkan renderer made for learning purposes.
# Build
- Download with submodules (You can use: git clone --recurse-submodules https://github.com/MateuszKolodziejczyk00/Sculptor)
- Download and install newest Vulkan SDK
- Run GenerateSolution.bat script
- Open solution and set SculptorEd as startup project
# Showcase
|  |  |  |
|---|---|---|
| ![](Images/GI2.gif) | ![](Images/GI1.gif) | ![](Images/Clouds.gif) |
| ![](Images/BistroInterior1.png) | ![](Images/SanMiguel1.png) | ![](Images/SanMiguel2.png) |
| ![](Images/SanMiguel3.png) | ![](Images/SanMiguel4.png) | ![](Images/SanMiguel5.png) |
| ![](Images/SunTemple1.png) | ![](Images/SunTemple2.png) | ![](Images/NewSponza1.png) |
| ![](Images/Sponza1.png) | ![](Images/Sponza2.png) | ![](Images/Bistro1.png) |
| ![](Images/Battle1.png) | ![](Images/Battle2.png) | ![](Images/Battle3.png) |

# Feature
- Ray Traced Reflections with spatiotemporal resampling (ReSTIR), Variable Rate Tracing and custom denoiser based on Nvidia RELAX
- Ray traced directional lights shadows with custom denoiser based on AMD FidelityFX Denoiser
- Render Graph
- DDGI with 3 LODs around camera (currently used only as light cache)
- Static mesh hierarchical culling using compute, task, and mesh shaders:V
  - Instance - frustum culling, occlusion culling (compute shader)
  - Meshlet - cone culling, frustum culling, occlusion culling (task shader)
  - Triangle - backface culling, small primitive culling (mesh shader)
  In addition to this, 2-pass occlusion culling is used. During first pass previous frame Hi-Z is reprojected and used for occlusion tests and then second pass renders disoccluded geometry
- visibility buffer rendering followed by materials depth pass and G-Buffer generation based on earily version of Unreal Engine's Nanite
- Volumetric Fog (with indirect lighting scattering)
- Volumetric Clouds, Aerial Perspective, Cloudscape Probes
- Physically based atmosphere
- Ray Traced ambient occlusion (same denoiser as shadows)
- Localized tonemapping based on bilarteral grid
- Depth pyramid rendering
- Bloom
- DPCF point lights shadows based on “Shadows of Cold War” by Kevin Myers (Also implemented MSM, standard PCF and PCSS), and dynamically selecting which lights should cast shadows
- Luminance histogram and automatic exposure
- Custom tool to capture and display textures written by each render graph node
- Assertions and print functions in shaders
- Super Resolution (DLSS)
- Temporal AA
- Depth of field
- Shaders hot reloading
- Bindless resources
- Physically based shading
- Lens flares
- Job system
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