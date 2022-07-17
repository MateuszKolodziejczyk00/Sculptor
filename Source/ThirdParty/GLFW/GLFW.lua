GLFW = Project:CreateProject("GLFW", ETargetType.StaticLibrary, ELanguage.C)

function GLFW:SetupConfiguration(configuration, platform)
    if platform == EPlatform.Windows then
        self:AddPrivateDefine("_GLFW_WIN32")
        self:AddPrivateDefine("_CRT_SECURE_NO_WARNINGS")

        self:AddPrivateDependency("Dwmapi.lib")

        self:AddPublicRelativeIncludePath("/include")
    end
end

function GLFW:GetProjectFiles(configuration, platform)
    if platform == EPlatform.Windows then
        return
        {
            "GLFW/include/GLFW/glfw3.h",
            "GLFW/include/GLFW/glfw3native.h",
            "GLFW/src/internal.h",
            "GLFW/src/platform.h",
            "GLFW/src/mappings.h",
            "GLFW/src/context.c",
            "GLFW/src/init.c",
            "GLFW/src/input.c",
            "GLFW/src/monitor.c",
            "GLFW/src/platform.c",
            "GLFW/src/vulkan.c",
            "GLFW/src/window.c",
            "GLFW/src/egl_context.c",
            "GLFW/src/osmesa_context.c",
            "GLFW/src/null_platform.h",
            "GLFW/src/null_joystick.h",
            "GLFW/src/null_init.c",

            "GLFW/src/null_monitor.c",
            "GLFW/src/null_window.c",
            "GLFW/src/null_joystick.c",

            "GLFW/src/win32_init.c",
            "GLFW/src/win32_module.c",
            "GLFW/src/win32_joystick.c",
            "GLFW/src/win32_monitor.c",
            "GLFW/src/win32_time.h",
            "GLFW/src/win32_time.c",
            "GLFW/src/win32_thread.h",
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