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

IncludeProject("TinyGLTF")

IncludeProject("STB")

IncludeProject("MeshOptimizer")

SetProjectsSubgroupName("Profiling")
IncludeProject("Optick")
IncludeProject("PerformanceAPI")
ResetProjectsSubgroupName()

SetProjectsSubgroupName("Tests")
IncludeProject("GoogleTest")
ResetProjectsSubgroupName()

SetProjectsSubgroupName("ImGui")
IncludeProject("ImGui")
IncludeProjectFromDirectory("ImGui", "ImGuiGLFWBackend")
IncludeProjectFromDirectory("ImGui", "ImGuiVulkanBackend")
ResetProjectsSubgroupName()