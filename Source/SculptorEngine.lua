StartProjectsType(EProjectType.Engine)

SetProjectsSubgroupName("Core")
IncludeProject("SculptorCore")
IncludeProject("Math")
IncludeProject("Profiler")
IncludeProject("SculptorLib")
IncludeProject("UICore")
IncludeProject("Tokenizer")
IncludeProject("Serialization")

SetProjectsSubgroupName("Graphics/RHI")
IncludeProject("RHI")

SetProjectsSubgroupName("Graphics/Platform")
IncludeProject("PlatformWindow")

SetProjectsSubgroupName("Graphics/Shaders")
IncludeProject("ShaderMetaData")
IncludeProject("ShaderCompiler")

SetProjectsSubgroupName("Graphics/Rendering")
IncludeProject("RendererTypes")
IncludeProject("RendererCore")