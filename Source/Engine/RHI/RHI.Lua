RHI = Project:CreateProject("RHI", ETargetType.None)

function RHI:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("VulkanRHI")
end

RHI:SetupProject()