VMA = Project:CreateProject("VMA", ETargetType.None, ELanguage.C)

function VMA:SetupConfiguration(configuration, platform)
    self:AddPublicRelativeIncludePath("/include")
end

function VMA:GetProjectFiles(configuration, platform)
    return
    {
        "VMA/include/vk_mem_alloc.h"
    }
end

VMA:SetupProject()