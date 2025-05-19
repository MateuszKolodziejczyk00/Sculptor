StartProjectsType(EProjectType.ThirdParty)

IncludeProject("VulkanSDK")
IncludeProject("Eigen")
IncludeProject("GLFW")
IncludeProject("Spdlog")
IncludeProject("Volk")
IncludeProject("VMA")
IncludeProject("YAML")
IncludeProject("Entt")
IncludeProject("NsightAftermath")
IncludeProject("TileableVolumeNoise")

IncludeProject("TinyGLTF")

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
IncludeProjectFromDirectory("ImGui", "ImGuiGLFWBackend")
IncludeProjectFromDirectory("ImGui", "ImGuiVulkanBackend")
ResetProjectsSubgroupName()