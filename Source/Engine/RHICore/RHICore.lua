RHICore = Project:CreateProject("RHICore", ETargetType.None)

function RHICore:SetupConfiguration(configuration, platform)
    self:AddPublicDependency("SculptorLib")

    self:AddPublicDefine("VULKAN_RHI=1")
end

RHICore:SetupProject()