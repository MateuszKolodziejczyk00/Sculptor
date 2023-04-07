# Sculptor
Toy vulkan renderer made for learning purposes.
# Build
- Download with submodules (You can use: git clone --recurse-submodules https://github.com/MateuszKolodziejczyk00/Sculptor)
- Download and install newest Vulkan SDK
- Run GenerateSolution.bat script
- Open solution and set SculptorEd as startup project
# Features
- Clustered Forward Renderer (based on “Rendering of Call of Duty Infinite Warfare” by Michal Drobot)
- DPCF point lights shadows based on “Shadows of Cold War” by Kevin Myers (Also implemented MSM, standard PCF and PCSS), and dynamically selecting which lights should cast shadows
- Ray traced directional lights shadows
- Render Graph
- Depth Prepass
- Depth pyramid rendering
- Static mesh hierarchical using compute shaders
  - Instance - frustum culling, occlusion culling
  - Meshlet - cone culling, frustum culling, occlusion culling
  - Triangle - backface culling, small primitive culling
- Bloom
- Film tonemapper
- Luminance histogram and automatic exposure
- Temporal AA
- Volumetric Fog
- Shaders hot reloading
- Bindless resources
- Physically based shading
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
