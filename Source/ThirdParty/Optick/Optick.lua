Optick = Project:CreateProject("Optick", ETargetType.SharedLibrary)

function Optick:SetupConfiguration(configuration, platform)
    self:AddPrivateDefine("OPTICK_EXPORTS")
    self:AddPublicDefine("WITH_OPTICK=1")
    self:AddPublicDefine("OPTICK_ENABLE_GPU=1")
    self:AddPublicDefine("OPTICK_ENABLE_GPU_D3D12=0")

    self:AddPublicRelativeIncludePath("/src")

    if GetSelectedRHI() == ERHI.Vulkan then
        self:AddPublicDefine("OPTICK_ENABLE_GPU_VULKAN=1")
        self:AddPrivateAbsoluteIncludePath("$(Vulkan_SDK)/Include")
        -- normally we shouldn't link that library as we're using volk to load vulkan functions, but optick doesn't have ony option to provide vulkan functions ptrs
        -- so we have to use lib. It shouldn't cause any conflicts, as it's used only in one translation unit (included in optick_gpu_vulkan.cpp)
        self:AddPrivateDependency("vulkan-1")
    end
end

function Optick:GetAdditionalLibPaths()
    return 
    {
        "$(VULKAN_SDK)/Lib" -- for vulkan
    }
end


function Optick:GetProjectFiles()
    return
    {
        "Optick/src/**",
    }
end

Optick:SetupProject()