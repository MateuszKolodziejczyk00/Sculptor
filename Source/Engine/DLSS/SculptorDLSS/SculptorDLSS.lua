SculptorDLSS = Project:CreateProjectInDirectory("DLSS/SculptorDLSS", "SculptorDLSS", ETargetType.None)

function SculptorDLSS:SetupConfiguration(configuration, platform)
    self:AddPublicDefine("ENABLE_DLSS=1")

    if GetSelectedRHI() == ERHI.Vulkan then
        self:AddPublicDependency("SculptorDLSSVulkan")
    else
        print("DLSS is only supported on Vulkan")
    end
end

SculptorDLSS:SetupProject()