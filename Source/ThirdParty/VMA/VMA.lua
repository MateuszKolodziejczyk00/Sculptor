VMA = Project:CreateProject("VMA", ETargetType.None, EProjectType.ThirdParty, ELanguage.C)

function VMA:GetProjectFiles(configuration, platform)
    return
    {
        "VMA/include/vk_mem_alloc.h"
    }
end

function VMA:GetIncludesRelativeLocation()
    return "/include"
end

VMA:SetupProject()