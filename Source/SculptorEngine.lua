StartProjectsType(EProjectType.Engine)

SetProjectsSubgroupName("Core")
IncludeProject("SculptorCore")
IncludeProject("Math")
IncludeProject("ProfilerCore")
IncludeProject("Platform")
IncludeProject("SculptorLib")
IncludeProject("UICore")
IncludeProject("Tokenizer")
IncludeProject("Serialization")

IncludeProject("JobSystem")

IncludeProject("SculptorECS")

SetProjectsSubgroupName("Engine")
IncludeProject("EngineCore")

SetProjectsSubgroupName("Graphics/RHI")
IncludeProject("RHI")

SetProjectsSubgroupName("Graphics/Platform")
IncludeProject("PlatformWindow")

SetProjectsSubgroupName("Graphics/Shaders")
IncludeProject("ShaderMetaData")
IncludeProject("ShaderCompiler")

SetProjectsSubgroupName("Graphics/Rendering")
IncludeProject("RendererCore")
IncludeProject("RenderGraph")
IncludeProject("Graphics")

SetProjectsSubgroupName("Tools")
IncludeProject("ScUI")
IncludeProject("Profiler")

SetProjectsSubgroupName("Scene")
IncludeProject("RenderScene")