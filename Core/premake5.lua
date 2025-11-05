project "Core"
	kind "StaticLib"
	language "C++"
	cppdialect "C++latest"
	staticruntime "on"
	
	targetdir ("%{wks.location}/bin/" .. outputdir .. "/%{prj.name}")
	objdir ("%{wks.location}/bin-int/" .. outputdir .. "/%{prj.name}")
	
	pchheader "CNMpch.hpp"
	pchsource "src/CNMpch.cpp"
	
	files
	{
		"src/**.hpp",
		"src/**.cpp",
		"src/**.h",
		"src/**.c",
	}
	
	includedirs
	{
		"src"
	}
	
	links
	{
	}
	
--------------------------- PLATFORMS --------------------------------
	filter "system:windows"
		systemversion "latest"
		defines
		{
			"CL_Platform_Windows"
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