RenderGraphCapturer = Project:CreateProject("RenderGraphCapturer", ETargetType.SharedLibrary)

function RenderGraphCapturer:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("SculptorLib")
    self:AddPublicDependency("ScUI")
    
    self:AddPublicDependency("RendererCore")

	self:AddPrivateDependency("EngineCore")
	self:AddPrivateDependency("RenderGraph")
	
    self:AddPrivateDependency("Graphics")
end

RenderGraphCapturer:SetupProject()