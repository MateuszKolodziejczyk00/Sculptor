SculptorEd = Project:CreateProject("SculptorEd", ETargetType.Application)

function SculptorEd:SetupConfiguration(configuration, platform)
	self:AddPublicDependency("SculptorLib")
	self:AddPublicDependency("PlatformWindow")
end

SculptorEd:SetupProject()