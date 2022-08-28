RendererCore = Project:CreateProject("RendererCore", ETargetType.SharedLibrary)

function RendererCore:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("RendererTypes")

	self:AddPrivateDependency("EngineCore")
end

RendererCore:SetupProject()