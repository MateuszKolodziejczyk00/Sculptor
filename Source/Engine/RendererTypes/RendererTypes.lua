RendererTypes = Project:CreateProject("RendererTypes", ETargetType.SharedLibrary)

function RendererTypes:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("RHI")
    self:AddPublicDependency("PlatformWindow")
    self:AddPublicDependency("ShaderCompiler")

    self:AddPublicDefine("RENDERER_VALIDATION=1")
    self:AddPublicDefine("PROFILE_GPU=0")
end

RendererTypes:SetupProject()