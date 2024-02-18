SculptorCore = Project:CreateProject("SculptorCore", ETargetType.None)

function SculptorCore:SetupConfiguration(configuration, platform)
	self:AddPublicDefine("_SILENCE_ALL_CXX23_DEPRECATION_WARNINGS=1")
end

SculptorCore:SetupProject()