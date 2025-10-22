RenderGraphCapturer = Project:CreateProject("RenderGraphCapturer", EngineLibrary)

function RenderGraphCapturer:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("SculptorLib")
    self:AddPublicDependency("ScUI")
    
    self:AddPublicDependency("RendererCore")

	self:AddPrivateDependency("EngineCore")
	self:AddPrivateDependency("RenderGraph")
	
    self:AddPrivateDependency("Graphics")
end

RenderGraphCapturer:SetupProject()