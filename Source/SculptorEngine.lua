StartProjectsType(EProjectType.Engine)

SetProjectsSubgroupName("Core")
IncludeProject("SculptorCore")
IncludeProject("Math")
IncludeProject("Profiler")
IncludeProject("SculptorLib")
IncludeProject("UICore")

SetProjectsSubgroupName("Graphics/RHI")
IncludeProject("RHICore")
IncludeProject("VulkanRHI")
IncludeProject("RHI")

SetProjectsSubgroupName("Graphics/Platform")
IncludeProject("PlatformWindow")

SetProjectsSubgroupName("Graphics/Rendering")
IncludeProject("RendererTypes")
IncludeProject("RendererCore")