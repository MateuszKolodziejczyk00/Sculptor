EditorSandbox = Project:CreateProject("EditorSandbox", ETargetType.SharedLibrary)

function EditorSandbox:SetupConfiguration(configuration, platform)
	self:AddPublicDependency("SculptorLib")
	self:AddPublicDependency("RendererCore")
	self:AddPublicDependency("JobSystem")
	self:AddPublicDependency("ScUI")

	self:AddPrivateDependency("EngineCore")
	self:AddPrivateDependency("Profiler")
end

EditorSandbox:SetupProject()