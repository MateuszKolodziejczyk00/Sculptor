GLFW = Project:CreateProject("GLFW", ETargetType.StaticLibrary, EProjectType.ThirdParty, ELanguage.C)

function GLFW:GetIncludesRelativeLocation()
    return "/include"
end

function GLFW:SetupConfiguration(configuration, platform)
    if platform == EPlatform.Windows then
        GLFW:AddPrivateDefine("_GLFW_WIN32")
        GLFW:AddPrivateDefine("_CRT_SECURE_NO_WARNINGS")
    end
end

function GLFW:GetProjectFiles(configuration, platform)
    if platform == EPlatform.Windows then
        return
        {
            "GLFW/include/GLFW/glfw3.h",
		    "GLFW/include/GLFW/glfw3native.h",
		    "GLFW/src/glfw_config.h",
		    "GLFW/src/context.c",
		    "GLFW/src/init.c",
		    "GLFW/src/input.c",
		    "GLFW/src/monitor.c",
		    "GLFW/src/vulkan.c",
		    "GLFW/src/window.c",

            "GLFW/src/win32_init.c",
			"GLFW/src/win32_joystick.c",
			"GLFW/src/win32_monitor.c",
			"GLFW/src/win32_time.c",
			"GLFW/src/win32_thread.c",
			"GLFW/src/win32_window.c",
			"GLFW/src/wgl_context.c",
			"GLFW/src/egl_context.c",
			"GLFW/src/osmesa_context.c"
        }
    else
        return {}
    end
end

GLFW:SetupProject()