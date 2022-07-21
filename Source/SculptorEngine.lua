StartProjectsType(EProjectType.Engine)

SetProjectsSubgroupName("Core")
IncludeProject("SculptorCore")
IncludeProject("Math")
IncludeProject("Profiler")
IncludeProject("SculptorLib")

SetProjectsSubgroupName("Graphics/RHI")
IncludeProject("RHICore")
IncludeProject("VulkanRHI")
IncludeProject("RHI")

SetProjectsSubgroupName("Graphics/Rendering")
IncludeProject("Renderer")

SetProjectsSubgroupName("Graphics")
IncludeProject("Window")

