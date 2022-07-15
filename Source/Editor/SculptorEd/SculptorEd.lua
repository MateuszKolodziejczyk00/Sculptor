SculptorEd = Project:CreateProject("SculptorEd", ETargetType.Application, EProjectType.Editor)

function SculptorEd:SetupConfiguration(configuration, platform)
	self:AddPublicDependency("SculptorLib")
end

SculptorEd:SetupProject()