StartProjectsType(EProjectType.ThirdParty)

IncludeProject("VulkanSDK")
IncludeProject("Eigen")
IncludeProject("Spdlog")
IncludeProject("Volk")
IncludeProject("VMA")
IncludeProject("JSON")
IncludeProject("Entt")
IncludeProject("NsightAftermath")
IncludeProject("Slang")
IncludeProject("TileableVolumeNoise")

IncludeProject("CGLTF")
IncludeProject("TinyEXR")
IncludeProject("TinyTIFF")

IncludeProject("DDS")
IncludeProject("STB")

IncludeProject("MeshOptimizer")

SetProjectsSubgroupName("Profiling")
IncludeProject("PerformanceAPI")
IncludeProject("PIX")
ResetProjectsSubgroupName()

SetProjectsSubgroupName("Graphics")
IncludeProject("DLSS")
ResetProjectsSubgroupName()

SetProjectsSubgroupName("Tests")
IncludeProject("GoogleTest")
ResetProjectsSubgroupName()

SetProjectsSubgroupName("ImGui")
IncludeProject("ImGui")
IncludeProjectFromDirectory("ImGui", "ImGuiWin32Backend")
IncludeProjectFromDirectory("ImGui", "ImGuiVulkanBackend")
ResetProjectsSubgroupName()
