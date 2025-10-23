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
		"src/**.cpp"
	}
	
	includedirs
	{
		"src"
	}
	
	links
	{
	}
	

	filter "system:windows"
		systemversion "latest"
		defines
		{
			"CNM_Platform_Windows"
		}
		includedirs
		{
		}
		links
		{
		}

--------------------------------- CONFIGS -------------------------
	filter "configurations:Debug"
		defines 
		{
		}
		symbols "Full"
		runtime "Debug"
		optimize "Off"
		links
		{
		}
			
	filter "configurations:Release"
		runtime "Release"
		optimize "On"
		symbols "Off"
		links
		{
		}
			
	filter "configurations:Dist"
		runtime "Release"
		optimize "Full"
		symbols "Off"
		links
		{
		}