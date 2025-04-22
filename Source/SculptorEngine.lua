StartProjectsType(EProjectType.Engine)

SetProjectsSubgroupName("Core")
IncludeProject("SculptorCore")
IncludeProject("Math")
IncludeProject("ProfilerCore")
IncludeProject("Platform")
IncludeProject("SculptorLib")
IncludeProject("Tokenizer")
IncludeProject("Serialization")

SetProjectsSubgroupName("Core/Utils/Blackboard")
IncludeProject("Blackboard")
IncludeProject("BlackboardTests")

SetProjectsSubgroupName("Core/ECS")
IncludeProject("SculptorECS")

SetProjectsSubgroupName("Core/JobSystem")
IncludeProject("JobSystem")
IncludeProject("JobSystemTests")

SetProjectsSubgroupName("Engine")
IncludeProject("EngineCore")
IncludeProject("Input")

SetProjectsSubgroupName("UI")
IncludeProject("UICore")
IncludeProject("ScUI")

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

SetProjectsSubgroupName("Graphics/Rendering/DLSS")
IncludeProjectFromDirectory("DLSS/SculptorDLSSVulkan", "SculptorDLSSVulkan")
IncludeProjectFromDirectory("DLSS/SculptorDLSS", "SculptorDLSS")

SetProjectsSubgroupName("Graphics/Rendering")
IncludeProject("Graphics")
IncludeProject("Materials")

SetProjectsSubgroupName("Scene")
IncludeProject("RenderScene")

SetProjectsSubgroupName("Core/AssetSystem")
IncludeProjectFromDirectory("Assets/DDC", "DDC")
IncludeProjectFromDirectory("Assets/AssetsSystem", "AssetsSystem")
IncludeProjectFromDirectory("Assets/AssetsSystemTests", "AssetsSystemTests")
IncludeProjectFromDirectory("Assets/TextureAsset", "TextureAsset")
IncludeProjectFromDirectory("Assets/TextureAssetTests", "TextureAssetTests")

SetProjectsSubgroupName("Tools")
IncludeProject("Profiler")
IncludeProject("RenderGraphCapturer")
IncludeProject("RenderSceneTools")