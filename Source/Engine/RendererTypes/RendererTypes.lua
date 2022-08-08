RendererTypes = Project:CreateProject("RendererTypes", ETargetType.SharedLibrary)

function RendererTypes:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("RHI")
    self:AddPublicDependency("PlatformWindow")
    self:AddPublicDependency("ShaderCompiler")

    self:AddPublicDefine("RENDERER_VALIDATION=1")
end

RendererTypes:SetupProject()