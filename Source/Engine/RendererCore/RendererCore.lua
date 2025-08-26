RendererCore = Project:CreateProject("RendererCore", ETargetType.SharedLibrary)

function RendererCore:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("RHI")
    self:AddPublicDependency("PlatformWindow")
    self:AddPublicDependency("ShaderCompiler")
	self:AddPublicDependency("ShaderStructs")

	self:AddPrivateDependency("EngineCore")
	self:AddPrivateDependency("JobSystem")

    if configuration ~= EConfiguration.Release or forceRHIValidation then
        self:AddPublicDefine("RENDERER_VALIDATION=1")
    else
        self:AddPublicDefine("RENDERER_VALIDATION=0")
    end

    self:AddPublicDefine("PROFILE_GPU=0")
    self:AddPublicDefine("WITH_SHADERS_HOT_RELOAD=1")

    self:AddPublicDefine("RENDERER_DEBUG=1")
    
    -- It seams that Nsight Graphics crashes while capturing frame in frame debugger if renderer waits for timeline sempahore
    -- This macro replaces all timeline semaphore waits with wait idle calls
    self:AddPublicDefine("WITH_NSIGHT_CRASH_FIX=0")
end

RendererCore:SetupProject()