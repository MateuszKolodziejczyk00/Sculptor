SculptorEd = Project:CreateProject("SculptorEd", ETargetType.Application)

function SculptorEd:SetupConfiguration(configuration, platform)
	self:AddPublicDependency("SculptorLib")
	self:AddPublicDependency("RendererCore")

	self:AddPrivateDependency("EngineCore")
	self:AddPrivateDependency("Profiler")

	self:AddDebugArgument("-EngineRelativePath=../../../")

	if configuration == EConfiguration.Debug then
		self:AddDebugArgument("-EnableValidation")
	end
end

SculptorEd:SetupProject()