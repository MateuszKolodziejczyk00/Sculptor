GGXMultiscatteringLUTsGenerator = Project:CreateProject("GGXMultiscatteringLUTsGenerator", ETargetType.Application)

function GGXMultiscatteringLUTsGenerator:SetupConfiguration(configuration, platform)
	self:AddPrivateDependency("ToolsFramework")
end

GGXMultiscatteringLUTsGenerator:SetupProject()
