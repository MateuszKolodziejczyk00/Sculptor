DLSS = Project:CreateProject("DLSS", ETargetType.None)


function DLSS:SetupConfiguration(configuration, platform)
	self:AddPublicRelativeIncludePath("/include")
	self:CopyProjectLibToOutputDir("/lib/Windows_x86_64/dev/nvngx_dlss.dll")
	self:CopyProjectLibToOutputDir("/lib/Windows_x86_64/dev/nvngx_dlssd.dll")

    self:SetPrecompiledLibsPath("/lib/Windows_x86_64/x64")
    if configuration == EConfiguration.Debug then
        self:AddPublicDependency("nvsdk_ngx_d_dbg")
    else
        self:AddPublicDependency("nvsdk_ngx_d")
    end
end


function DLSS:GetProjectFiles(configuration, platform)
    return
    {
       "DLSS/include/**.h"
    }
end

DLSS:SetupProject()