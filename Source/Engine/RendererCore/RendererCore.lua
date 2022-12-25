RendererCore = Project:CreateProject("RendererCore", ETargetType.SharedLibrary)

function RendererCore:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("RHI")
    self:AddPublicDependency("PlatformWindow")
    self:AddPublicDependency("ShaderCompiler")
	self:AddPrivateDependency("EngineCore")
	self:AddPrivateDependency("JobSystem")

    self:AddPublicDefine("RENDERER_VALIDATION=1")
    self:AddPublicDefine("PROFILE_GPU=0")
    self:AddPublicDefine("WITH_SHADERS_HOT_RELOAD=1")
end

RendererCore:SetupProject()