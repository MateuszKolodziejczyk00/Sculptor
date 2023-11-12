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
- DDGI
- Ray Traced Specular and glossy Reflections (with SVGF denoiser)
- Volumetric Fog (with indirect lighting scattering)
- Physically based atmosphere
- Ray Traced ambient occlusion
- Static mesh hierarchical using compute shaders
  - Instance - frustum culling, occlusion culling
  - Meshlet - cone culling, frustum culling, occlusion culling
  - Triangle - backface culling, small primitive culling
- Depth Prepass
- Depth pyramid rendering
- Bloom
- Film tonemapper
- Luminance histogram and automatic exposure
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