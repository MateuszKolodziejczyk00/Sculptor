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
IncludeProject("Input")

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

SetProjectsSubgroupName("Scene")
IncludeProject("RenderScene")

SetProjectsSubgroupName("Tools")
IncludeProject("ScUI")
IncludeProject("Profiler")
IncludeProject("RenderSceneTools")