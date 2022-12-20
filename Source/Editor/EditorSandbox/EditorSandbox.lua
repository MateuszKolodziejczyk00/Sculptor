EditorSandbox = Project:CreateProject("EditorSandbox", ETargetType.SharedLibrary)

function EditorSandbox:SetupConfiguration(configuration, platform)
	self:AddPublicDependency("SculptorLib")
	self:AddPublicDependency("Graphics")
	self:AddPublicDependency("JobSystem")
	self:AddPublicDependency("ScUI")

	self:AddPrivateDependency("EngineCore")
	self:AddPrivateDependency("Profiler")
end

EditorSandbox:SetupProject()