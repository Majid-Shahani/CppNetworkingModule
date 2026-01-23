project "Core"
	kind "StaticLib"
	language "C++"
	cppdialect "C++23"
	staticruntime "on"
	
	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")
	
	pchheader "src/CNMpch.hpp"
	pchsource "src/CNMpch.cpp"
	
	files
	{
		"include/**.hpp",
		"include/**.cpp",
		"include/**.h",
		"include/**.c",
		"src/**.hpp",
		"src/**.cpp",
		"src/**.h",
		"src/**.c",
	}
	
	includedirs
	{
		"%{wks.location}/%{prj.name}/include",
		"%{wks.location}/%{prj.name}/",
	}
	
	links
	{
	}
	
--------------------------- ARCHITECTURE -----------------------------
	filter "architecture:x86_64"
		defines "CL_X64"
	filter "architecture:ARM64"
		defines "CL_ARM64"
--------------------------- PLATFORMS --------------------------------
	filter "system:windows"
		systemversion "latest"
		defines
		{
			"CL_Platform_Windows"
		}
		links
		{
			"winmm",
		}
	filter "system:linux"
		defines
		{
			"CL_Platform_Linux"
		}
	filter "system:macosx"
		defines
		{
			"CL_Platform_Mac"
		}
--------------------------------- CONFIGS -------------------------
	filter "configurations:Debug"
		defines 
		{
			"CL_DEBUG",
			"CL_ENABLE_ASSERTS",
		}
		symbols "Full"
		runtime "Debug"
		optimize "Off"
		links
		{
		}
			
	filter "configurations:Release"
		defines "CL_RELEASE"
		runtime "Release"
		optimize "On"
		symbols "Off"
		links
		{
		}
			
	filter "configurations:Dist"
		defines "CL_DIST"
		runtime "Release"
		optimize "Full"
		symbols "Off"
		links
		{
		}