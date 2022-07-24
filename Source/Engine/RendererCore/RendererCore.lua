RendererCore = Project:CreateProject("RendererCore", ETargetType.SharedLibrary)

function RendererCore:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("RendererTypes")
end

RendererCore:SetupProject()