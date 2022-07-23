RendererTypes = Project:CreateProject("RendererTypes", ETargetType.SharedLibrary)

function RendererTypes:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("RHI")
end

RendererTypes:SetupProject()