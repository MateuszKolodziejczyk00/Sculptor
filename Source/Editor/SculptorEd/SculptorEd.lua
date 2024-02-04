SculptorEd = Project:CreateProject("SculptorEd", ETargetType.Application)

function SculptorEd:SetupConfiguration(configuration, platform)
	self:AddPrivateDependency("SculptorLib")
	self:AddPrivateDependency("Graphics")
	self:AddPrivateDependency("JobSystem")
	self:AddPrivateDependency("ScUI")

	self:AddPrivateDependency("EditorSandbox")

	self:AddPrivateDependency("EngineCore")
	self:AddPrivateDependency("Profiler")

	self:AddDebugArgument("-EngineRelativePath=../../../")

	self:AddDebugArgument("-Scene=Sponza/glTF/Sponza.gltf")
	
	self:AddDebugArgument("-EnableRayTracing")
end

SculptorEd:SetupProject()