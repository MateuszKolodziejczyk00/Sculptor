SculptorEd = Project:CreateProject("SculptorEd", ETargetType.Application)

function SculptorEd:SetupConfiguration(configuration, platform)
	self:AddPublicDependency("SculptorLib")
	self:AddPublicDependency("RendererCore")

	self:AddPrivateDependency("EngineCore")
end

SculptorEd:SetupProject()