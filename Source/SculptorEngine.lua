StartProjectsType(EProjectType.Engine)

SetProjectsSubgroupName("Core")
IncludeProject("SculptorCore")
IncludeProject("Math")
IncludeProject("Profiler")
IncludeProject("SculptorLib")

SetProjectsSubgroupName("Rendering")
IncludeProject("RHICore")
IncludeProject("VulkanRHI")
IncludeProject("RHI")
IncludeProject("Window")

