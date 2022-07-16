RHI = Project:CreateProject("RHI", ETargetType.None, EProjectType.Engine)

function RHI:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("VulkanRHI")
end

RHI:SetupProject()