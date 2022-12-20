Graphics = Project:CreateProject("Graphics", ETargetType.None)

function Graphics:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("RendererCore")
    self:AddPublicDependency("RenderGraph")
end

Graphics:SetupProject()