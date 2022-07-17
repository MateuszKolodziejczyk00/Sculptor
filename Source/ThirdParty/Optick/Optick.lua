Optick = Project:CreateProject("Optick", ETargetType.SharedLibrary)

function Optick:SetupConfiguration(configuration, platform)
    self:AddPrivateDefine("OPTICK_EXPORTS")
    self:AddPublicDefine("WITH_OPTICK=1")
    self:AddPublicDefine("OPTICK_ENABLE_GPU_VULKAN=0")
    self:AddPublicDefine("OPTICK_ENABLE_GPU_D3D12=0")

    self:AddPublicRelativeIncludePath("/src")
end

function Optick:GetProjectFiles()
    return
    {
        "Optick/src/**",
    }
end

Optick:SetupProject()